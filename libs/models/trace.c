/*
	trace.c

	BSP line tracing

	Copyright (C) 2004 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/9/25

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/model.h"

#include "compat.h"
#include "world.h"

/* LINE TESTING IN HULLS */

typedef struct {
	vec3_t     backpt;
	int        side;
	int        num;
	mplane_t   *plane;
} tracestack_t;

static inline void
calc_impact (trace_t *trace, const vec3_t start, const vec3_t end,
			 mplane_t *plane)
{
	vec_t       t1, t2, frac;
	int         i, side;

	if (plane->type < 3) {
		t1 = start[plane->type] - plane->dist;
		t2 = end[plane->type] - plane->dist;
	} else {
		t1 = DotProduct (plane->normal, start) - plane->dist;
		t2 = DotProduct (plane->normal, end) - plane->dist;
	}

	side = t1 < 0;
	if (side) {
		frac = (t1 + DIST_EPSILON) / (t1 - t2);
		// invert plane paramterers
		trace->plane.normal[0] = -plane->normal[0];
		trace->plane.normal[1] = -plane->normal[1];
		trace->plane.normal[2] = -plane->normal[2];
		trace->plane.dist = -plane->dist;
	} else {
		frac = (t1 - DIST_EPSILON) / (t1 - t2);
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	}
	frac = bound (0, frac, 1);
	trace->fraction = frac;
	for (i = 0; i < 3; i++)
		trace->endpos[i] = start[i] + frac * (end[i] - start[i]);
}

qboolean
MOD_TraceLine (hull_t *hull, int num, const vec3_t start, const vec3_t end,
			   trace_t *trace)
{
	vec_t       front, back;
	vec3_t      frontpt, backpt;
	int         side, empty, solid;
	tracestack_t *tstack;
	tracestack_t tracestack[256];
	dclipnode_t *node;
	mplane_t   *plane, *split_plane;

	VectorCopy (start, frontpt);
	VectorCopy (end, backpt);

	tstack = tracestack;
	empty = 0;
	solid = 0;
	split_plane = 0;

	while (1) {
		while (num < 0) {
			if (!solid && num != CONTENTS_SOLID) {
				empty = 1;
				if (num == CONTENTS_EMPTY)
					trace->inopen = true;
				else
					trace->inwater = true;
			} else if (!empty && num == CONTENTS_SOLID) {
				solid = 1;
			} else if (empty || solid) {
				// DONE!
				trace->allsolid = solid & (num == CONTENTS_SOLID);
				trace->startsolid = solid;
				calc_impact (trace, start, end, split_plane);
				return false;
			}

			// pop up the stack for a back side
			if (tstack-- == tracestack) {
				trace->allsolid = solid & (num == CONTENTS_SOLID);
				trace->startsolid = solid;
				return true;
			}

			// set the hit point for this plane
			VectorCopy (backpt, frontpt);

			// go down the back side
			VectorCopy (tstack->backpt, backpt);
			side = tstack->side;
			split_plane = tstack->plane;

			num = hull->clipnodes[tstack->num].children[side ^ 1];
		}

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3) {
			front = frontpt[plane->type] - plane->dist;
			back = backpt[plane->type] - plane->dist;
		} else {
			front = DotProduct (plane->normal, frontpt) - plane->dist;
			back = DotProduct (plane->normal, backpt) - plane->dist;
		}

		if (front >= 0 && back >= 0) {
			num = node->children[0];
			continue;
		}

		if (front < 0 && back < 0) {
			num = node->children[1];
			continue;
		}

		side = front < 0;

		front = front / (front - back);

		tstack->num = num;
		tstack->side = side;
		tstack->plane = plane;
		VectorCopy (backpt, tstack->backpt);

		tstack++;

		backpt[0] = frontpt[0] + front * (backpt[0] - frontpt[0]);
		backpt[1] = frontpt[1] + front * (backpt[1] - frontpt[1]);
		backpt[2] = frontpt[2] + front * (backpt[2] - frontpt[2]);

		num = node->children[side];
	}
}
