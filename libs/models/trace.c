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
	int         i;
	int         planenum;
	plane_t     cutplane;
	vec_t       offset, dist, v_n;
	vec_t      *point, *edge;

	planenum = plane - hull->planes;
	cutplane.type = 3;	// generic plane
	v_n = DotProduct (vel, plane->normal);
	if (!v_n) {
		//FIXME is this correct? The assumption is that if we got to a leaf
		//travelliing parallel to its plane, then we have to be in the leaf
		return 1;
	}

	for (portal = leaf->portals; portal; portal = portal->next[side]) {
		side = portal->leafs[1] == leaf;
		if (portal->planenum != planenum)
			continue;
		for (i = 0; i < portal->winding->numpoints; i++) {
			point = portal->winding->points[i];
			edge = portal->edges->points[i];
			// so long as the plane distance and offset are calculated using
			// the  same normal vector, the normal vector length does not
			// matter.
			CrossProduct (vel, edge, cutplane.normal);
			cutplane.dist = DotProduct (cutplane.normal, point);
			dist = PlaneDiff (org, &cutplane);
			offset = calc_offset (trace, &cutplane);
			if (v_n > 0) {
				if (dist >= offset)
					break;
			} else  {
				if (dist <= -offset)
					break;
			}
		}
		if (i == portal->winding->numpoints)
			return 1;
	}
	return 0;
}

typedef struct {
	plane_t     planes[3];
	winding_t   points[3];
	winding_t   edges[3];
	clipport_t  portals[3];
} clipbox_t;

/** Initialize the clipbox representing the faces that might hit the portal.

	When a box is moving, at most three of its faces might hit the portal.
	Those faces all face such that the dot product of their normal and the
	box's velocity is positive (ie, n.v > 0). When the motion is in an axial
	plane, only two faces might hit. For axial motion, only one face.

	The faces are set up such that the first point of each winding is the
	leading corner of the box, and the edges are set up such that testing
	only the first three edges of each winding covers all nine edges (ie,
	the forth edge of one face is the reverse of the first edge of the next
	face).

	The windings are clockwise when looking down the face's normal (ie, when
	looking at the front of the face).
*/
static void
init_box (const trace_t *trace, clipbox_t *box, const vec3_t vel)
{
	vec3_t      p;
	int         u[3];
	int         i, j, k;
	vec_t       s[2] = {1, -1};

	//FIXME rotated box
	for (i = 0; i < 3; i++)
		u[i] = (vel[i] >= 0 ? 1 : -1);
	Vector3Scale (u, trace->extents, p);
	for (i = 0; i < 3; i++) {
		box->portals[i].planenum = i;
		box->portals[i].next[0] = 0;
		box->portals[i].next[0] = 0;
		box->portals[i].leafs[0] = 0;
		box->portals[i].leafs[0] = 0;
		box->portals[i].winding = &box->points[i];
		box->portals[i].edges = &box->edges[i];

		VectorZero (box->planes[i].normal);
		box->planes[i].normal[i] = u[i];
		box->planes[i].dist = DotProduct (p, box->planes[i].normal);
		box->planes[i].type = i;

		box->points[i].numpoints = 4;
		box->edges[i].numpoints = 4;
		VectorCopy (u, box->points[i].points[0]);
		VectorZero (box->edges[i].points[0]);
		j = (i + (u[0]*u[1]*u[2] + 3) / 2) % 3;
		box->edges[i].points[0][j] = -u[j];
		for (j = 1; j < 4; j++) {
			box->points[i].points[j][i] = box->points[i].points[j - 1][i];
			box->edges[i].points[j][i] = box->edges[i].points[j - 1][i];
			for (k = 0; k < 2; k++) {
				int         a, b;
				a = (i + 1 + k) % 3;
				b = (i + 2 - k) % 3;
				box->points[i].points[j][a]
					= s[k] * u[i] * box->points[i].points[j - 1][b];
				box->edges[i].points[j][a]
					= s[k] * u[i] * box->edges[i].points[j - 1][b];
			}
			Vector3Scale (box->points[i].points[j - 1], trace->extents,
						  box->points[i].points[j - 1]);
			Vector3Scale (box->edges[i].points[j - 1], trace->extents,
						  box->edges[i].points[j - 1]);
			VectorScale (box->edges[i].points[j - 1], 2,
						 box->edges[i].points[j - 1]);
		}
		Vector3Scale (box->points[i].points[3], trace->extents,
					  box->points[i].points[3]);
		Vector3Scale (box->edges[i].points[3], trace->extents,
					  box->edges[i].points[3]);
		VectorScale (box->edges[i].points[3], 2,
					 box->edges[i].points[3]);
	}
}

