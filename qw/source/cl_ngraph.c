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

#include "compat.h"

#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "sbar.h"

cvar_t     *r_netgraph;
cvar_t     *r_netgraph_alpha;
cvar_t     *r_netgraph_box;

void
CL_NetGraph (void)
{
	char        st[80];
	int         lost, a, l, x, y, i;

	if (!r_netgraph->int_val)
		return;

	x = hudswap ? r_data->vid->conwidth - (NET_TIMINGS + 16): 0;
	y = r_data->vid->conheight - sb_lines - 24
		- r_data->graphheight->int_val - 1;

	if (r_netgraph_box->int_val)
		r_funcs->Draw_TextBox (x, y, NET_TIMINGS / 8,
							   r_data->graphheight->int_val / 8 + 1,
							   r_netgraph_alpha->value * 255);

	lost = CL_CalcNet ();
	x = hudswap ? r_data->vid->conwidth - (NET_TIMINGS + 8) : 8;
	y = r_data->vid->conheight - sb_lines - 9;

	l = NET_TIMINGS;
	if (l > r_data->refdef->vrect.width - 8)
		l = r_data->refdef->vrect.width - 8;
	i = cls.netchan.outgoing_sequence & NET_TIMINGSMASK;
	a = i - l;
	if (a < 0) {
		r_funcs->R_LineGraph (x, y, &packet_latency[a + NET_TIMINGS], -a);
		x -= a;
		l += a;
		a = 0;
	}
	r_funcs->R_LineGraph (x, y, &packet_latency[a], l);

	y = r_data->vid->conheight - sb_lines - 24
		- r_data->graphheight->int_val + 7;
	snprintf (st, sizeof (st), "%3i%% packet loss", lost);
	if (hudswap) {
		r_funcs->Draw_String (r_data->vid->conwidth - ((strlen (st) * 8) + 8),
							  y, st);
	} else {
		r_funcs->Draw_String (8, y, st);
	}
}
