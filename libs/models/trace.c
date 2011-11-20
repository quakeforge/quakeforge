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
#include "QF/winding.h"

#include "compat.h"
#include "world.h"

/* LINE TESTING IN HULLS */

// 1/32 epsilon to keep floating point happy
#ifndef DIST_EPSILON
#define	DIST_EPSILON	(0.03125)
#endif

typedef struct {
	vec3_t     start;
	vec3_t     end;
	vec_t      start_frac;
	int        side;
	int        num;
	plane_t   *plane;
} tracestack_t;

static inline float
calc_offset (trace_t *trace, plane_t *plane)
{
	vec_t       d = 0;
	vec3_t      Rn;

	switch (trace->type) {
		case tr_point:
			break;
		case tr_box:
			if (plane->type < 3)
				d = trace->extents[plane->type];
			else
				d = (fabs (trace->extents[0] * plane->normal[0])
					 + fabs (trace->extents[1] * plane->normal[1])
					 + fabs (trace->extents[2] * plane->normal[2]));
			break;
		case tr_ellipsoid:
			VectorSet (trace->extents[0] * plane->normal[0],
					   trace->extents[1] * plane->normal[1],
					   trace->extents[2] * plane->normal[2], Rn);
			d = sqrt(DotProduct (Rn, Rn));	//FIXME no sqrt
			break;
	}
	return d;
}

static int
check_in_leaf (hull_t *hull, trace_t *trace, clipleaf_t *leaf, plane_t *plane,
			   const vec3_t vel, const vec3_t org)
{
	clipport_t *portal;
	int         side;
	int         miss = 0;
	int         i;
	int         planenum;
	plane_t     cutplane;
	vec_t       offset, dist, v_n;
	vec_t      *point, *edge;

	planenum = plane - hull->planes;
	cutplane.type = 3;	// generic plane
	v_n = DotProduct (vel, plane->normal);

	for (portal = leaf->portals; portal; portal = portal->next[side]) {
		side = portal->leafs[1] == leaf;
		if (portal->planenum != planenum)
			continue;
		for (i = 0; !miss && i < portal->winding->numpoints; i++) {
			point = portal->winding->points[i];
			edge = portal->edges->points[i];
			// so long as the plane distance and offset are calculated using
			// the  same normal vector, the normal vector length does not
			// matter.
			CrossProduct (vel, edge, cutplane.normal);
			cutplane.dist = DotProduct (cutplane.normal, point);
			dist = PlaneDiff (org, &cutplane);
			offset = calc_offset (trace, &cutplane);
			if (v_n >= 0)
				miss = dist >= offset;
			else
				miss = dist <= -offset;
		}
		if (!miss)
			return 1;
	}
	return 0;
}

