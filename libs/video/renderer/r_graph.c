/*
	r_graph.c

	rednerer diagnostic graphs

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
#include <string.h>

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"
#include "QF/plugin.h"
#include "QF/sys.h"

#include "r_internal.h"


#define MAX_TIMINGS 100
int          graphval;


/*
	R_TimeGraph

	Performance monitoring tool
*/
void
R_TimeGraph (void)
{
	static int  timex;
	int         a;
	int         l;
	//XXX float       r_time2;
	static int  r_timings[MAX_TIMINGS];
	int         timings[MAX_TIMINGS];
	int         x, o;

	//XXX r_time2 = Sys_DoubleTime ();

	a = graphval;

	r_timings[timex] = a;

	l = MAX_TIMINGS;
	if (l > r_refdef.vrect.width)
		l = r_refdef.vrect.width;
	x = r_refdef.vrect.width - l;
	o = 0;
	a = timex - l;
	if (a < 0) {
		memcpy (timings + o, r_timings + a + MAX_TIMINGS,
				-a * sizeof (timings[0]));
		o -= a;
		l += a;
		a = 0;
	}
	memcpy (timings + o, r_timings + a, l * sizeof (timings[0]));
	vr_funcs->R_LineGraph (x, r_refdef.vrect.height - 2, r_timings,
						   MAX_TIMINGS, vr_data.graphheight->int_val);

	timex = (timex + 1) % MAX_TIMINGS;
}

void
R_ZGraph (void)
{
	int         x, w;
	static int  height[256];

	if (r_refdef.vrect.width <= 256)
		w = r_refdef.vrect.width;
	else
		w = 256;

	height[r_framecount & 255] = ((int) r_origin[2]) & 31;

	x = 0;
	vr_funcs->R_LineGraph (x, r_refdef.vrect.height - 2, height,
						   w, vr_data.graphheight->int_val);
}
