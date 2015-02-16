/**
 * @file sig.c
 * @brief Portable Scheduler Library (libpsched)
 *        Signals interface
 *
 * Date: 16-02-2015
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

#include "psched.h"
#include "event.h"
#include "mm.h"

void sig_handler(int sig, siginfo_t *si, void *context) {
	psched_t *handler = (psched_t *) si->si_value.sival_ptr;

	event_process(handler);
}

