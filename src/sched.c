/**
 * @file sched.c
 * @brief Portable Scheduler Library (libpsched)
 *        Scheduler interface
 *
 * Date: 28-05-2014
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

#include <pall/cll.h>

#include "mm.h"
#include "sched.h"
#include "sig.h"

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


/* Core */
psched_t *psched_sig_init(int sig) {
	psched_t *handler = NULL;
	struct sigevent sevp;

	memset(&sevp, 0, sizeof(struct sigevent));

	if (!(handler = mm_alloc(sizeof(psched_t))))
		return NULL;

	memset(handler, 0, sizeof(psched_t));

	if (!(handler->s = pall_cll_init(&_cll_compare, &_cll_destroy, NULL, NULL))) {
		mm_free(handler);

		return NULL;
	}

	handler->s->set_config(handler->s, CONFIG_SEARCH_AUTO | CONFIG_INSERT_HEAD);

	sevp.sigev_notify = SIGEV_SIGNAL;
	sevp.sigev_signo = sig;
	sevp.sigev_value.sival_ptr = handler;

	if (timer_create(CLOCK_REALTIME, &sevp, &handler->timer) < 0) {
		pall_cll_destroy(handler->s);
		mm_free(handler);

		return NULL;
	}

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

	return handler;
}

int psched_sig_destroy(psched_t *handler) {
	if (sigaction(handler->sig, &handler->sa_old, NULL) < 0)
		return -1;

	if (timer_delete(handler->timer) < 0)
		return -1;

	pall_cll_destroy(handler->s);
	mm_free(handler);

	return 0;
}

pschedid_t psched_timestamp_arm(
		psched_t *handler,
		time_t trigger,
		time_t step,
		void *(*routine) (void *),
		void *arg)
{
	struct timespec ts_trigger, ts_step;

	ts_trigger.tv_sec = trigger;
	ts_trigger.tv_nsec = 0;

	ts_step.tv_sec = step;
	ts_step.tv_nsec = 0;

	return psched_timespec_arm(handler, &ts_trigger, &ts_step, routine, arg);
}

pschedid_t psched_timespec_arm(
		psched_t *handler,
		struct timespec *trigger,
		struct timespec *step,
		void *(*routine) (void *),
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

	memcpy(&entry->trigger, trigger, sizeof(struct timespec));

	if (step) 
		memcpy(&entry->step, step, sizeof(struct timespec));

	entry->routine = routine;
	entry->arg = arg;

	entry->id = (pschedid_t) (uintptr_t) entry;

	handler->s->insert(handler->s, entry);

	if (psched_update_timers(handler) < 0) {
		handler->s->del(handler->s, entry);

		if (psched_update_timers(handler) < 0)
			abort();

		return -1;
	}

	return entry->id;
}

int psched_disarm(psched_t *handler, pschedid_t id) {
	struct psched_entry *entry = NULL;

	if (!(entry = handler->s->search(handler->s, psched_val(id)))) {
		errno = EINVAL;
		return -1;
	}

	if (entry != handler->armed) {
		handler->s->del(handler->s, entry);

		return 0;
	}

	handler->armed = NULL;

	return psched_update_timers(handler);
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
		} else if (entry->trigger.tv_sec > handler->armed->trigger.tv_sec) {
			handler->armed = entry;
		} else if (entry->trigger.tv_sec == handler->armed->trigger.tv_sec) {
			if (entry->trigger.tv_nsec > handler->armed->trigger.tv_nsec)
				handler->armed = entry;
		}
	}

	if (!handler->armed)
		return 0;

	its.it_value.tv_sec = handler->armed->trigger.tv_sec;
	its.it_value.tv_nsec = handler->armed->trigger.tv_nsec;
	its.it_interval.tv_sec = handler->armed->step.tv_sec;
	its.it_interval.tv_nsec = handler->armed->step.tv_nsec;

	if (timer_settime(handler->timer, TIMER_ABSTIME, &its, NULL) < 0)
		return -1;

	return 0;
}


