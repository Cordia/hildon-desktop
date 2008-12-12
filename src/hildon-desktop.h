/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Kimmo Hamalainen <kimmo.hamalainen@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef HILDON_DESKTOP_H
#define HILDON_DESKTOP_H

#include <stdlib.h>
#include <locale.h>
#include <libintl.h>

#define _(X) gettext(X)

/* Do not create threads in scratchbox if $HD_NOTHREADS is defined.
 * gdb doesn't like threads. */
#ifdef __i386__
# define hd_disable_threads()          getenv("HD_NOTHREADS")
#else
# define hd_disable_threads()          0
#endif

#endif
