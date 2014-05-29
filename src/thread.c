/**
 * @file thread.c
 * @brief Portable Scheduler Library (libpsched)
 *        Threading interface
 *
 * Date: 29-05-2014
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

#include <signal.h>
#include <pthread.h>

#include "sched.h"
#include "event.h"

int thread_init(psched_t *handler) {
	if (pthread_mutex_init(&handler->event_mutex, NULL))
		return -1;

	return 0;
}

void thread_handler(union sigval sv) {
	psched_t *handler = sv.sival_ptr;

	/* Lock event mutex */
	pthread_mutex_lock(&handler->event_mutex);

	event_process(handler);

	/* Unlock event mutex */
	pthread_mutex_unlock(&handler->event_mutex);
}

