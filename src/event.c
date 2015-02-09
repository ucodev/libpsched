/**
 * @file event.c
 * @brief Portable Scheduler Library (libpsched)
 *        Event Processing interface
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

	/* Lock event mutex */
	if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

	/* If there's an indication to destroy the handler, don't pass from this point on... */
	if (handler->destroy) {
		handler->armed = NULL;

		if (handler->threaded) pthread_cond_signal(&handler->event_cond);

		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return;
	}

	/* Get current time */
	if (clock_gettime(CLOCK_REALTIME, &tp_now) < 0) {
		if (gettimeofday(&tv, NULL) < 0) {
			tp_now.tv_sec = tv.tv_sec;
			tp_now.tv_nsec = tv.tv_usec * 1000;
		} else {
			tp_now.tv_sec = time(NULL);
			tp_now.tv_nsec = 0;
		}
	}

	entry = handler->armed;

	/* If there's an armed entry, mark it as 'in progress' and start processing it */
	if (entry) {
		entry->in_progress = 1;

		/* No armed entry */
		handler->armed = NULL;

		/* Unlock event mutex to maximize parallel processing of entries */
		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		/* Validate if entry isn't expired */
		if ((entry->expire.tv_sec || entry->expire.tv_nsec) && (timespec_cmp(&tp_now, &entry->expire) >= 0))
			entry->expired = 1;

		/* If no step defined or if expired, set it to be removed */
		if (entry->expired) {
			entry->to_remove = 1;
		} else if ((timespec_cmp(&tp_now, &entry->trigger) >= 0)) {
			/* If the entry is recurrent... */
			if ((entry->step.tv_sec || entry->step.tv_nsec)) {
				/* Add step to trigger while its lesser than current time */
				do timespec_add(&entry->trigger, &entry->step);
				while (timespec_cmp(&tp_now, &entry->trigger) >= 0);

			} else {
				/* Otherwise, mark it to be removed from scheduling list */
				entry->to_remove = 1;
			}

			/* Execute the entry routine */
			entry->routine(entry->arg);

		}

		if (entry->to_remove)
			handler->s->del(handler->s, entry);

		/* Acquire lock again as we're managing critical regions */
		if (handler->threaded) pthread_mutex_lock(&handler->event_mutex);

		entry->in_progress = 0;
	}

	/* Since the lock may have been released before the last handler->destroy check, we must do
	 * it again here.
	 */

	/* If there's an indication to destroy the handler, don't pass from this point on... */
	if (handler->destroy) {
		handler->armed = NULL;

		if (handler->threaded) pthread_cond_signal(&handler->event_cond);

		if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);

		return;
	}

	/* Update timers */
	if (psched_update_timers(handler) < 0) {
		handler->fatal = 1;
		abort();
	}

	/* Unlock event mutex */
	if (handler->threaded) pthread_mutex_unlock(&handler->event_mutex);
}

