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
	vec_t       t1, t2, frac, offset;
	vec3_t      dist;

	t1 = PlaneDiff (start, plane);
	t2 = PlaneDiff (end, plane);
	offset = 0;
	if (trace->isbox) {
		if (plane->type < 3)
			offset = trace->extents[plane->type];
		else
			offset = (fabs (trace->extents[0] * plane->normal[0])
					  + fabs (trace->extents[1] * plane->normal[1])
					  + fabs (trace->extents[2] * plane->normal[2]));
	}

	if (t1 < 0) {
		frac = (t1 + offset + DIST_EPSILON) / (t1 - t2);
		// invert plane paramterers
		VectorNegate (plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	} else {
		frac = (t1 - offset - DIST_EPSILON) / (t1 - t2);
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	}
	frac = bound (0, frac, 1);
	trace->fraction = frac;
	VectorSubtract (end, start, dist);
	VectorMultAdd (start, frac, dist, trace->endpos);
}

qboolean
MOD_TraceLine (hull_t *hull, int num, const vec3_t start, const vec3_t end,
			   trace_t *trace)
{
	vec_t       front, back, offset, frac;
	vec3_t      frontpt, backpt, dist;
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

		offset = 0;
		front = PlaneDiff (frontpt, plane);
		back = PlaneDiff (backpt, plane);
		if (trace->isbox) {
			if (plane->type < 3)
				offset = trace->extents[plane->type];
			else
				offset = (fabs (trace->extents[0] * plane->normal[0])
						  + fabs (trace->extents[1] * plane->normal[1])
						  + fabs (trace->extents[2] * plane->normal[2]));
		}

		// when offset is 0, the following is equivalent to:
		//		if (front >= 0 && back >= 0) ...
		//		if (front < 0 && back < 0) ...
		// due to the order of operations
		// however, when (front == offset && back == offset) or
		// (front == -offset && back == -offset), the trace will go down
		// the /correct/ side of the plane: ie, the side the box is actually
		// on
		if (front >= 0 && back >= 0) {
			num = node->children[0];
			continue;
		}
		if (front < 0 && back < 0) {
			num = node->children[1];
			continue;
		}
		if (front >= offset || front <= -offset
			|| back >= offset || back <= -offset) {
			// either:
			//  back is guaranteed to be < offset or > -offset
			// or
			//  front is guaranteed to be < offset or > -offset
			// so splitting is needed, on the offset plane closes to front
			side = front < 0;
			if (front < 0) {
				frac = (front + offset) / (front - back);
			} else {
				frac = (front - offset) / (front - back);
			}
			frac = bound (0, frac, 1);
		} else {
			// both: 
			//  front is guaranteed to be < offset and > -offset
			// and
			//  back is guaranteed to be < offset and > -offset
			frac = 1;
			side = front < back;
		}

		tstack->num = num;
		tstack->side = side;
		tstack->plane = plane;
		VectorCopy (backpt, tstack->backpt);
		tstack++;

		VectorSubtract (backpt, frontpt, dist);
		VectorMultAdd (frontpt, frac, dist, backpt);

		num = node->children[side];
	}
}
