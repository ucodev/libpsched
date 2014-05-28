/**
 * @file sig.h
 * @brief Portable Scheduler Library (libpsched)
 *        Signals interface header
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


#ifndef LIBPSCHED_SIG_H
#define LIBPSCHED_SIG_H


#include <signal.h>

/* Prototypes */

void sig_handler(int sig, siginfo_t *si, void *context);

#endif
