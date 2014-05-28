/**
 * @file sig.c
 * @brief Portable Scheduler Library (libpsched)
 *        Signals interface
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
#include <signal.h>
#include <time.h>

#include "sched.h"

void sig_handler(int sig, siginfo_t *si, void *context) {
	psched_t *handler = si->si_value.sival_ptr;
	struct timespec tp_now;

	if (clock_gettime(CLOCK_REALTIME, &tp_now) < 0) {
		/* TODO: implement alternatives: gettimeofday(), time(), etc. */
		abort();
	}

	if (handler->armed) {
		if ((tp_now.tv_sec >= handler->armed->trigger.tv_sec) && (tp_now.tv_nsec >= handler->armed->trigger.tv_nsec)) {
			handler->armed->routine(handler->armed->arg);
		}
	}

	handler->s->del(handler->s, handler->armed);

	handler->armed = NULL;

	if (psched_update_timers(handler) < 0)
		abort();
}

