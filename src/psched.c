/**
 * @file psched.c
 * @brief Portable Scheduler Library (libpsched)
 *        Scheduler interface
 *
 * Date: 13-03-2015
 * 
 * Copyright 2014-2015 Pedro A. Hortas (pah@ucodev.org)
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
#include "psched.h"
#include "sig.h"
#include "thread.h"
#include "timer_ul.h"

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

static unsigned int _count_events_in_progress(struct cll_handler *s) {
	const struct psched_entry *entry = NULL;
	unsigned int count = 0;

	for (s->rewind(s, 0); (entry = s->iterate(s)); )
		count += (entry->in_progress == 1);

	return count;
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
	}
#ifndef PSCHED_NO_SIG
	 else {
		sevp.sigev_notify = SIGEV_SIGNAL;
		sevp.sigev_signo = sig;
	}
#endif

	if (timer_create(CLOCK_REALTIME, &sevp, &handler->timer) < 0) {
		pall_cll_destroy(handler->s);
		mm_free(handler);

		return NULL;
	}

#ifndef PSCHED_NO_SIG
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
#endif

	return handler;
}

/* Core */
psched_t *psched_thread_init(void) {
	return _init(0, 1);
}

psched_t *psched_sig_init(int sig) {
#ifdef PSCHED_NO_SIG
	errno = ENOSYS;
	return -1;
#else
	return _init(sig, 0);
#endif
}

int psched_fatal(psched_t *handler) {
	return handler->fatal;
}

int psched_destroy(psched_t *handler) {
	if (!handler->threaded) {
		if (sigaction(handler->sig, &handler->sa_old, NULL) < 0)
			return -1;
	}

	/* Return error only if no fatal state is currently set. Otherwise (on fatal state) continue
	 * cleaning the psched data.
	 */
	if ((timer_delete(handler->timer) < 0) && !handler->fatal)
		return -1;

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	/* Set this handler to be destroyed by event handling function when execution queue is empty */
	handler->destroy = 1;

	/* Wait for any entries that are in progress to complete, before destroying the
	 * scheduling queue.
	 */
	for (;;) {
		if (!_count_events_in_progress(handler->s))
			break;

		if (handler->threaded) pthread_cond_wait(&handler->event_cond, &handler->event_mutex);
	}

	/* Destroy the scheduling queue */
	pall_cll_destroy(handler->s);

	/* No entry is or will be armed from this point on ... */
	handler->armed = NULL;

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return 0;
}

void psched_handler_destroy(psched_t *handler) {
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	/* Wait for any armed entry to be disarmed */
	for (;;) {
		if (!handler->armed)
			break;

		if (handler->threaded) pthread_cond_wait(&handler->event_cond, &handler->event_mutex);
	}

	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	/* Destroy the threading interface */
	thread_destroy(handler);

	/* Free handler memory */
	mm_free(handler);
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

	/* Check if a fatal error occurred */
	if (handler->fatal) {
		errno = ECANCELED; /* A clean restart of the library is required */
		return -1;
	}

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

		if (psched_update_timers(handler) < 0) {
			handler->fatal = 1;
			abort();
		}

		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		errno = ECANCELED;

		return -1;
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return entry->id;
}

int psched_disarm(psched_t *handler, pschedid_t id) {
	int ret = 0;
	struct itimerspec its;
	struct psched_entry *entry = NULL;

	/* Check if a fatal error occurred */
	if (handler->fatal) {
		errno = ECANCELED; /* A clean restart of the library is required */
		return -1;
	}

	memset(&its, 0, sizeof(struct itimerspec));

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	/* Search for scheduling entry */
	if (!(entry = handler->s->search(handler->s, psched_val(id)))) {
		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		errno = EINVAL;
		return -1;
	}

	/* Check if the found entry is currently armed */
	if (entry != handler->armed) {
		handler->s->del(handler->s, entry);

		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return 0;
	}

	/* NOTE: If the entry to be delete is armed, we need to disarm the timer and reset the handler->armed pointer
	 * before we can delete it from the handlers list.
	 */

	/* Disarm timer */
	if (timer_settime(handler->timer, TIMER_ABSTIME, &its, NULL) < 0) {
		/* Unlock event mutex */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return -1;
	}

	handler->armed = NULL;

	handler->s->del(handler->s, entry);

	ret = psched_update_timers(handler);

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return ret;
}

/**
 * NOTE: This function does not grant that after it returns the entry still exists, unless it is called inside the
 *       notification routine, invoked by timer expiration.
 *
 */
int psched_search(
		psched_t *handler,
		pschedid_t id,
		struct timespec *trigger,
		struct timespec *step,
		struct timespec *expire) {
	struct psched_entry *entry = NULL;
	int ret = -1;

	/* Check if a fatal error occurred */
	if (handler->fatal) {
		errno = ECANCELED; /* A clean restart of the library is required */
		return -1;
	}

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	/* Search for scheduling entry */
	entry = handler->s->search(handler->s, psched_val(id));

	/* If the entry was found, update trigger, step and expire arguments */
	if (entry && !entry->to_remove) {
		memcpy(trigger, &entry->trigger, sizeof(struct timespec));
		memcpy(step, &entry->step, sizeof(struct timespec));
		memcpy(expire, &entry->expire, sizeof(struct timespec));

		ret = 0;
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	return ret;
}

int psched_update_timers(psched_t *handler) {
	struct itimerspec its;
	struct psched_entry *entry = NULL;

	memset(&its, 0, sizeof(struct itimerspec));

	if (handler->armed) {
		/* Disarm timer */
		if (timer_settime(handler->timer, TIMER_ABSTIME, &its, NULL) < 0)
			return -1;

		handler->armed = NULL;
	}

	for (handler->s->rewind(handler->s, 0); (entry = handler->s->iterate(handler->s)); ) {
		if (entry->in_progress)
			continue;

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


