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
#include "QF/sys.h"

#include "compat.h"
#include "world.h"

/* LINE TESTING IN HULLS */

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

typedef struct {
	vec3_t     end;
	int        side;
	int        num;
	mplane_t   *plane;
} tracestack_t;

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	vec_t       start_dist, end_dist, offset, frac;
	vec3_t      start, end, dist;
	int         side, empty, solid;
	tracestack_t *tstack;
	tracestack_t tracestack[256];
	dclipnode_t *node;
	mplane_t   *plane, *split_plane;

	VectorCopy (start_point, start);
	VectorCopy (end_point, end);

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
			} else if (solid && num != CONTENTS_SOLID) {
				//FIXME not sure what I want
				//made it out of the solid and into open space, continue
				//on as if we were always in empty space
				empty = 1;
				solid = 0;
				trace->startsolid = 1;
				if (num == CONTENTS_EMPTY)
					trace->inopen = true;
				else
					trace->inwater = true;
			} else if (empty/* || solid*/) {//FIXME not sure what I want
				// DONE!
				trace->allsolid = solid & (num == CONTENTS_SOLID);
				trace->startsolid = solid;
				calc_impact (trace, start_point, end_point, split_plane);
				return;
			}

			// pop up the stack for a back side
			if (tstack-- == tracestack) {
				trace->allsolid = solid & (num == CONTENTS_SOLID);
				trace->startsolid = solid;
				return;
			}

			// set the hit point for this plane
			VectorCopy (end, start);

			// go down the back side
			VectorCopy (tstack->end, end);
			side = tstack->side;
			split_plane = tstack->plane;

			num = hull->clipnodes[tstack->num].children[side ^ 1];
		}

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		offset = 0;
		start_dist = PlaneDiff (start, plane);
		end_dist = PlaneDiff (end, plane);
		if (trace->isbox) {
			if (plane->type < 3)
				offset = trace->extents[plane->type];
			else
				offset = (fabs (trace->extents[0] * plane->normal[0])
						  + fabs (trace->extents[1] * plane->normal[1])
						  + fabs (trace->extents[2] * plane->normal[2]));
		}

		/*	when offset is 0, the following is equivalent to:
				if (start_dist >= 0 && end_dist >= 0) ...
				if (start_dist < 0 && end_dist < 0) ...
			due to the order of operations
			however, when (start_dist == offset && end_dist == offset) or
			(start_dist == -offset && end_dist == -offset), the trace will go
			down the /correct/ side of the plane: ie, the side the box is
			actually on
		*/
		if (start_dist >= offset && end_dist >= offset) {
			// entirely in front of the plane
			num = node->children[0];
			continue;
		}
		//if (start_dist <= -offset && end_dist <= -offset) {
		//XXX not so equivalent, it seems.
		if (start_dist < -offset && end_dist < -offset) {
			// entirely behind the plane
			num = node->children[1];
			continue;
		}
		// when offset is 0, equvalent to (start_dist >= 0 && end_dist < 0) and
		// (start_dist < 0 && end_dist >= 0) due to the above tests.
		if (start_dist >= offset && end_dist <= -offset) {
			side = 0;
			frac = (start_dist - offset) / (start_dist - end_dist);
		} else if (start_dist <= offset && end_dist >= offset) {
			side = 1;
			frac = (start_dist + offset) / (start_dist - end_dist);
		} else {
			// get here only when offset is non-zero
			Sys_DPrintf ("foo\n");
			frac = 1;
			side = start_dist < end_dist;
		}
		frac = bound (0, frac, 1);

		tstack->num = num;
		tstack->side = side;
		tstack->plane = plane;
		VectorCopy (end, tstack->end);
		tstack++;

		VectorSubtract (end, start, dist);
		VectorMultAdd (start, frac, dist, end);

		num = node->children[side];
	}
}
