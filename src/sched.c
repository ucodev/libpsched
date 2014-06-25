/**
 * @file sched.c
 * @brief Portable Scheduler Library (libpsched)
 *        Scheduler interface
 *
 * Date: 25-06-2014
 * 
 * Copyright 2014 Pedro A. Hortas (pah@ucodev.org)
 *
 * This file is part of libpsched.
 *
 * libpsched is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libpsched is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpsched.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include <pall/cll.h>

#include "mm.h"
#include "sched.h"
#include "sig.h"
#include "thread.h"

/* Statics */
static int _cll_compare(const void *d1, const void *d2) {
	const struct psched_entry *pd1 = (struct psched_entry *) d1, *pd2 = (struct psched_entry *) d2;

	if (pd1->id > pd2->id)
		return 1;

	if (pd1->id < pd2->id)
		return -1;

	return 0;
}

static void _cll_destroy(void *data) {
	mm_free(data);
}

static psched_t *_init(int sig, int threaded) {
	psched_t *handler = NULL;
	struct sigevent sevp;

	memset(&sevp, 0, sizeof(struct sigevent));

	if (!(handler = mm_alloc(sizeof(psched_t))))
		return NULL;

	memset(handler, 0, sizeof(psched_t));

	if (threaded) {
		if (thread_init(handler) < 0) {
			mm_free(handler);

			return NULL;
		}

		handler->threaded = 1;
	}

	if (!(handler->s = pall_cll_init(&_cll_compare, &_cll_destroy, NULL, NULL))) {
		mm_free(handler);

		return NULL;
	}

	handler->s->set_config(handler->s, CONFIG_SEARCH_AUTO | CONFIG_INSERT_HEAD);

	sevp.sigev_value.sival_ptr = handler;

	if (threaded) {
		sevp.sigev_notify = SIGEV_THREAD;
		sevp.sigev_notify_function = &thread_handler;
	} else {
		sevp.sigev_notify = SIGEV_SIGNAL;
		sevp.sigev_signo = sig;
	}

	if (timer_create(CLOCK_REALTIME, &sevp, &handler->timer) < 0) {
		pall_cll_destroy(handler->s);
		mm_free(handler);

		return NULL;
	}

	if (!threaded) {
		handler->sig = sig;
		handler->sa.sa_flags = SA_SIGINFO;
		handler->sa.sa_sigaction = &sig_handler;
		sigemptyset(&handler->sa.sa_mask);

		if (sigaction(sig, &handler->sa, &handler->sa_old) < 0) {
			timer_delete(handler->timer);
			pall_cll_destroy(handler->s);
			mm_free(handler);

			return NULL;
		}
	}

	return handler;
}

/* Core */
psched_t *psched_thread_init(void) {
	return _init(0, 1);
}

psched_t *psched_sig_init(int sig) {
	return _init(sig, 0);
}

int psched_destroy(psched_t *handler) {
	if (!handler->threaded) {
		if (sigaction(handler->sig, &handler->sa_old, NULL) < 0)
			return -1;
	}

	if (timer_delete(handler->timer) < 0)
		return -1;

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	pall_cll_destroy(handler->s);

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	/* Set this handler to be destroyed by event handling function when execution queue is empty */
	handler->destroy = 1;

	return 0;
}

pschedid_t psched_timestamp_arm(
		psched_t *handler,
		time_t trigger,
		time_t step,
		time_t expire,
		void (*routine) (void *),
		void *arg)
{
	struct timespec ts_trigger, ts_step, ts_expire;

	ts_trigger.tv_sec = trigger;
	ts_trigger.tv_nsec = 0;

	ts_step.tv_sec = step;
	ts_step.tv_nsec = 0;

	ts_expire.tv_sec = expire;
	ts_expire.tv_nsec = 0;

	return psched_timespec_arm(handler, &ts_trigger, &ts_step, &ts_expire, routine, arg);
}

pschedid_t psched_timespec_arm(
		psched_t *handler,
		struct timespec *trigger,
		struct timespec *step,
		struct timespec *expire,
		void (*routine) (void *),
		void *arg)
{
	struct psched_entry *entry = NULL;

	if (!trigger) {
		errno = EINVAL;
		return (pschedid_t) -1;
	}

	if (!routine) {
		errno = EINVAL;
		return (pschedid_t) -1;
	}

	if (!(entry = mm_alloc(sizeof(struct psched_entry))))
		return (pschedid_t) -1;

	memset(entry, 0, sizeof(struct psched_entry));

	memcpy(&entry->trigger, trigger, sizeof(struct timespec));

	if (step) 
		memcpy(&entry->step, step, sizeof(struct timespec));

	if (expire)
		memcpy(&entry->expire, expire, sizeof(struct timespec));

	entry->routine = routine;
	entry->arg = arg;

	entry->id = (pschedid_t) (uintptr_t) entry;

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	handler->s->insert(handler->s, entry);

	if (psched_update_timers(handler) < 0) {
		handler->s->del(handler->s, entry);

		if (psched_update_timers(handler) < 0)
			abort();

		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return -1;
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return entry->id;
}

int psched_disarm(psched_t *handler, pschedid_t id) {
	int ret = 0;
	struct psched_entry *entry = NULL;

	if (!(entry = handler->s->search(handler->s, psched_val(id)))) {
		errno = EINVAL;
		return -1;
	}

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	if (entry != handler->armed) {
		handler->s->del(handler->s, entry);

		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return 0;
	}

	handler->armed = NULL;

	ret = psched_update_timers(handler);

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return ret;
}

int psched_search(psched_t *handler, pschedid_t id, struct psched_entry **entry) {
	int ret = -1;

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	for (handler->s->rewind(handler->s, 0); (*entry = handler->s->iterate(handler->s)); ) {
		if ((*entry)->id == id) {
			ret = 0;
			break;
		}
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return -1;
}

int psched_update_timers(psched_t *handler) {
	struct itimerspec its;
	struct psched_entry *entry = NULL;

	memset(&its, 0, sizeof(struct itimerspec));

	if (handler->armed) {
		if (timer_settime(handler->timer, TIMER_ABSTIME, &its, NULL) < 0)
			return -1;

		handler->armed = NULL;
	}

	for (handler->s->rewind(handler->s, 0); (entry = handler->s->iterate(handler->s)); ) {
		if (!handler->armed) {
			handler->armed = entry;
		} else if (entry->trigger.tv_sec < handler->armed->trigger.tv_sec) {
			handler->armed = entry;
		} else if (entry->trigger.tv_sec == handler->armed->trigger.tv_sec) {
			if (entry->trigger.tv_nsec < handler->armed->trigger.tv_nsec)
				handler->armed = entry;
		}
	}

	/* Validate if there's at least one timer to be armed */
	if (!handler->armed)
		return 0;

	its.it_value.tv_sec = handler->armed->trigger.tv_sec;
	its.it_value.tv_nsec = handler->armed->trigger.tv_nsec;

	if (timer_settime(handler->timer, TIMER_ABSTIME, &its, NULL) < 0)
		return -1;

	return 0;
}


