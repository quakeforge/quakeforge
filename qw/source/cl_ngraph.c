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

#include "compat.h"

#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "sbar.h"

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
view_t     *cl_netgraph_view;

static void
cl_netgraph_f (void *data, const cvar_t *cvar)
{
	if (cl_netgraph_view) {
		cl_netgraph_view->visible = cl_netgraph != 0;
	}
}

static void
cl_netgraph_height_f (void *data, const cvar_t *cvar)
{
	cl_netgraph_height = max (32, cl_netgraph_height);
	if (cl_netgraph_view) {
		view_resize (cl_netgraph_view, cl_netgraph_view->xlen,
					 cl_netgraph_height + 25);
	}
}

void
CL_NetGraph (view_t *view)
{
	int         lost, a, l, x, y, i, o;
	int         timings[NET_TIMINGS];

	x = view->xabs;
	y = view->yabs;

	if (cl_netgraph_box) {
		r_funcs->Draw_TextBox (x, y, NET_TIMINGS / 8,
							   cl_netgraph_height / 8 + 1,
							   cl_netgraph_alpha * 255);
	}

	lost = CL_CalcNet ();
	x = view->xabs + 8;
	y = view->yabs + view->ylen - 9;

	l = NET_TIMINGS;
	if (l > view->xlen - 8)
		l = view->xlen - 8;
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

	x = view->xabs + 8;
	y = view->yabs + 8;
	r_funcs->Draw_String (x, y, va (0, "%3i%% packet loss", lost));
}

void
CL_NetGraph_Init_Cvars (void)
{
	Cvar_Register (&cl_netgraph_cvar, cl_netgraph_f, 0);
	Cvar_Register (&cl_netgraph_alpha_cvar, 0, 0);
	Cvar_Register (&cl_netgraph_box_cvar, 0, 0);
	Cvar_Register (&cl_netgraph_height_cvar, cl_netgraph_height_f, 0);
}
