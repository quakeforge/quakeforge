/*
	cl_ngraph.c

	client network diagnostic graph

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/va.h"
#include "QF/ui/view.h"

#include "client/hud.h"
#include "client/screen.h"

#include "compat.h"

#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "client/sbar.h"

int cl_netgraph;
static cvar_t cl_netgraph_cvar = {
	.name = "cl_netgraph",
	.description =
		"Toggle the display of a graph showing network performance",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_netgraph },
};
float cl_netgraph_alpha;
static cvar_t cl_netgraph_alpha_cvar = {
	.name = "cl_netgraph_alpha",
	.description =
		"Net graph translucency",
	.default_value = "0.5",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &cl_netgraph_alpha },
};
int cl_netgraph_box;
static cvar_t cl_netgraph_box_cvar = {
	.name = "cl_netgraph_box",
	.description =
		" Draw box around net graph",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_netgraph_box },
};
int cl_netgraph_height;
static cvar_t cl_netgraph_height_cvar = {
	.name = "cl_netgraph_height",
	.description =
		"Set the fullscale (1s) height of the graph",
	.default_value = "32",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_netgraph_height },
};

static view_t cl_netgraph_view;

static void
cl_netgraph_f (void *data, const cvar_t *cvar)
{
	if (View_Valid (cl_netgraph_view)) {
		View_SetVisible (cl_netgraph_view, cl_netgraph != 0);
	}
}

static void
cl_netgraph_height_f (void *data, const cvar_t *cvar)
{
	cl_netgraph_height = max (32, cl_netgraph_height);
	if (View_Valid (cl_netgraph_view)) {
		view_pos_t  len = View_GetLen (cl_netgraph_view);
		View_SetLen (cl_netgraph_view, len.x, cl_netgraph_height + 25);
		View_UpdateHierarchy (cl_netgraph_view);
	}
}

static void
CL_NetGraph (view_pos_t abs, view_pos_t len)
{
	int         lost, a, l, x, y, i, o;
	int         timings[NET_TIMINGS];

	if (cl_netgraph_box) {
		r_funcs->Draw_TextBox (abs.x, abs.y, NET_TIMINGS / 8,
							   cl_netgraph_height / 8 + 1,
							   cl_netgraph_alpha * 255);
	}

	lost = CL_CalcNet ();
	x = abs.x + 8;
	y = abs.y + len.y - 9;

	l = NET_TIMINGS;
	if (l > len.x - 8)
		l = len.x - 8;
	i = cls.netchan.outgoing_sequence & NET_TIMINGSMASK;
	a = i - l;
	o = 0;
	if (a < 0) {
		memcpy (timings + o, packet_latency + a + NET_TIMINGS,
				-a * sizeof (timings[0]));
		o -= a;
		l += a;
		a = 0;
	}
	memcpy (timings + o, packet_latency + a, l * sizeof (timings[0]));
	r_funcs->R_LineGraph (x, y, timings,
						  NET_TIMINGS, cl_netgraph_height);

	x = abs.x + 8;
	y = abs.y + 8;
	r_funcs->Draw_String (x, y, va (0, "%3i%% packet loss", lost));
/*
	//FIXME don't do every frame
	view_move (cl_netgraph_view, cl_netgraph_view->xpos, hud_sb_lines);
	view_setgravity (cl_netgraph_view,
					 hud_swap ? grav_southeast : grav_southwest);
*/
}

void
CL_NetGraph_Init (void)
{
	cl_netgraph_view = View_New (hud_viewsys, cl_screen_view);
	View_SetPos (cl_netgraph_view, 0, 64);
	View_SetLen (cl_netgraph_view, NET_TIMINGS + 16, cl_netgraph_height + 25);
	View_SetGravity (cl_netgraph_view, grav_southwest);
	void       *f = CL_NetGraph;
	Ent_SetComponent (cl_netgraph_view.id, hud_func, cl_netgraph_view.reg, &f);
	View_SetVisible (cl_netgraph_view, cl_netgraph);
}

void
CL_NetGraph_Init_Cvars (void)
{
	Cvar_Register (&cl_netgraph_cvar, cl_netgraph_f, 0);
	Cvar_Register (&cl_netgraph_alpha_cvar, 0, 0);
	Cvar_Register (&cl_netgraph_box_cvar, 0, 0);
	Cvar_Register (&cl_netgraph_height_cvar, cl_netgraph_height_f, 0);
}

void
CL_NetUpdate (void)
{
	if ((hud_ping || sbar_showscores)
		&& realtime - cl.last_ping_request > 2) {
		// FIXME this should be on a timer
		cl.last_ping_request = realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}

	if (hud_pl) {
		Sbar_UpdatePL (CL_CalcNet ());
	}
}
