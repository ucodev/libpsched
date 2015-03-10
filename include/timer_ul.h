/**
 * @file timer_ul.h
 * @brief Portable Scheduler Library (libpsched)
 *        Header file for the userland implementation of the timer_*() calls
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

#ifndef LIBPSCHED_TIMER_UL_H
#define LIBPSCHED_TIMER_UL_H

#include <signal.h>
#include <time.h>
#include <pthread.h>


#define PSCHED_TIMER_UL_THREAD_ARMED_FLAG	0x01	/* Timer is armed */
#define PSCHED_TIMER_UL_THREAD_INTR_FLAG	0x02	/* Timer must be interrupted */
#define PSCHED_TIMER_UL_THREAD_READ_FLAG	0x04	/* A read op on timer is required */
#define PSCHED_TIMER_UL_THREAD_WRITE_FLAG	0x08	/* A write op on timer is required */

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

