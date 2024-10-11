/*
	backtrace.c

	Support for dumping backtrace information.

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/12/04

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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <string.h>
#include <backtrace.h>

#include "QF/backtrace.h"
#include "QF/dstring.h"
#include "QF/sys.h"

static struct backtrace_state *bt_state;

static void bt_error (void *data, const char *msg, int errnum)
{
	if (errnum > 0) {
		Sys_Printf ("%s: %s\n", msg, strerror (errnum));
	} else if (errnum < 0) {
		Sys_Printf ("%s: no symbol table\n", msg);
	}
}

void
BT_Init (const char *filename)
{
	bt_state = backtrace_create_state (filename, 1, bt_error, 0);
}

static int
bt_print (void *data, uintptr_t pc, const char *fname, int lineno,
		  const char *func)
{
	dstring_t  *str = data;
	dasprintf (str, "(%"PRIxPTR") %s:%s:%d", pc, func, fname, lineno);
	return 0;
}

void
BT_pcInfo (dstring_t *str, uintptr_t pc)
{
	if (bt_state) {
		backtrace_pcinfo (bt_state, pc, bt_print, 0, str);
	} else {
	}
}
