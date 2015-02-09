/**
 * @file thread.c
 * @brief Portable Scheduler Library (libpsched)
 *        Threading interface
 *
 * Date: 09-02-2015
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

#include <signal.h>
#include <pthread.h>

#include "sched.h"
#include "event.h"
#include "mm.h"

int thread_init(psched_t *handler) {
	if (pthread_mutex_init(&handler->event_mutex, NULL))
		return -1;

	if (pthread_cond_init(&handler->event_cond, NULL))
		return -1;

	return 0;
}

void thread_destroy(psched_t *handler) {
	pthread_mutex_destroy(&handler->event_mutex);
	pthread_cond_destroy(&handler->event_cond);
}

void thread_handler(union sigval sv) {
	psched_t *handler = sv.sival_ptr;

	event_process(handler);

	/* Check if handler is set to be destroyed */
	if (!handler->armed && handler->destroy)
		mm_free(handler);
}