static inline void
calc_impact (trace_t *trace, const vec3_t start, const vec3_t end,
			 plane_t *plane)
{
	vec_t       t1, t2, frac, offset;
	vec3_t      dist;

	t1 = PlaneDiff (start, plane);
	t2 = PlaneDiff (end, plane);
	offset = calc_offset (trace, plane);

	if (t1 < 0) {
		frac = (t1 + offset + DIST_EPSILON) / (t1 - t2);
		// invert plane paramterers
	} else {
		frac = (t1 - offset - DIST_EPSILON) / (t1 - t2);
	}
	frac = bound (0, frac, 1);
	if (frac < trace->fraction) {
		trace->fraction = frac;
		VectorSubtract (end, start, dist);
		VectorMultAdd (start, frac, dist, trace->endpos);
		if (t1 < 0) {
			// invert plane paramterers
			VectorNegate (plane->normal, trace->plane.normal);
			trace->plane.dist = -plane->dist;
		} else {
			VectorCopy (plane->normal, trace->plane.normal);
			trace->plane.dist = plane->dist;
		}
	}
}

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	vec_t       start_dist, end_dist, frac[2], offset;
	vec3_t      start, end, dist, vel;
	int         side;
	qboolean    seen_empty, seen_solid;
	tracestack_t *tstack;
	tracestack_t tracestack[256];
	mclipnode_t *node;
	plane_t    *plane, *split_plane;
	clipleaf_t *leaf;

	VectorCopy (start_point, start);
	VectorCopy (end_point, end);
	VectorSubtract (end, start, vel);
	VectorNormalize (vel);

	tstack = tracestack;
	seen_empty = 0;
	seen_solid = 0;
	split_plane = 0;
	leaf = 0;
	plane = 0;

	trace->allsolid = true;
	trace->startsolid = false;
	trace->inopen = false;
	trace->inwater = false;
	trace->fraction = 1.0;

	while (1) {
		while (num < 0) {
			// it's not possible to not be in the leaf when doing a point trace
			// if leaf is null, assume we're in the leaf
			// if plane is null, the trace did not cross the last node plane
			int         in_leaf = 1;
			if (trace->type != tr_point && leaf && split_plane)
				in_leaf = check_in_leaf (hull, trace, leaf, split_plane,
										 vel, start);
			if (in_leaf && num == CONTENTS_SOLID) {
				if (!seen_empty && !seen_solid) {
					// this is the first leaf visited, thus the start leaf
					trace->startsolid = seen_solid = true;
				} else if (!seen_empty && seen_solid) {
					// If crossing from one solid leaf to another, treat the
					// whole trace as solid (this is what id does).
					// However, since allsolid is initialized to true, no need
					// to do anything.
					return;
				} else {
					// crossing from an empty leaf to a solid leaf: the trace
					// has collided.
					calc_impact (trace, start_point, end_point, split_plane);
					if (trace->type == tr_point)
						return;
				}
			} else if (in_leaf) {
				seen_empty = true;
				trace->allsolid = false;
				if (num == CONTENTS_EMPTY)
					trace->inopen = true;
				else
					trace->inwater = true;
			}

			// pop up the stack for a back side
			do {
				if (tstack-- == tracestack) {
					// we've finished.
					return;
				}
			} while (tstack->start_frac > trace->fraction);

			// set the hit point for this plane
			VectorCopy (end, tstack->start);

			// go down the back side
			VectorCopy (tstack->end, end);
			side = tstack->side;
			split_plane = tstack->plane;

			leaf = hull->nodeleafs[tstack->num].leafs[side ^ 1];
			num = hull->clipnodes[tstack->num].children[side ^ 1];
		}

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		start_dist = PlaneDiff (start, plane);
		end_dist = PlaneDiff (end, plane);
		offset = calc_offset (trace, plane);

		if (start_dist >= offset && end_dist >= offset) {
			// entirely in front of the plane
			plane = 0;
			leaf = hull->nodeleafs[num].leafs[0];
			num = node->children[0];
			continue;
		}
		//FIXME non-zero offset lets the object touch the plane but still be
		//behind the plane
		if (start_dist < -offset && end_dist < -offset) {
			// entirely behind the plane
			plane = 0;
			leaf = hull->nodeleafs[num].leafs[1];
			num = node->children[1];
			continue;
		}

		// cross the plane

		if (start_dist == end_dist) {
			// avoid division by zero (non-point clip only)
			// since neither side is forward and we need to check both sides
			// anyway, it doesn't matter which side we start with, so long as
			// the fractions are correct.
			side = 0;
			frac[0] = 1;
			frac[1] = 0;
		} else {
			// choose the side such that we are always moving forward through
			// the map
			side = start_dist < end_dist;
			// if either start or end have the box straddling the plane, then
			// frac will be appropriately clipped to 0 and 1, otherwise, frac
			// will be inside that range
			frac[0] = (start_dist + offset) / (start_dist - end_dist);
			frac[1] = (start_dist - offset) / (start_dist - end_dist);
			frac[0] = bound (0, frac[0], 1);
			frac[1] = bound (0, frac[1], 1);
		}

		VectorSubtract (end, start, dist);

		tstack->num = num;
		tstack->side = side;
		tstack->plane = plane;
		VectorCopy (end, tstack->end);
		VectorMultAdd (start, frac[side ^ 1], dist, tstack->start);
		tstack->start_frac = frac[side ^ 1];
		tstack++;

		VectorMultAdd (start, frac[side], dist, end);

		leaf = hull->nodeleafs[num].leafs[side];
		num = node->children[side];
	}
}
