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
#if ENABLE_BOXCLIP || defined(TEST_BOXCLIP)

#define ALLSOLID 1
#define STARTSOLID 2
#define INOPEN 4
#define INWATER 8

#define SOLID (ALLSOLID | STARTSOLID)

typedef struct {
	mplane_t    *plane;
	vec3_t       point;				// arbitrary point on plane
	int          side;
} tp_t;

typedef struct {
	vec3_t      p;
	vec3_t		v;
} line_t;

typedef struct {
	const vec_t *start;
	const vec_t *end;
	const vec_t *extents;
	qboolean    isbox;
	hull_t      hull;
	tp_t        split;
	tp_t        impact;
	float       fraction;
	int         flags;
} tl_t;

static inline void
set_split (tl_t *tl, tp_t *n,  tp_t *o)
{
	if (o)
		*o = tl->split;
	if (n) {
		tl->split = *n;
	} else {
		tl->split.plane = 0;
		VectorZero (tl->split.point);
		tl->split.side = 0;
	}
}

static inline void
set_impact (tl_t *tl, mplane_t *plane, int side)
{
	tl->impact.plane = plane;
	tl->impact.side = side;
}

#define print_tp(tp) \
	Sys_Printf ("%s [(%g %g %g) %g %d]\n", #tp, \
				(tp).plane->normal[0], (tp).plane->normal[1], \
				(tp).plane->normal[2], (tp).plane->dist, (tp).side)

