/*
	qfselect.h

	select fd_set wrapper

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/09/28

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __qfselect_h
#define __qfselect_h

#include "config.h"

#ifdef HAVE_WINDOWS_H
# define mouse_event __hide_mouse_event
# include "winquake.h"
# undef mouse_event
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

typedef struct qf_fd_set {
	fd_set fdset;
} qf_fd_set;

GNU89INLINE inline void QF_FD_CLR (int fd, qf_fd_set *set);
GNU89INLINE inline int QF_FD_ISSET (int fd, qf_fd_set *set);
GNU89INLINE inline void QF_FD_SET (int fd, qf_fd_set *set);
GNU89INLINE inline void QF_FD_ZERO (qf_fd_set *set);

#ifndef IMPLEMENT_QFSELECT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
QF_FD_CLR (int fd, qf_fd_set *set)
{
	FD_CLR (fd, &set->fdset);
}

#ifndef IMPLEMENT_QFSELECT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
QF_FD_ISSET(int fd, qf_fd_set *set)
{
	return FD_ISSET(fd, &set->fdset);
}

#ifndef IMPLEMENT_QFSELECT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
QF_FD_SET(int fd, qf_fd_set *set)
{
	FD_SET(fd, &set->fdset);
}

#ifndef IMPLEMENT_QFSELECT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
QF_FD_ZERO(qf_fd_set *set)
{
	FD_ZERO(&set->fdset);
}

#endif//__qfselect_h
