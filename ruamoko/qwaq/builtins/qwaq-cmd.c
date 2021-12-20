/*
	qwaq-cmd.c

	Qwaq command-line tool initialization

	Copyright (C) 2021 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/07/06

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <getopt.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/gib.h"
#include "QF/idparse.h"
#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/ruamoko.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/debugger/debug.h"

static progsinit_f main_app[] = {
	0
};

int
qwaq_init_threads (qwaq_thread_set_t *thread_data)
{
	int         main_ind = -1;

	for (size_t i = 1, thread_ind = 0; i < thread_data->size; i++) {
		qwaq_thread_t *thread = thread_data->a[i];
		progsinit_f *app_funcs = main_app;

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
	}
	return main_ind;
}