static vec_t
edge_portal_dist (const plane_t *plane, const clipport_t *portal,
				  const vec3_t p1, const vec3_t p2, const vec3_t vel)
{
	int         i;
	winding_t  *winding = portal->winding;
	winding_t  *edges = portal->edges;

	// check for edge point hitting portal face
	{
		vec_t       t1, t2, vn;

		vn = DotProduct (vel, plane->normal);
		t1 = PlaneDiff (p1, plane);
		t2 = PlaneDiff (p2, plane);
		// ensure p1 is the closest point to the plane
		if ((0 < t2 && t2 < t1)
				   || (0 > t2 && t2 > t1)) {
			// p2 is closer to the plane, so swap the points and times
			const vec_t *r = p2;
			vec_t       t = t2;
			t2 = t1; t1 = t;
			p2 = p1; p1 = r;
		}
		if (vn * t1 > 0) {
			// the edge is travelling away from the portal's plane
			return 1;
		}
		// if t1 * t2 < 0, the points straddle the portal's plane and the
		// nearest point test can be skipped
		if (vn && t1 * t2 > 0) {		//FIXME epsilon
			vec3_t      x, c;
			vec3_t      imp;

			t1 /= vn;
			if (t1 <= -1) {
				// the edge doesn't make it as far as the portal's plane
				return 1;
			}
			VectorMultSub (p1, t1, vel, imp);
			for (i = 0; i < winding->numpoints; i++) {
				VectorSubtract (imp, winding->points[i], x);
				CrossProduct (x, edges->points[i], c);
				if (DotProduct (c, plane->normal) < 0)
					break;		// miss
			}
			if (i == winding->numpoints) {
				// the closer end of the edge hit the portal, so -t1 is the
				// fraction
				return -t1;
			}
			// the closer end of the edge missed the portal, check the farther
			// end, but only with this portal edge.
			VectorMultSub (p2, t2 / vn, vel, imp);
			VectorSubtract (imp, winding->points[i], x);
			CrossProduct (x, edges->points[i], c);
			if (DotProduct (c, plane->normal) < 0) {
				// both impacts are on the outside of this portal edge, so the
				// edge being tested misses the portal
				return 1;
			}
			// the two impact points are on both sides of a portal edge, so the
			// edge being tested might hit a portal edge rather than the portal
			// face
		}
	}
	{
		vec3_t      e;
		vec_t       frac = 1.0;
		plane_t     ep;

		// set up the plane through which the edge travels
		VectorSubtract (p2, p1, e);
		CrossProduct (e, vel, ep.normal);
		ep.dist = DotProduct (p1, ep.normal);
		ep.type = 3;
		for (i = 0; i < winding->numpoints; i++) {
			vec_t       t, vn;
			const vec_t *r = winding->points[i];
			const vec_t *v = edges->points[i];
			vec3_t      x, y;

			vn = DotProduct (v, ep.normal);
			if (!vn)	// FIXME epsilon
				continue;	// portal edge is parallel to the plane
			t = PlaneDiff (r, &ep) / vn;
			if (t < -1 || t > 0)
				continue;	// portal edge does not reach the plane
			// impact point of portal edge with the plane
			VectorMultSub (r, t, v, x);
			// project the impact point back to the edge along the velocity
			VectorSubtract (x, p1, y);
			t = DotProduct (y, vel) / DotProduct (vel, vel);
			VectorMultSub (x, t, vel, y);
			VectorSubtract (y, p1, y);
			t = DotProduct (y, y) / DotProduct (e, y);
			if (t < 0 || t > 1)
				continue;	// the edge misses the portal edge
			VectorMultAdd (p1, t, e, y);
			VectorSubtract (x, y, y);
			t = DotProduct (y, vel) / DotProduct (vel, vel);
			if (t >= frac)
				continue;	// this is not the nearest edge pair
			// the edges hit, and they are the closes edge pair so far
			frac = t;
		}
		return frac;
	}
}

static vec_t
box_portal_dist (const hull_t *hull, const clipport_t *portal,
				 const clipbox_t *box, const vec3_t start, const vec3_t vel)
{
	vec3_t      nvel;
	plane_t    *plane = hull->planes + portal->planenum;
	int         i, j;
	vec_t       frac, t;
	vec3_t      p1, p2;

	frac = 1.0;
	for (i = 0; i < 3; i++) {
		// all faces on box have 4 points (and edges), but we need test only
		// three on each face
		for (j = 0; j < 3; j++) {
			VectorAdd (box->points[i].points[j], start, p1);
			VectorAdd (box->points[i].points[j + 1], start, p2);
			t = edge_portal_dist (plane, portal, p1, p2, vel);
			if (t < frac)
				frac = t;
		}
	}

	VectorNegate (vel, nvel);
	for (i = 0; i < portal->winding->numpoints; i++) {
		j = i + 1;
		if (j >= portal->winding->numpoints)
			j -= portal->winding->numpoints;
		VectorSubtract (portal->winding->points[i], start, p1);
		VectorSubtract (portal->winding->points[j], start, p2);
		for (j = 0; j < 3; j++) {
			const clipport_t *p = &box->portals[j];
			t = edge_portal_dist (box->planes + p->planenum, p, p1, p2, nvel);
			if (t < frac)
				frac = t;
		}
	}
	return frac;
}

