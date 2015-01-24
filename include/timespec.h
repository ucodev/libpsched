/**
 * @file timespec.h
 * @brief Portable Scheduler Library (libpsched)
 *        Timespec interface header
 *
 * Date: 11-06-2014
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


#ifndef LIBPSCHED_TIMESPEC_H
#define LIBPSCHED_TIMESPEC_H

#include <time.h>

/* Prototypes */
void timespec_add(struct timespec *dest, const struct timespec *src);
int timespec_cmp(const struct timespec *ts1, const struct timespec *ts2);

#endif
