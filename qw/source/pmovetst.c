/*
	pmovetst.c

	(description)

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

#include "QF/model.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/pmove.h"
#include "world.h"

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static plane_t box_planes[6];


/*
	PM_InitBoxHull

	Set up the planes and clipnodes so that the six floats of a bounding box
	can just be stored out and get a proper hull_t structure.
*/
void
PM_InitBoxHull (void)
{
	int         side, i;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;
	box_hull.depth = 6;

	for (i = 0; i < 6; i++) {
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side ^ 1] = i + 1;
		else
			box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1;
	}

}

/*
	PM_HullForBox

	To keep everything totally uniform, bounding boxes are turned into small
	BSP trees instead of being compared directly.
*/
static inline hull_t *
PM_HullForBox (const vec3_t mins, const vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

inline int
PM_HullPointContents (hull_t *hull, int num, const vec3_t p)
{
	float        d;
	dclipnode_t *node;
	plane_t     *plane;

	while (num >= 0) {
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

int
PM_PointContents (const vec3_t p)
{
	float       d;
	int         num;
	dclipnode_t *node;
	hull_t     *hull;
	plane_t    *plane;

	hull = &pmove.physents[0].model->brush.hulls[0];

	num = hull->firstclipnode;

	while (num >= 0) {
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

/*
	PM_TestPlayerPosition

	Returns false if the given player position is not valid (in solid)
*/
qboolean
PM_TestPlayerPosition (const vec3_t pos)
{
	hull_t     *hull;
	int         i;
	physent_t  *pe;
	vec3_t      mins, maxs, test;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->model)
			hull = &pmove.physents[i].model->brush.hulls[1];
		else {
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = PM_HullForBox (mins, maxs);
		}

		VectorSubtract (pos, pe->origin, test);

		if (PM_HullPointContents (hull, hull->firstclipnode, test) ==
			CONTENTS_SOLID) return false;
	}

	return true;
}

static inline int
bboxes_touch (const vec3_t min1, const vec3_t max1,
			  const vec3_t min2, const vec3_t max2)
{
	if (min1[0] > max2[0] || max1[0] < min2[0])
		return 0;
	if (min1[1] > max2[1] || max1[1] < min2[1])
		return 0;
	if (min1[2] > max2[2] || max1[2] < min2[2])
		return 0;
	return 1;
}

/* PM_PlayerMove */
trace_t
PM_PlayerMove (const vec3_t start, const vec3_t end)
{
	hull_t     *hull;
	int         i, check_box, move_missed;
	physent_t  *pe;
	trace_t     trace, total;
	vec3_t      maxs, mins, offset, start_l, end_l;
	vec3_t      move[2];
	vec3_t      forward, right, up;
	int         rot = 0;
	vec3_t      temp;

	// fill in a default trace
	memset (&total, 0, sizeof (trace_t));

	total.fraction = 1;
	total.ent = 0;
	VectorCopy (end, total.endpos);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->hull) {
			hull = pe->hull;
			check_box = 0;
			VectorZero (mins);
			VectorZero (maxs);
		} else {
			check_box = 1;
			if (pe->model) {
				hull = &pe->model->brush.hulls[1];
				VectorSubtract (pe->model->mins, player_maxs, mins);
				VectorSubtract (pe->model->maxs, player_mins, maxs);
			} else {
				VectorSubtract (pe->mins, player_maxs, mins);
				VectorSubtract (pe->maxs, player_mins, maxs);
				hull = PM_HullForBox (mins, maxs);
			}
		}

		// PM_HullForEntity (ent, mins, maxs, offset);
		VectorCopy (pe->origin, offset);

		VectorSubtract (start, offset, start_l);
		VectorSubtract (end, offset, end_l);

		rot = 0;
		if (i && pe->model && pe->model->type == mod_brush
			&& !VectorIsZero (pe->angles)) {
			rot = 1;
			AngleVectors (pe->angles, forward, right, up);
			VectorNegate (right, right);    // convert lhs to rhs

			VectorCopy (start_l, temp);
			start_l[0] = DotProduct (temp, forward);
			start_l[1] = DotProduct (temp, right);
			start_l[2] = DotProduct (temp, up);

			VectorCopy (end_l, temp);
			end_l[0] = DotProduct (temp, forward);
			end_l[1] = DotProduct (temp, right);
			end_l[2] = DotProduct (temp, up);
		}

		// fill in a default trace
		memset (&trace, 0, sizeof (trace_t));

		trace.fraction = 1;
		trace.allsolid = true;
//		trace.startsolid = true;
		VectorCopy (end, trace.endpos);

		move_missed = 0;
		if (check_box) {
			move[0][0] = min (start_l[0], end_l[0]);
			move[0][1] = min (start_l[1], end_l[1]);
			move[0][2] = min (start_l[2], end_l[2]);
			move[1][0] = max (start_l[0], end_l[0]);
			move[1][1] = max (start_l[1], end_l[1]);
			move[1][2] = max (start_l[2], end_l[2]);
			if (!bboxes_touch (move[0], move[1], mins, maxs)) {
				move_missed = 1;
				if (PM_HullPointContents (hull, hull->firstclipnode,
										  start_l) != CONTENTS_SOLID) {
					// since the move missed the entity entirely, the start
					// point is outside the entity and the entity's outside
					// is not solid, the whole trace is not solid
					trace.allsolid = false;
				}
			}
		}

		if (!move_missed) {
			// trace a line through the appropriate clipping hull
			MOD_TraceLine (hull, hull->firstclipnode, start_l, end_l, &trace);
		}

		if (trace.allsolid)
			trace.startsolid = true;
		if (trace.startsolid)
			trace.fraction = 0;

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			// fix up trace by the offset
			if (rot) {
				vec_t       t;

				// transpose the rotation matrix to get its inverse
				t = forward[1]; forward[1] = right[0]; right[0] = t;
				t = forward[2]; forward[2] = up[0]; up[0] = t;
				t = right[2]; right[2] = up[1]; up[1] = t;

				VectorCopy (trace.endpos, temp);
				trace.endpos[0] = DotProduct (temp, forward);
				trace.endpos[1] = DotProduct (temp, right);
				trace.endpos[2] = DotProduct (temp, up);

				VectorCopy (trace.plane.normal, temp);
				trace.plane.normal[0] = DotProduct (temp, forward);
				trace.plane.normal[1] = DotProduct (temp, right);
				trace.plane.normal[2] = DotProduct (temp, up);
			}
			VectorAdd (trace.endpos, offset, trace.endpos);
			total = trace;
			total.ent = (struct edict_s *) &pmove.physents[i];
		}

	}

	return total;
}
