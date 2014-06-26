/**
 * @file event.c
 * @brief Portable Scheduler Library (libpsched)
 *        Event Processing interface
 *
 * Date: 26-06-2014
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
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include <sys/time.h>

#include "sched.h"
#include "timespec.h"

void event_process(psched_t *handler) {
	struct psched_entry *entry = NULL;
	struct timespec tp_now;
	struct timeval tv;

	if (clock_gettime(CLOCK_REALTIME, &tp_now) < 0) {
		if (gettimeofday(&tv, NULL) < 0) {
			tp_now.tv_sec = tv.tv_sec;
			tp_now.tv_nsec = tv.tv_usec * 1000;
		} else {
			tp_now.tv_sec = time(NULL);
			tp_now.tv_nsec = 0;
		}
	}

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	entry = handler->armed;

	/* If exists, mark this entry as 'in progress' */
	if (entry) {
		entry->in_progress = 1;

		handler->armed = NULL;
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

	/* If there's an armed entry... */
	if (entry) {
		/* Validate if entry isn't expired */
		if ((entry->expire.tv_sec || entry->expire.tv_nsec) && (timespec_cmp(&tp_now, &entry->expire) >= 0))
			entry->expired = 1;

		/* If no step defined or if expired, set it to be removed */
		if (entry->expired) {
			entry->to_remove = 1;
		} else if ((timespec_cmp(&tp_now, &entry->trigger) >= 0)) {
			/* Lock event mutex */
			if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

			/* If the entry is recurrent... */
			if ((entry->step.tv_sec || entry->step.tv_nsec)) {
				/* Add step to trigger */
				timespec_add(&entry->trigger, &entry->step);
				entry->in_progress = 0;
			} else {
				/* Otherwise, mark it to be removed from scheduling list */
				entry->to_remove = 1;
			}

			/* Unlock event mutex */
			if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

			entry->routine(entry->arg);
		}

		if (entry->to_remove) {
			/* Lock event mutex */
			if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

			handler->s->del(handler->s, entry);

			/* Unlock event mutex */
			if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);
		}
	}

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	if (psched_update_timers(handler) < 0)
		abort();

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);
}

