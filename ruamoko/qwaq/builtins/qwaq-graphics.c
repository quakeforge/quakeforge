/*
	qwaq-graphics.c

	Qwaq graphics initialization

	Copyright (C) 2021 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/12/20

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
# include "config.h"
#endif

#include <string.h>

#include "QF/zone.h"

#include "ruamoko/qwaq/qwaq.h"

static progsinit_f main_app[] = {
	BI_Graphics_Init,
	0
};
#if 0// FIXME no multi-thread support yet
static progsinit_f secondary_app[] = {
	0
};
#endif
static FILE *logfile;

static __attribute__((format(PRINTF, 1, 0))) void
qwaq_print (const char *fmt, va_list args)
{
	vfprintf (logfile, fmt, args);
	fflush (logfile);
}

int
qwaq_init_threads (qwaq_thread_set_t *thread_data)
{
	int         main_ind = -1;
	size_t      memsize = 8 * 1024 * 1024;
	memhunk_t  *hunk = Memory_Init (Sys_Alloc (memsize), memsize);

	logfile = fopen ("qwaq-graphics.log", "wt");
	Sys_SetStdPrintf (qwaq_print);

	for (size_t i = 1, thread_ind = 0; i < thread_data->size; i++) {
		qwaq_thread_t *thread = thread_data->a[i];
		progsinit_f *app_funcs = 0;//secondary_app;

		if (thread->args.size && thread->args.a[0]
			&& strcmp (thread->args.a[0], "--qargs")) {
			// skip the args set that's passed to qargs
			continue;
		}
		if (thread_ind < thread_data->a[0]->args.size) {
			thread->args.a[0] = thread_data->a[0]->args.a[thread_ind++];
		} else {
			printf ("ignoring extra arg sets\n");
			break;
		}
		if (main_ind < 0) {
			main_ind = i;
			app_funcs = main_app;
		}
		thread->progsinit = app_funcs;
		thread->rua_security = 1;
		thread->hunk = hunk;	//FIXME shared (but currently only one thread)
	}
	return main_ind;
}
