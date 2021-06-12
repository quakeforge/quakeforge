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
#include "QF/va.h"

#include "QF/plugin/console.h"

#include "QF/ui/view.h"

#include "sv_console.h"

#include "qtv/include/client.h"
#include "qtv/include/server.h"
#include "qtv/include/qtv.h"

static void
draw_clients (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	sv_sbar_t  *sb = sv_view->obj;
	const char *str;
	const char *s;
	char       *d;

	str = va (0, "[CL: %3d]", client_count);
	for (s = str, d = sb->text + view->xrel; *s; s++)
		*d++ = *s;
}

static void
draw_servers (view_t *view)
{
	sv_view_t  *sv_view = view->data;
	sv_sbar_t  *sb = sv_view->obj;
	const char *str;
	const char *s;
	char       *d;

	str = va (0, "[SV: %2d]", server_count);
	for (s = str, d = sb->text + view->xrel; *s; s++)
		*d++ = *s;
}

void
qtv_sbar_init (void)
{
	view_t     *status;
	view_t     *view;

	if (!con_module || !con_module->data->console->status_view)
		return;
	status = con_module->data->console->status_view;

	view = view_new (0, 0, 8, 1, grav_northwest);
	view->draw = draw_servers;
	view->data = status->data;
	view_add (status, view);

	view = view_new (8, 0, 9, 1, grav_northwest);
	view->draw = draw_clients;
	view->data = status->data;
	view_add (status, view);
}