static inline vec_t
sgn (vec_t v)
{
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

static inline float
calc_offset (tl_t *tl, mplane_t *plane)
{
	if (tl->isbox) {
		if (plane->type < 3)
			return tl->extents[plane->type];
		else
			return (fabs (tl->extents[0] * plane->normal[0])
					+ fabs (tl->extents[1] * plane->normal[1])
					+ fabs (tl->extents[2] * plane->normal[2]));
	}
	return 0;
}


static void
impact (tl_t *tl, tp_t *split)
{
	float       t1, t2, offset, frac;
	int         side = 0;

	t1 = PlaneDiff (tl->start, split->plane);
	t2 = PlaneDiff (tl->end, split->plane);
	offset = calc_offset (tl, split->plane);
	if (t1 < t2) {
		side = -1;
		frac = (t1 + offset) / (t1 - t2);
	} else if (t1 > t2) {
		side = 0;
		frac = (t1 - offset) / (t1 - t2);
	} else {
		frac = 0;
		Sys_DPrintf ("help! help! the world is falling apart!\n");
	}
	if (frac >= 0) {
		tl->fraction = frac;
		set_impact (tl, split->plane, side);
//		print_tp (tl->impact);
	}
}

static void
select_vertex (tl_t *tl, tp_t *split, vec3_t vert)
{
	int         axis;

	for (axis = 0; axis < 3; axis++) {
		vec_t       s = sgn (tl->split.plane->normal[axis]);
		if (!tl->split.side)
			s = -s;
		if (!s) {
			s = sgn (split->plane->normal[axis]);
			if (split->side)
				s = -s;
			if (!s)
				s = 1;
		}
		vert[axis] = tl->extents[axis] * s;
	}
}

static int
intersection_point (const mplane_t *plane, const vec3_t _p1, const vec3_t _p2,
					const vec3_t offs, vec3_t point)
{
	vec_t       t1, t2;
	vec3_t      p1, p2;

	VectorAdd (_p1, offs, p1);
	VectorAdd (_p2, offs, p2);

	t1 = PlaneDiff (p1, plane);
	t2 = PlaneDiff (p2, plane);

	if (!(t1 - t2))
		return 0;
	VectorSubtract (p2, p1, point);
	VectorMultAdd (p1, t1 / (t1 - t2), point, point);
	return 1;
}

static int
intersection_line (tl_t *tl, tp_t *split, const vec3_t p1, const vec3_t p2,
				   line_t *line)
{
	const vec_t *pn1 = tl->split.plane->normal;
	const vec_t *pp1 = tl->split.point;
	const vec_t *pn2 = split->plane->normal;
	const vec_t *pp2 = split->point;
	vec3_t      pvec;
	vec_t       t;

	// find the line of intersection of the two planes, both direction
	// and a point on the line.
	CrossProduct (pn1, pn2, line->v);
	if (VectorIsZero (line->v))
		//planes are parallel, no intersection
		return 0;
	CrossProduct (pn1, line->v, pvec);
	VectorSubtract (pp2, pp1, line->p);
	t = DotProduct (line->p, pn2) / DotProduct (pvec, pn2);
	VectorMultAdd (pp1, t, line->p, line->p);
	return 1;
}

static int
traceline (int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2,
		   tl_t *tl)
{
	dclipnode_t *node;
	mplane_t   *plane;
	float       t1, t2, frac, frac2, midf, offset;
	int         side, cross;
	vec3_t      mid;
	int         c1, c2;

	tp_t        split, save;
	line_t      line;
	vec3_t      dist, vert, vert2, point;
	int         check_intersection;

	do {
		// Skip past non-intersecting nodes
//		Sys_Printf ("%d\n", num);
		while (num >= 0) {
			node = tl->hull.clipnodes + num;
			plane = tl->hull.planes + node->planenum;

			t1 = PlaneDiff (p1, plane);
			t2 = PlaneDiff (p2, plane);
			offset = calc_offset (tl, plane);

			if (t1 >= offset && t2 >= offset) {
				num = node->children[side = 0];
			} else if (t1 < -offset && t2 < -offset) {
				num = node->children[side = 1];
			} else {
				break;
			}
//			Sys_Printf ("%d\n", num);
		}
		if (num < 0) {
			return num;
		}

		split.plane = plane;
		split.side = t1 < t2;
		VectorSubtract (p2, p1, dist);
		VectorMultAdd (p1, t1 / (t1 - t2), dist, split.point);
		if (tl->split.plane) {
			select_vertex (tl, &split, vert);
			if ((t1 >= -offset && t1 < offset)
				&& (t2 < -offset || t2 >= offset)) {
				// p1 straddles the plane, p2 is clear of the plane
				intersection_point (tl->split.plane, p1, p2, vert, point);
				if ((PlaneDiff (point, plane) < 0) != (t1 < t2)) {
					// the trace misses the intersection of the two planes, so
					// both ends of the trace are really on the same side of
					// the plane
					num = node->children[!(t1 < t2)];
					continue;
				}
			}
		}
		break;
	} while (1);

	check_intersection = 0;
	if (tl->split.plane) {
		VectorNegate (vert, vert2);
		intersection_point (tl->split.plane, p1, p2, vert2, point);
		if ((PlaneDiff (point, plane) < 0) != (t1 < t2)) {
			// the two edges of the hypercube of motion are on opposite sides
			// of the line of intersection of the two planes, so the box hits
			// the intersection somewhere
			check_intersection = 1;
		}
	}
	cross = !(t1 >= -offset && t1 < offset && t2 >= -offset && t2 < offset);

	if (t1 < t2) {
		side = 1;
		frac  = (t1 - offset) / (t1 - t2);
		frac2 = (t1 + offset) / (t1 - t2);
	} else {
		side = 0;
		frac  = (t1 + offset) / (t1 - t2);
		frac2 = (t1 - offset) / (t1 - t2);
	}

	if (check_intersection) {
		intersection_line (tl, &split, p1, p2, &line);
	}

	set_split (tl, &split, &save);
	if (cross) {
		frac = bound (0, frac, 1);
		midf = p1f + (p2f - p1f) * frac;
		VectorMultAdd (p1, frac, dist, mid);
		c1 = c2 = traceline (node->children[side], p1f, midf, p1, mid, tl);

		frac2 = bound (0, frac2, 1);
		midf = p1f + (p2f - p1f) * frac2;
		if (!tl->impact.plane || midf < tl->fraction) {
			VectorMultAdd (p1, frac2, dist, mid);
			c2 = traceline (node->children[side ^ 1], midf, p2f, mid, p2, tl);
		}
	} else {
		c1 = c2 = traceline (node->children[side], p1f, p2f, p1, mid, tl);
		if (c1 != CONTENTS_SOLID)
			c2 = traceline (node->children[side ^ 1], p1f, p2f, mid, p2, tl);
		if (c1 == CONTENTS_SOLID || c2 == CONTENTS_SOLID)
			c1 = c2 = CONTENTS_SOLID;
		else
			c1 = c2 = min (c1, c2);
	}
	set_split (tl, &save, 0);
	if (cross) {
		if (c1 != CONTENTS_SOLID && c2 == CONTENTS_SOLID)
			impact (tl, &split);
		if (c1 == CONTENTS_SOLID && !(tl->flags & SOLID))
			tl->flags |= STARTSOLID;
		if (c1 == CONTENTS_EMPTY || c2 == CONTENTS_EMPTY) {
			tl->flags &= ~ALLSOLID;
			tl->flags |= INOPEN;
		}
		if (c1 < CONTENTS_SOLID || c2 < CONTENTS_SOLID) {
			tl->flags &= ~ALLSOLID;
			tl->flags |= INWATER;
		}
		return c1;
		//return frac2 ? c1 : c2;
	} else {
		if (c1 == CONTENTS_SOLID || c2 == CONTENTS_SOLID) {
			if (!p1f) {
				tl->flags &= SOLID;
				tl->flags |= STARTSOLID;
			}
			return CONTENTS_SOLID;
		}
		if (c1 == CONTENTS_EMPTY && c2 == CONTENTS_EMPTY) {
			tl->flags &= ~ALLSOLID;
			tl->flags |= INOPEN;
		} else {
			tl->flags &= ~ALLSOLID;
			tl->flags |= INWATER;
		}
		return min (c1, c2); //FIXME correct?
	}
}

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	tl_t        tl;
	int         c;

	tl.start = start_point;
	tl.end = end_point;
	tl.extents = trace->extents;
	tl.isbox = trace->isbox;
	tl.hull = *hull;
	set_split (&tl, 0, 0);
	set_impact (&tl, 0, 0);
	tl.fraction = 1;
	tl.flags = ALLSOLID;
	c = traceline (num, 0, 1, start_point, end_point, &tl);
	if (c == CONTENTS_EMPTY) {
		tl.flags &= ~ALLSOLID;
		tl.flags |= INOPEN;
	}
	if (c < CONTENTS_SOLID) {
		tl.flags &= ~ALLSOLID;
		tl.flags |= INOPEN;
	}
	if (tl.fraction < 1) {
		calc_impact (trace, start_point, end_point, tl.impact.plane);
	}
	trace->allsolid = (tl.flags & ALLSOLID) != 0;
	trace->startsolid = (tl.flags & STARTSOLID) != 0;
	trace->inopen = (tl.flags & INOPEN) != 0;
	trace->inwater = (tl.flags & INWATER) != 0;
}
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
#endif
