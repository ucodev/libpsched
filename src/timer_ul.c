/**
 * @file timer_ul.c
 * @brief Portable Scheduler Library (libpsched)
 *        A userland implementation of the timer_*() calls
 *
 * Date: 28-03-2015
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

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include "timer_ul.h"
#include "mm.h"
#include "timespec.h"

/* Globals */
static struct timer_ul *_timers = NULL;
static size_t _nr_timers = 0;
static pthread_mutex_t _mutex_timers = PTHREAD_MUTEX_INITIALIZER;

static void *_notify_routine(void *arg) {
	struct sigevent *sevp = arg;

	sevp->sigev_notify_function(sevp->sigev_value);

	pthread_exit(NULL);

	return NULL;
}

static int _fd_set_nonblock(int fd) {
#ifdef COMPILE_WIN32
	return 0;
#else
	int flags = 0;

	if ((flags = fcntl(fd, F_GETFL)) < 0)
		return -1;

	if (!(flags & O_NONBLOCK)) {
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
			return -1;
	}

	return 0;
#endif
}

static void *_timer_process(void *arg) {
	struct timer_ul *timer = arg;
	struct timespec tcur, tsleep, tv_start, tv_stop, tv_delta;
	struct timeval wait_val;
	pthread_t t_notify;

	/* Acquire the lock */
	pthread_mutex_lock(&timer->t_mutex);

	/* Create the pipe */
	if (pipe(timer->wait_pipe) < 0)
		abort(); /* Out of options */

	/* Set the read end of the pipe as non-block */
	_fd_set_nonblock(timer->wait_pipe[0]);

	/* Mark the timer as ARMED */
	timer->t_flags |= PSCHED_TIMER_UL_THREAD_ARMED_FLAG;

	/* Inform that this timer is now armed. NOTE: The mutex release is present before select() */
	pthread_cond_signal(&timer->t_cond);

	/* It's imperative that timer->rem be set to zero before the first iteration of the loop */
	memset(&timer->rem, 0, sizeof(struct timespec));

	for (;;) {
		if ((timer->rem.tv_sec <= 0) && (timer->rem.tv_nsec <= 0)) {
			memset(&timer->rem, 0, sizeof(struct timespec));

			/* Process timer based on flags */
			if (timer->flags & TIMER_ABSTIME) {
				/* If absolute ... */
				memcpy(&tsleep, &timer->arm.it_value, sizeof(struct timespec));

				/* If we can't retrieve current time, there's no point in
				 * continuing
				 */
				/* TODO: use some alternatives to clock_gettime() if it fails. */
				if (clock_gettime(timer->clockid, &tcur) < 0)
					abort();

				/* Subtract the current time to the absolute value of the timer */
				timespec_sub(&tsleep, &tcur);
			} else {
				/* If relative ... */
				memcpy(&tsleep, &timer->arm.it_value, sizeof(struct timespec));
			}
		} else {
			/* If there's time left to wait ... */
			memcpy(&tsleep, &timer->rem, sizeof(struct timespec));
		}

		/* Prepare parameters for select() */
		FD_ZERO(&timer->wait_set);
		FD_SET(timer->wait_pipe[0], &timer->wait_set);

		wait_val.tv_sec = tsleep.tv_sec;
		wait_val.tv_usec = tsleep.tv_nsec / 1000;

		/* Release the timer mutex while the timer is blocking */
		pthread_mutex_unlock(&timer->t_mutex);

		/* Snapshot the current time */
		clock_gettime(CLOCK_REALTIME, &tv_start); /* TODO: Alternatives? */

		/* Wait for timeout or some event on the pipe */
		if (select(timer->wait_pipe[0] + 1, &timer->wait_set, NULL, NULL, &wait_val) < 0) {
			/* 'wait_set' is unreliable from now on ... */
			;
		}

		/* Take another snapshot of the current time so we can calculate the variation */
		clock_gettime(CLOCK_REALTIME, &tv_stop); /* TODO: Alternatives? */

		/* Re-acquire the timer mutex */
		pthread_mutex_lock(&timer->t_mutex);

		/* If something was written on the pipe, an interrupt ocurred */
		if (FD_ISSET(timer->wait_pipe[0], &timer->wait_set)) {
			/* On interrupt, we need to calculate how much time is left */
			memcpy(&timer->rem, &tsleep, sizeof(struct timespec));
			memcpy(&tv_delta, &tv_stop, sizeof(struct timespec));
			timespec_sub(&tv_delta, &tv_start);
			timespec_sub(&timer->rem, &tv_delta);

			/* pipe is O_NONBLOCK as the 'wait_set' may be unreliable */
			read(timer->wait_pipe[0], (char [1]) { 0 }, 1);
		} else {
			/* On timeout return of select(), reset the rem as there's
			 * nothing to compensate
			 */
			memset(&timer->rem, 0, sizeof(struct timespec));
		}

		/* Interrupt flag means urgent stop of this thread */
		if (timer->t_flags & PSCHED_TIMER_UL_THREAD_INTR_FLAG) {
			/* Let this thread die */
			break;
		}

		/* If a read operation was performed, signal the reader that it can now read the
		 * updated values whenever possible.
		 */
		if (timer->t_flags & PSCHED_TIMER_UL_THREAD_READ_FLAG) {
			timer->t_flags &= ~PSCHED_TIMER_UL_THREAD_READ_FLAG;
			pthread_cond_signal(&timer->t_cond);

			continue;
		}

		/* Invoke notification */
		switch (timer->sevp.sigev_notify) {
			case SIGEV_THREAD: {
				pthread_create(&t_notify, NULL, &_notify_routine, &timer->sevp);
				pthread_detach(t_notify);
			} break;
			case SIGEV_SIGNAL: {
				/* TODO: Not yet implemented */
				abort();
			} break;
			case SIGEV_NONE: {
			} break;
			default: {
				/* Something went wrong... we don't recognize this state */
				abort();
			}
		}

		/* TODO: Check if overrun occurred. */

		/* Add the it_interval to it_value, or break the loop if no interval is set */
		if (!timer->arm.it_interval.tv_sec && !timer->arm.it_interval.tv_nsec)
			break;

		/* Update timer value */
		if (timer->flags & TIMER_ABSTIME) {
			/* If absolute, add the interval to the current timer value */
			timespec_add(&timer->arm.it_value, &timer->arm.it_interval);
		} else {
			/* If relative, the interval value is the new timer value */
			memcpy(&timer->arm.it_value, &timer->arm.it_interval, sizeof(struct timespec));
		}
	}

	/* Update timer state */
	timer->t_flags |= PSCHED_TIMER_UL_THREAD_WAIT_FLAG;	/* Waiting for pthread_join() */
	timer->t_flags &= ~PSCHED_TIMER_UL_THREAD_ARMED_FLAG;	/* Timer no longer armed */

	/* Release the timer mutex */
	pthread_mutex_unlock(&timer->t_mutex);

	/* All good */
	pthread_exit(NULL);

	return NULL;
}