static inline void
calc_impact (hull_t *hull, trace_t *trace,
			 const vec3_t start, const vec3_t end,
			 clipleaf_t *leaf, plane_t *plane)
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

	if (leaf && trace->type != tr_point) {
		int         i;
		int         side;
		int         planenum;
		clipport_t *portal;
		vec3_t      impact;
		clipbox_t   box;

		planenum = plane - hull->planes;

		VectorSubtract (end, start, dist);
		VectorMultAdd (start, frac, dist, impact);
		if (DotProduct (dist, plane->normal) > 0)
			VectorMultAdd (impact, offset, plane->normal, impact);
		else
			VectorMultSub (impact, offset, plane->normal, impact);

		init_box (trace, &box, dist);

		for (portal = leaf->portals; portal; portal = portal->next[side]) {
			side = portal->leafs[1] == leaf;
			if (portal->planenum != planenum)
				continue;
			for (i = 0; i < portal->winding->numpoints; i++) {
				vec3_t      r, s;
				VectorSubtract (impact, portal->winding->points[i], r);
				CrossProduct (r, portal->edges->points[i], s);
				if (DotProduct (s, plane->normal) > 0)
					continue;	// "hit"
				break;	// "miss";
			}
			if (i == portal->winding->numpoints)
				goto finish_impact;	// impact with the face of the polygon
		}
		frac = 1.0;
		for (portal = leaf->portals; portal; portal = portal->next[side]) {
			vec_t       t;

			side = portal->leafs[1] == leaf;
			if (portal->planenum != planenum)
				continue;
			t = box_portal_dist (hull, portal, &box, start, dist);
			if (t < frac)
				frac = t;
		}
	}
finish_impact:
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

static int
trace_contents (hull_t *hull, trace_t *trace, clipleaf_t *leaf,
				const vec3_t origin)
{
	clipport_t *portal;
	int         side;
	int         contents = leaf->contents;
	// set the auxiliary contents data. this is a bit field of all contents
	// types contained within the trace.
	// contents start at -1 (empty). bit 0 represents CONTENTS_EMPTY
	trace->contents |= 1 << (~contents);
	// check all adjoining leafs that contain part of the trace
	for (portal = leaf->portals; portal; portal = portal->next[side]) {
		vec_t       offset;
		vec_t       dist;
		plane_t    *plane;
		int         res;

		side = portal->leafs[1] == leaf;
		plane = hull->planes + portal->planenum;

		dist = PlaneDiff (origin, plane);
		offset = calc_offset (trace, plane);
		// the side of the plane on which we are does not matter, only
		// whether we're crossing the plane. merely touching the plane does
		// not cause us to cross it
		if (fabs (dist) >= offset)
			continue;
		//FIXME test portal!
		res = trace_contents (hull, trace, portal->leafs[side ^ 1], origin);
		//FIXME better test?
		// solid > lava > slime > water > empty (desired)
		// solid > current (good)
		// problem is, current > sky > lava (what is best?)
		if (res == CONTENTS_SOLID
			|| (contents != CONTENTS_SOLID && res < contents))
			contents = res;
	}
	return contents;
}

VISIBLE void
MOD_TraceLine (hull_t *hull, int num,
			   const vec3_t start_point, const vec3_t end_point,
			   trace_t *trace)
{
	vec_t       start_dist, end_dist, frac[2], offset;
	vec3_t      start, end, dist, vel;
	int         side;
	qboolean    seen_empty, seen_solid, moved;
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
	seen_empty = false;
	seen_solid = false;
	moved = false;
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
					if (moved)
						return;
				} else {
					// crossing from an empty leaf to a solid leaf: the trace
					// has collided.
					calc_impact (hull, trace, start_point, end_point, leaf,
								 split_plane);
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
			moved = tstack->start_frac > 0;

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
		//non-zero offset lets the object touch the plane but still be
		//behind the plane. however, point traces are assumed to be on the
		//front side of the plane if touching the plane
		if ((offset && start_dist <= -offset && end_dist <= -offset)
			|| (!offset && start_dist < -offset && end_dist < -offset)) {
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

VISIBLE int
MOD_HullContents (hull_t *hull, int num, const vec3_t origin, trace_t *trace)
{
	int         prevnode = -1;
	int         side = 0;
	clipleaf_t *leaf;
	// follow origin down the bsp tree to find the "central" leaf
	while (num >= 0) {
		vec_t       d;
		mclipnode_t *node;
		plane_t    *plane;

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		d = PlaneDiff (origin, plane);
		prevnode = num;
		side = d < 0;
		num = node->children[side];
	}
	if (!trace || trace->type == tr_point
		|| prevnode == -1 || !hull->nodeleafs) {
		return num;
	}
	// check the contents of the "central" and surrounding touched leafs
	leaf = hull->nodeleafs[prevnode].leafs[side];
	trace->contents = 0;
	return trace_contents (hull, trace, leaf, origin);
}
