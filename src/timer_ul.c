/**
 * @file timer_ul.c
 * @brief Portable Scheduler Library (libpsched)
 *        A userland implementation of the timer_*() calls
 *
 * Date: 10-03-2015
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "timer_ul.h"
#include "mm.h"

/* Globals */
static struct timer_ul *_timers = NULL;
static size_t _nr_timers = 0;
static pthread_mutex_t _mutex_timers = PTHREAD_MUTEX_INITIALIZER;

/* API */
int timer_create_ul(clockid_t clockid, struct sigevent *sevp, timer_t *timerid) {
	int i = 0, errsv = 0, slot = -1;

	/* Acquire timers critical region lock */
	pthread_mutex_lock(&_mutex_timers);

	/* Check if there's a free control slot */
	for (i = 0; _timers && (i < _nr_timers); i ++) {
		if (!_timers[i].id) {
			slot = i;
			break;
		}
	}

	/* Check if we've found a free control slot */
	if (slot == -1) {
		/* No free slots... allocate a new one */
		if (!(_timers = mm_realloc(_timers, _nr_timers + 1))) {
			pthread_mutex_unlock(&_mutex_timers);

			/* Everything will be screwed from now on... */
			abort();
		}

		/* Update our slot index */
		slot = _nr_timers;

		/* Update number of allocated timer control slots */
		_nr_timers ++;
	}

	/* Reset timer memory */
	memset(&_timers[slot], 0, sizeof(struct timer_ul));

	/* Validate clockid value */
	switch (clockid) {
		case CLOCK_REALTIME: {
		} break;
		case CLOCK_MONOTONIC: {
		} break;
		case CLOCK_PROCESS_CPUTIME_ID: {
		} break;
		case CLOCK_THREAD_CPUTIME_ID: {
		} break;
		default: {
			errsv = EINVAL;
			goto _create_failure;
		}
	}

	/* Set the clock id */
	_timers[slot].clockid = clockid;

	/* Validate sigevent */
	if (!sevp) {
		errsv = EINVAL;
		goto _create_failure;
	}

	/* Store sigevent data */
	memcpy(&_timers[slot].sevp, sevp, sizeof(struct sigevent));

	/* Grant that ID is never 0 */
	*timerid = _timers[slot].id = (timer_t) (uintptr_t) (slot + 1); /* Grant that ID is never 0 */

	/* Release timers critical region lock */
	pthread_mutex_unlock(&_mutex_timers);

	/* All good */
	return 0;

_create_failure:
	memset(&_timers[slot], 0, sizeof(struct timer_ul));

	pthread_mutex_unlock(&_mutex_timers);

	errno = errsv;

	return -1;
}

int timer_delete_ul(timer_t timerid) {
	errno = ENOSYS;
	return -1;
}

int timer_settime_ul(
	timer_t timerid,
	int flags,
	const struct itimerspec *new_value,
	struct itimerspec *old_value)
{
	errno = ENOSYS;
	return -1;
}

int timer_gettime_ul(timer_t timerid, struct itimerspec *curr_value) {
	errno = ENOSYS;
	return -1;
}

int timer_getoverrun_ul(timer_t timerid) {
	errno = ENOSYS;
	return -1;
}


