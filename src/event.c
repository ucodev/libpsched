/**
 * @file event.c
 * @brief Portable Scheduler Library (libpsched)
 *        Event Processing interface
 *
 * Date: 12-06-2014
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

	if (handler->armed) {
		if ((handler->armed->expire.tv_sec || handler->armed->expire.tv_nsec) && (timespec_cmp(&tp_now, &handler->armed->expire) >= 0))
			handler->armed->expired = 1;

		if ((timespec_cmp(&tp_now, &handler->armed->trigger) >= 0) && !handler->armed->expired) {
			handler->armed->routine(handler->armed->arg);

			if ((handler->armed->step.tv_sec || handler->armed->step.tv_nsec)) {
				/* Add step to trigger */
				timespec_add(&handler->armed->trigger, &handler->armed->step);

				if (!(entry = mm_alloc(sizeof(struct psched_entry))))
					abort();

				memcpy(entry, handler->armed, sizeof(struct psched_entry));
				entry->id = (pschedid_t) (uintptr_t) entry;

				handler->s->insert(handler->s, entry);
			}

			handler->s->del(handler->s, handler->armed);
		} else if (handler->armed->expired) {
			/* Expired entries should be removed */
			handler->s->del(handler->s, handler->armed);
		}
	}

	handler->armed = NULL;

	if (psched_update_timers(handler) < 0)
		abort();
}

