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
#if 0
static inline void
check_contents (int num, trace_t *trace)
{
	if (num != CONTENTS_SOLID) {
		trace->allsolid = false;
		if (num == CONTENTS_EMPTY)
			trace->inopen = true;
		else
			trace->inwater = true;
	} else {
		if (!trace->inwater && !trace->inopen)
			trace->startsolid = true;
	}
}

#if 1
typedef struct {
	const vec_t *start;
	const vec_t *end;
	trace_t     trace;
	hull_t      hull;
	mplane_t   *plane;
	float       fraction;
} tl_t;

static inline float
calc_offset (trace_t *trace, mplane_t *plane)
{
	if (trace->isbox) {
		if (plane->type < 3)
			return trace->extents[plane->type];
		else
			return (fabs (trace->extents[0] * plane->normal[0])
					+ fabs (trace->extents[1] * plane->normal[1])
					+ fabs (trace->extents[2] * plane->normal[2]));
	}
	return 0;
}

static void
traceline (int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2,
		   tl_t *tl)
{
	dclipnode_t *node;
	mplane_t   *plane;
	float       t1, t2, frac, frac2, midf, offset;
	int         side;
	vec3_t      mid;

	if (num < 0) {
		check_contents (num, &tl->trace);
		t1 = t2 = -3.14;
		if (!tl->trace.startsolid && num == CONTENTS_SOLID) {
			if (tl->plane) {
				t1 = PlaneDiff (tl->start, tl->plane);
				t2 = PlaneDiff (tl->end, tl->plane);
				offset = calc_offset (&tl->trace, tl->plane);
				if (t1 < t2) {
					tl->fraction  = (t1 + offset) / (t1 - t2);
				} else if (t1 > t2) {
					tl->fraction  = (t1 - offset) / (t1 - t2);
				} else {
					tl->fraction  = p1f;
					Sys_Printf ("help! help! the world is falling apart!\n");
				}
//			} else {
//				tl->fraction = p1f;
			}
		}
		return;
	}

	node = tl->hull.clipnodes + num;
	plane = tl->hull.planes + node->planenum;

	t1 = PlaneDiff (p1, plane);
	t2 = PlaneDiff (p2, plane);
	offset = calc_offset (&tl->trace, plane);

	if (t1 >= offset && t2 >= offset) {
		traceline (node->children[0], p1f, p2f, p1, p2, tl);
		return;
	}
	if (t1 < -offset && t2 < -offset) {
		traceline (node->children[1], p1f, p2f, p1, p2, tl);
		return;
	}

	if (t1 < t2) {
		side = 1;
		frac  = (t1 - offset) / (t1 - t2);
		frac2 = (t1 + offset) / (t1 - t2);
	} else if (t1 > t2) {
		side = 0;
		frac  = (t1 + offset) / (t1 - t2);
		frac2 = (t1 - offset) / (t1 - t2);
	} else {
		side = 0;
		frac  = 0;
		frac2 = 0;
		plane = tl->plane;
	}

	frac = bound (0, frac, 1);
	midf = p1f + (p2f - p1f) * frac;
	VectorSubtract (p2, p1, mid);
	VectorMultAdd (p1, frac, mid, mid);
	traceline (node->children[side], p1f, midf, p1, mid, tl);

	frac2 = bound (0, frac2, 1);
	midf = p1f + (p2f - p1f) * frac2;
	if (midf <= tl->fraction) {
		VectorSubtract (p2, p1, mid);
		VectorMultAdd (p1, frac2, mid, mid);
		tl->plane = plane;
		traceline (node->children[side ^ 1], midf, p2f, mid, p2, tl);
	}
}

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	tl_t        tl;

	tl.start = start_point;
	tl.end = end_point;
	tl.trace = *trace;
	tl.hull = *hull;
	tl.fraction = 1;
	tl.plane = 0;
	traceline (num, 0, 1, start_point, end_point, &tl);
	if (tl.fraction < 1) {
		calc_impact (&tl.trace, start_point, end_point, tl.plane);
	}
	*trace = tl.trace;
}
#else
static int
point_contents (hull_t *hull, int num, vec3_t p)
{
	float       d;
	dclipnode_t *node;
	mplane_t   *plane;

	while (num >= 0) {
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		d = PlaneDiff (p, plane);
		num = node->children[d < 0];
	}

	return num;
}

static qboolean
traceline (hull_t *hull, int num, float p1f, float p2f,
			const vec3_t p1, const vec3_t p2, trace_t *trace)
{
	dclipnode_t *node;
	mplane_t   *plane;
	float       t1, t2, frac, midf;
	vec3_t      mid;
	int         side;

	if (num < 0) {
		check_contents (num, trace);
		return true;
	}

	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	t1 = PlaneDiff (p1, plane);
	t2 = PlaneDiff (p2, plane);

	if (t1 >= 0 && t2 >= 0)
		return traceline (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if (t1 < 0 && t2 < 0)
		return traceline (hull, node->children[1], p1f, p2f, p1, p2, trace);

	frac = t1 / (t1 - t2);
	frac = bound (0, frac, 1);

	midf = p1f + (p2f - p1f) * frac;
	VectorSubtract (p2, p1, mid);
	VectorMultAdd (p1, frac, mid, mid);

	side = t1 < 0;

	if (!traceline (hull, node->children[side], p1f, midf, p1, mid, trace))
		return false;

	if (point_contents (hull, node->children[side ^ 1], mid) != CONTENTS_SOLID)
		return traceline (hull, node->children[side ^ 1], midf, p2f, mid, p2,
						  trace);

	if (trace->allsolid)
		return false;		// never got out of the solid area

	calc_impact (trace, p1, p2, plane);

	return false;
}

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	traceline (hull, num, 0, 1, start_point, end_point, trace);
}
#endif
#else
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
			Sys_Printf ("foo\n");
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
#endif
