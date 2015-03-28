/**
 * @file timer_ul.h
 * @brief Portable Scheduler Library (libpsched)
 *        Header file for the userland implementation of the timer_*() calls
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

#ifndef LIBPSCHED_TIMER_UL_H
#define LIBPSCHED_TIMER_UL_H

#include <signal.h>
#include <time.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/types.h>

#ifdef PSCHED_INTERNAL_TIMER_T
typedef void * timer_t;
#endif

#ifdef PSCHED_INTERNAL_SIGVAL
union sigval {
	int sival_int;
	void *sival_ptr;
};
#endif

#ifdef PSCHED_INTERNAL_SIGEVENT
struct sigevent {
	int sigev_notify;
	int sigev_signo;
	union sigval sigev_value;

	void (*sigev_notify_function) (union sigval);

	void *sigev_notify_attributes;
};

#define	SIGEV_NONE	0x01
#define SIGEV_SIGNAL	0x02
#define SIGEV_THREAD	0x04
#endif

#ifdef PSCHED_INTERNAL_TIMER_UL
#define timer_create		timer_create_ul
#define timer_delete		timer_delete_ul
#define timer_settime		timer_settime_ul
#define timer_gettime		timer_gettime_ul
#define timer_getoverrun	timer_getoverrun_ul
#endif

/* Flags for userland timer_*() implementation */
#define PSCHED_TIMER_UL_THREAD_ARMED_FLAG	0x01	/* Timer is armed */
#define PSCHED_TIMER_UL_THREAD_INTR_FLAG	0x02	/* Timer must be interrupted */
#define PSCHED_TIMER_UL_THREAD_READ_FLAG	0x04	/* A read op on timer is required */
#define PSCHED_TIMER_UL_THREAD_WRITE_FLAG	0x08	/* A write op on timer is required */
#define PSCHED_TIMER_UL_THREAD_WAIT_FLAG	0x10	/* Thread exited and waiting to join */

/* Structures */
struct timer_ul {
	timer_t id;
	int flags;
	clockid_t clockid;
	struct sigevent sevp;

	struct timespec init_time;	/* Absolute time when first armed */

	struct timespec rem;
	struct itimerspec arm;

	int overruns;

	/* Thread specific */
	pthread_t t_id;
	pthread_cond_t t_cond;
	pthread_mutex_t t_mutex;
	int t_flags;
	int wait_pipe[2];
	fd_set wait_set;
};


/* Prototypes */
int timer_create_ul(clockid_t clockid, struct sigevent *sevp, timer_t *timerid);
int timer_delete_ul(timer_t timerid);
int timer_settime_ul(
	timer_t timerid,
	int flags,
	const struct itimerspec *new_value,
	struct itimerspec *old_value);
int timer_gettime_ul(timer_t timerid, struct itimerspec *curr_value);
int timer_getoverrun_ul(timer_t timerid);

#endif

