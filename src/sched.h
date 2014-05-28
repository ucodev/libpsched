/**
 * @file sched.h
 * @brief Portable Scheduler Library (libpsched)
 *        Scheduler interface header
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


#ifndef LIBPSCHED_SCHED_H
#define LIBPSCHED_SCHED_H

#include <stdint.h>
#include <signal.h>
#include <time.h>

#include <pall/cll.h>

#include "mm.h"

typedef uintptr_t pschedid_t;

typedef struct psched_handler {
	timer_t timer;
	int sig;
	struct sigaction sa;
	struct sigaction sa_old;
	struct cll_handler *s;
	struct psched_entry *armed;
} psched_t;

struct psched_entry {
	pschedid_t id;
	struct timespec trigger;
	struct timespec step;
	void *(*routine) (void *);
	void *arg;
};

/* Macros */
#define psched_val(val) ((struct psched_entry [1]) { { val, } })

/* Prototypes */
psched_t *psched_sig_init(int sig);
int psched_sig_destroy(psched_t *handler);
pschedid_t psched_timestamp_arm(
		psched_t *handler,
		time_t trigger,
		time_t step,
		void *(*routine) (void *),
		void *arg);
pschedid_t psched_timespec_arm(
		psched_t *handler,
		struct timespec *trigger,
		struct timespec *step,
		void *(*routine) (void *),
		void *arg);
int psched_disarm(psched_t *handler, pschedid_t id);
int psched_update_timers(psched_t *handler);

#endif