/* API */
int timer_create_ul(clockid_t clockid, struct sigevent *sevp, timer_t *timerid) {
	int i = 0, errsv = 0, slot = -1;

	/* Acquire timers critical region lock */
	pthread_mutex_lock(&_mutex_timers);

	/* Check if there's a free control slot */
	for (i = 0; _timers && (i < _nr_timers); i ++) {
		if (!_timers[i].id) {
			/* Assign the slot */
			slot = i;
			break;
		} else if (_timers[i].t_flags & PSCHED_TIMER_UL_THREAD_WAIT_FLAG) {
			/* Thread is waiting to be joined */
			pthread_join(_timers[i].t_id, NULL);

			/* Assign the slot */
			slot = i;
			break;
		}
	}

	/* Check if we've found a free control slot */
	if (slot == -1) {
		/* No free slots... allocate a new one */
		if (!(_timers = mm_realloc(_timers, sizeof(struct timer_ul) * (_nr_timers + 1)))) {
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
#ifndef PSCHED_TIMER_UL_NO_CPUTIME
		case CLOCK_PROCESS_CPUTIME_ID: {
		} break;
		case CLOCK_THREAD_CPUTIME_ID: {
		} break;
#endif
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

	switch (sevp->sigev_notify) {
		case SIGEV_THREAD: {
		} break;
		case SIGEV_NONE: {
		} break;
		case SIGEV_SIGNAL:
		default: {
			errsv = EINVAL;
			goto _create_failure;
		}
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
	int i = 0, used = 0;
	uintptr_t slot = ((uintptr_t) timerid) - 1;
	struct itimerspec disarm = { { 0, 0 }, { 0, 0 } };

	/* Sanity check */
	if (slot >= _nr_timers) {
		errno = EINVAL;
		return -1;
	}

	/* Disarm timer, if armed */
	if (timer_settime_ul(timerid, 0, &disarm, NULL) < 0)
		return -1;

	/* Cleanup timer data */
	memset(&_timers[slot], 0, sizeof(struct timer_ul));

	/* Entering critical region */
	pthread_mutex_lock(&_mutex_timers);

	/* Check if the list is still being used, and peform some cleanup */
	for (i = 0; i < _nr_timers; i ++) {
		if (_timers[i].t_flags & PSCHED_TIMER_UL_THREAD_WAIT_FLAG) {
			pthread_join(_timers[i].t_id, NULL);
			memset(&_timers[i], 0, sizeof(struct timer_ul));
		}

		if (_timers[i].id) {
			used = 1;
			break;
		}
	}

	/* If unused, free all the resources */
	if (!used) {
		_nr_timers = 0;
		mm_free(_timers);
		_timers = NULL;
	}

	/* Leaving critical region */
	pthread_mutex_unlock(&_mutex_timers);

	/* All good */
	return 0;
}

int timer_settime_ul(
	timer_t timerid,
	int flags,
	const struct itimerspec *new_value,
	struct itimerspec *old_value)
{
	int errsv = 0;
	uintptr_t slot = ((uintptr_t) timerid) - 1;

	/* Sanity check */
	if (slot >= _nr_timers) {
		errno = EINVAL;
		return -1;
	}

	if (!new_value) {
		errno = EINVAL;
		return -1;
	}

	/* Acquire timers critical region lock */
	pthread_mutex_lock(&_mutex_timers);

	/* Check if this timer is to be disarmed or changed */
	if (_timers[slot].t_flags & PSCHED_TIMER_UL_THREAD_ARMED_FLAG) {
		/* Interrupt the current timer */
		_timers[slot].t_flags |= PSCHED_TIMER_UL_THREAD_INTR_FLAG;

		write(_timers[slot].wait_pipe[1], (char [1]) { 0 }, 1);

		/* Acquire the target timer thread mutex, granting that a condition wait occurred */
		pthread_mutex_lock(&_timers[slot].t_mutex);

		/* Copy last known value and interval */
		if (old_value)
			memcpy(old_value, &_timers[slot].arm, sizeof(struct itimerspec));

		/* We're done with reads */
		/*_timers[slot].t_flags &= ~PSCHED_TIMER_UL_THREAD_INTR_FLAG; */

		/* Release the target timer thread mutex */
		pthread_mutex_unlock(&_timers[slot].t_mutex);

		/* Wait for thread termination */
		pthread_join(_timers[slot].t_id, NULL);

		/* Destroy mutexes */
		pthread_cond_destroy(&_timers[slot].t_cond);
		pthread_mutex_destroy(&_timers[slot].t_mutex);

		/* Close pipe */
		close(_timers[slot].wait_pipe[0]);
		close(_timers[slot].wait_pipe[1]);

		/* Cleanup timer entry slot */
		memset(&_timers[slot].init_time, 0, sizeof(struct timespec));
		memset(&_timers[slot].rem, 0, sizeof(struct timespec));
		memset(&_timers[slot].arm, 0, sizeof(struct timespec));
		_timers[slot].overruns = 0;
		memset(&_timers[slot].t_id, 0, sizeof(pthread_t));
		memset(&_timers[slot].t_cond, 0, sizeof(pthread_cond_t));
		memset(&_timers[slot].t_mutex, 0, sizeof(pthread_mutex_t));
		_timers[slot].t_flags = 0;
		_timers[slot].wait_pipe[0] = 0;
		_timers[slot].wait_pipe[1] = 0;
	}

	/* If it this is a disarm operation ... */
	if (!new_value->it_value.tv_sec && !new_value->it_value.tv_nsec) {
		pthread_mutex_unlock(&_mutex_timers);

		return 0;
	}

	/* Set the init time */
	if (clock_gettime(_timers[slot].clockid, &_timers[slot].init_time) < 0) {
		errsv = errno;
		goto _settime_failure;
	}

	/* Set flags */
	_timers[slot].flags = flags;

	/* Copy the timer values */
	memcpy(&_timers[slot].arm, new_value, sizeof(struct itimerspec));

	/* Initialize thread cond and mutex */
	pthread_mutex_init(&_timers[slot].t_mutex, NULL);
	pthread_cond_init(&_timers[slot].t_cond, NULL);

	/* Timer mutex shall be acquired before thread is created */
	pthread_mutex_lock(&_timers[slot].t_mutex);

	/* Create a thread to process this timer */
	if (pthread_create(&_timers[slot].t_id, NULL, &_timer_process, &_timers[slot])) {
		errsv = errno;
		goto _settime_failure;
	}

	/* Wait for the newly created timer inform that it is armed */
	while (!(_timers[slot].t_flags & PSCHED_TIMER_UL_THREAD_ARMED_FLAG))
		pthread_cond_wait(&_timers[slot].t_cond, &_timers[slot].t_mutex);

	/* Release the timer mutex */
	pthread_mutex_unlock(&_timers[slot].t_mutex);

	/* Release timers critical region lock */
	pthread_mutex_unlock(&_mutex_timers);

	/* All good */
	return 0;

_settime_failure:
	/* Release timers critical region lock */
	pthread_mutex_unlock(&_mutex_timers);

	errno = errsv;

	return -1;
}

int timer_gettime_ul(timer_t timerid, struct itimerspec *curr_value) {
	uintptr_t slot = ((uintptr_t) timerid) - 1;

	/* Sanity check */
	if (slot >= _nr_timers) {
		errno = EINVAL;
		return -1;
	}

	if (!curr_value) {
		errno = EFAULT;
		return -1;
	}

	/* Acquire timers critical region lock */
	pthread_mutex_lock(&_mutex_timers);

	if (!(_timers[slot].t_flags & PSCHED_TIMER_UL_THREAD_ARMED_FLAG)) {
		/* If the timer isn't armed, return error */
		errno = EINVAL;
		return -1;
	}

	/* Acquire the target timer thread mutex */
	pthread_mutex_lock(&_timers[slot].t_mutex);

	/* This is a read operation */
	_timers[slot].t_flags |= PSCHED_TIMER_UL_THREAD_READ_FLAG;

	/* Interrupt thread if it's sleeping */
	write(_timers[slot].wait_pipe[1], (char [1]) { 0 }, 1);

	/* Wait until the timer updates data */
	while (_timers[slot].t_flags & PSCHED_TIMER_UL_THREAD_READ_FLAG)
		pthread_cond_wait(&_timers[slot].t_cond, &_timers[slot].t_mutex);

	/* Populate the curr_value */
	memcpy(&curr_value->it_interval, &_timers[slot].arm.it_interval, sizeof(struct timespec));
	memcpy(&curr_value->it_value, &_timers[slot].rem, sizeof(struct timespec));

	/* Release the target timer thread mutex */
	pthread_mutex_unlock(&_timers[slot].t_mutex);

	/* Release timers critical region lock */
	pthread_mutex_unlock(&_mutex_timers);

	return 0;
}

int timer_getoverrun_ul(timer_t timerid) {
	uintptr_t slot = ((uintptr_t) timerid) - 1;

	/* Sanity check */
	if (slot >= _nr_timers) {
		errno = EINVAL;
		return -1;
	}

	return _timers[slot].overruns;
}


