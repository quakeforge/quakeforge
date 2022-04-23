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

#include "QF/ui/view.h"

#include "r_internal.h"


#define MAX_TIMINGS 100
int          graphval;


/*
	R_TimeGraph

	Performance monitoring tool
*/
void
R_TimeGraph (view_t *view)
{
	static int  timex;
	int         a;
	int         l;
	double      r_time2;
	static int  r_timings[MAX_TIMINGS];
	int         timings[MAX_TIMINGS];
	int         o;

	r_time2 = Sys_DoubleTime ();

	r_timings[timex] = (r_time2 - r_time1) * 10000;
	//printf ("%d %g\n", r_timings[timex], r_time2 - r_time1);

	l = MAX_TIMINGS;
	if (l > r_refdef.vrect.width)
		l = r_refdef.vrect.width;
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
	r_funcs->R_LineGraph (view->xabs, view->yabs, r_timings,
						  MAX_TIMINGS, 200);//r_data->graphheight->int_val);

	timex = (timex + 1) % MAX_TIMINGS;
}

void
R_ZGraph (view_t *view)
{
	int         w;
	static int  height[256];

	if (r_refdef.vrect.width <= 256)
		w = r_refdef.vrect.width;
	else
		w = 256;

	height[r_framecount & 255] = ((int) r_refdef.frame.position[2]) & 31;

	r_funcs->R_LineGraph (view->xabs, view->yabs, height,
						  w, *r_data->graphheight);
}
