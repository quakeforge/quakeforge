/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2007 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include "QF/console.h"
#include "QF/view.h"
#include "QF/va.h"

#include "QF/plugin/console.h"

#include "sv_console.h"

#include "qw/include/server.h"
#include "qw/include/sv_recorder.h"

static void
draw_cpu (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	sv_sbar_t  *sb = sv_view->obj;
	double      cpu;
	const char *cpu_str;
	const char *s;
	char       *d;

	cpu = (svs.stats.latched_active + svs.stats.latched_idle);
	cpu = 100 * svs.stats.latched_active / cpu;

	cpu_str = va (0, "[CPU: %3d%%]", (int) cpu);
	for (s = cpu_str, d = sb->text + view->xrel; *s; s++)
		*d++ = *s;
	if (cpu > 70.0) {
		int         i;
		for (i = 6; i < 9; i++)
			sb->text[view->xrel + i] |= 0x80;
	}
}

static void
draw_rec (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	sv_sbar_t  *sb = sv_view->obj;
	const char *str;
	const char *s;
	char       *d;

	str = va (0, "[REC: %d]", SVR_NumRecorders ());
	for (s = str, d = sb->text + view->xrel; *s; s++)
		*d++ = *s;
}

void
SV_Sbar_Init (void)
{
	view_t     *status;
	view_t     *view;

	if (!con_module || !con_module->data->console->status_view)
		return;
	status = con_module->data->console->status_view;

	view = view_new (0, 0, 11, 1, grav_northwest);
	view->draw = draw_cpu;
	view->data = status->data;
	view_add (status, view);

	view = view_new (11, 0, 8, 1, grav_northwest);
	view->draw = draw_rec;
	view->data = status->data;
	view_add (status, view);
}
