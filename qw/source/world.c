/*
	world.c

	world query functions

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

#include <stdio.h>

#include "QF/clip_hull.h"
#include "QF/console.h"
#include "QF/crc.h"
#include "QF/sys.h"

#include "compat.h"

#include "qw/include/server.h"
#include "qw/include/sv_progs.h"
#include "world.h"

#define always_inline inline __attribute__((__always_inline__))

#define EDICT_LEAFS 32
typedef struct edict_leaf_bucket_s {
	struct edict_leaf_bucket_s *next;
	edict_leaf_t edict_leafs[EDICT_LEAFS];
} edict_leaf_bucket_t;

static edict_leaf_bucket_t *edict_leaf_buckets;
static edict_leaf_bucket_t **edict_leaf_bucket_tail = &edict_leaf_buckets;
static edict_leaf_t *free_edict_leaf_list;

static edict_leaf_t *
alloc_edict_leaf (void)
{
	edict_leaf_bucket_t *bucket;
	edict_leaf_t *el;
	int         i;

	if ((el = free_edict_leaf_list)) {
		free_edict_leaf_list = el->next;
		el->next = 0;
		return el;
	}

	bucket = malloc (sizeof (edict_leaf_bucket_t));
	bucket->next = 0;
	*edict_leaf_bucket_tail = bucket;
	edict_leaf_bucket_tail = &bucket->next;

	for (el = bucket->edict_leafs, i = 0; i < EDICT_LEAFS - 1; i++, el++)
		el->next = el + 1;
	el->next = 0;
	free_edict_leaf_list = bucket->edict_leafs;

	return alloc_edict_leaf ();
}

static void
free_edict_leafs (edict_leaf_t **edict_leafs)
{
	edict_leaf_t **el;

	for (el = edict_leafs; *el; el = &(*el)->next)
		;
	*el = free_edict_leaf_list;
	free_edict_leaf_list = *edict_leafs;
	*edict_leafs = 0;
}

void
SV_FreeAllEdictLeafs (void)
{
	edict_leaf_bucket_t *bucket;
	edict_leaf_t *el;
	int         i;

	for (bucket = edict_leaf_buckets; bucket; bucket = bucket->next) {
		for (el = bucket->edict_leafs, i = 0; i < EDICT_LEAFS - 1; i++, el++)
			el->next = el + 1;
		el->next = bucket->next ? bucket->next->edict_leafs : 0;
	}
	free_edict_leaf_list = 0;
	if (edict_leaf_buckets)
		free_edict_leaf_list = edict_leaf_buckets->edict_leafs;
}

/*
	entities never clip against themselves, or their owner
	line of sight checks trace->crosscontent, but bullets don't
*/

typedef struct {
	vec3_t       boxmins, boxmaxs;		// enclose the test object along
										// entire move
	const float *mins, *maxs;			// size of the moving object
	vec3_t       mins2, maxs2;			// size when clipping against
										// monsters
	const float *start, *end;
	trace_t      trace;
	int          type;
	edict_t     *passedict;
} moveclip_t;

/* HULL BOXES */

static hull_t box_hull;
static mclipnode_t box_clipnodes[6];
static plane_t box_planes[6];


/*
	SV_InitHull SV_InitBoxHull

	Set up the planes and clipnodes so that the six floats of a bounding box
	can just be stored out and get a proper hull_t structure.
*/
void
SV_InitHull (hull_t *hull, mclipnode_t *clipnodes, plane_t *planes)
{
	int			side, i;

	hull->clipnodes = clipnodes;
	hull->planes = planes;
	hull->firstclipnode = 0;
	hull->lastclipnode = 5;
	hull->depth = 6;

	for (i = 0; i < 6; i++) {
		hull->clipnodes[i].planenum = i;

		side = i & 1;

		hull->clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			hull->clipnodes[i].children[side ^ 1] = i + 1;
		else
			hull->clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		hull->planes[i].type = i >> 1;
		hull->planes[i].normal[i >> 1] = 1;
	}
}

static inline void
SV_InitBoxHull (void)
{
	SV_InitHull (&box_hull, box_clipnodes, box_planes);
}

/*
	SV_HullForBox

	To keep everything totally uniform, bounding boxes are turned into small
	BSP trees instead of being compared directly.
*/
static inline hull_t *
SV_HullForBox (const vec3_t mins, const vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

/*
	SV_HullForEntity

	Returns a hull that can be used for testing or clipping an object of
	mins/maxs size.  Offset is filled in to contain the adjustment that
	must be added to the testing object's origin to get a point to use with
	the returned hull.
*/
hull_t *
SV_HullForEntity (edict_t *ent, const vec3_t mins, const vec3_t maxs,
				  vec3_t extents, vec3_t offset)
{
	int         hull_index = 0;
	hull_t     *hull = 0, **hull_list = 0;
	model_t    *model;
	vec3_t      hullmins, hullmaxs, size;

	if ((sv_fields.rotated_bbox != -1
		 && SVinteger (ent, rotated_bbox))
		|| SVfloat (ent, solid) == SOLID_BSP) {
		VectorSubtract (maxs, mins, size);
		if (size[0] < 3)
			hull_index = 0;
		else if (size[0] <= 32)
			hull_index = 1;
		else
			hull_index = 2;
	}
	if (sv_fields.rotated_bbox != -1
		&& SVinteger (ent, rotated_bbox)) {
		int h = SVinteger (ent, rotated_bbox) - 1;
		hull_list = pf_hull_list[h]->hulls;
	} if (SVfloat (ent, solid) == SOLID_BSP) {
		// explicit hulls in the BSP model
		if (SVfloat (ent, movetype) != MOVETYPE_PUSH)
			Sys_Error ("SOLID_BSP without MOVETYPE_PUSH: %d %s",
					   NUM_FOR_EDICT (&sv_pr_state, ent),
					   PR_GetString (&sv_pr_state,
						   			 SVstring (ent, classname)));

		model = sv.models[(int) SVfloat (ent, modelindex)];

		if (!model || model->type != mod_brush)
			Sys_Error ("SOLID_BSP with a non bsp model: %d %s",
					   NUM_FOR_EDICT (&sv_pr_state, ent),
					   PR_GetString (&sv_pr_state,
						   			 SVstring (ent, classname)));

		hull_list = model->brush.hull_list;
	}
	if (hull_list) {
		// decide which clipping hull to use, based on the size
		VectorSubtract (maxs, mins, size);
		if (extents) {
			VectorScale (size, 0.5, extents);
		} else {
			if (size[0] < 3)
				hull_index = 0;
			else if (size[0] <= 32)
				hull_index = 1;
			else
				hull_index = 2;
		}
		hull = hull_list[hull_index];
	}

	if (hull) {
		// calculate an offset value to center the origin
		if (extents) {
			VectorAdd (extents, mins, offset);
			VectorSubtract (SVvector (ent, origin), offset, offset);
		} else {
			VectorSubtract (hull->clip_mins, mins, offset);
			VectorAdd (offset, SVvector (ent, origin), offset);
		}
	} else {
		// create a temp hull from bounding box sizes
		if (extents) {
			VectorCopy (SVvector (ent, mins), hullmins);
			VectorCopy (SVvector (ent, maxs), hullmaxs);

			//FIXME broken for map models (ie, origin always 0, 0, 0)
			VectorAdd (extents, mins, offset);
			VectorSubtract (SVvector (ent, origin), offset, offset);
		} else {
			VectorSubtract (SVvector (ent, mins), maxs, hullmins);
			VectorSubtract (SVvector (ent, maxs), mins, hullmaxs);

			VectorCopy (SVvector (ent, origin), offset);
		}
		hull = SV_HullForBox (hullmins, hullmaxs);
	}

	return hull;
}

/* ENTITY AREA CHECKING */

areanode_t  sv_areanodes[AREA_NODES];
int         sv_numareanodes;

static areanode_t *
SV_CreateAreaNode (int depth, const vec3_t mins, const vec3_t maxs)
{
	areanode_t *anode;
	vec3_t      mins1, maxs1, mins2, maxs2, size;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth + 1, mins1, maxs1);

	return anode;
}

void
SV_ClearWorld (void)
{
	SV_InitBoxHull ();

	memset (sv_areanodes, 0, sizeof (sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->mins, sv.worldmodel->maxs);
}

link_t        **sv_link_next;
link_t        **sv_link_prev;

void
SV_UnlinkEdict (edict_t *ent)
{
	free_edict_leafs (&SVdata (ent)->leafs);
	if (!SVdata (ent)->area.prev)
		return;							// not linked in anywhere
	RemoveLink (&SVdata (ent)->area);
	if (sv_link_next && *sv_link_next == &SVdata (ent)->area)
		*sv_link_next = SVdata (ent)->area.next;
	if (sv_link_prev && *sv_link_prev == &SVdata (ent)->area)
		*sv_link_prev = SVdata (ent)->area.prev;
	SVdata (ent)->area.prev = SVdata (ent)->area.next = NULL;
}

static void
SV_TouchLinks (edict_t *ent, areanode_t *node)
{
	int         old_self, old_other;
	edict_t    *touch;
	link_t     *l, *next;

	// touch linked edicts
	sv_link_next = &next;
	for (l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next) {
		next = l->next;
		touch = EDICT_FROM_AREA (l);
		if (touch == ent)
			continue;
		if (!SVfunc (touch, touch)
			|| SVfloat (touch, solid) != SOLID_TRIGGER)
			continue;
		if (SVvector (ent, absmin)[0] > SVvector (touch, absmax)[0]
			|| SVvector (ent, absmin)[1] > SVvector (touch, absmax)[1]
			|| SVvector (ent, absmin)[2] > SVvector (touch, absmax)[2]
			|| SVvector (ent, absmax)[0] < SVvector (touch, absmin)[0]
			|| SVvector (ent, absmax)[1] < SVvector (touch, absmin)[1]
			|| SVvector (ent, absmax)[2] < SVvector (touch, absmin)[2])
			continue;

		old_self = *sv_globals.self;
		old_other = *sv_globals.other;

		*sv_globals.time = sv.time;
		sv_pr_touch (touch, ent);

		*sv_globals.self = old_self;
		*sv_globals.other = old_other;
	}
	sv_link_next = 0;

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (SVvector (ent, absmax)[node->axis] > node->dist)
		SV_TouchLinks (ent, node->children[0]);
	if (SVvector (ent, absmin)[node->axis] < node->dist)
		SV_TouchLinks (ent, node->children[1]);
}

static void
SV_FindTouchedLeafs (edict_t *ent, mnode_t *node)
{
	int			sides;
	mleaf_t    *leaf;
	plane_t    *splitplane;
	edict_leaf_t *edict_leaf;

	if (node->contents == CONTENTS_SOLID)
		return;

	// add an efrag if the node is a leaf
	if (node->contents < 0) {
		leaf = (mleaf_t *) node;

		edict_leaf = alloc_edict_leaf ();
		edict_leaf->leafnum = leaf - sv.worldmodel->brush.leafs - 1;
		edict_leaf->next = SVdata (ent)->leafs;
		SVdata (ent)->leafs = edict_leaf;
		return;
	}

	// NODE_MIXED
	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE (SVvector (ent, absmin),
							   SVvector (ent, absmax), splitplane);

	// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs (ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs (ent, node->children[1]);
}

void
SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t *node;

	if (SVdata (ent)->area.prev)
		SV_UnlinkEdict (ent);			// unlink from old position

	if (ent == sv.edicts)
		return;							// don't add the world
	if (ent->free)
		return;

	if (SVfloat (ent, solid) == SOLID_BSP
		&& !VectorIsZero (SVvector (ent, angles)) && ent != sv.edicts) {
		float       m, v;
		vec3_t      r;
		m = DotProduct (SVvector (ent, mins), SVvector (ent, mins));
		v = DotProduct (SVvector (ent, maxs), SVvector (ent, maxs));
		if (m < v)
			m = v;
		m = sqrt (m);
		VectorSet (m, m, m, r);
		VectorSubtract (SVvector (ent, origin), r, SVvector (ent, absmin));
		VectorAdd (SVvector (ent, origin), r, SVvector (ent, absmax));
	} else {
		// set the abs box
		VectorAdd (SVvector (ent, origin), SVvector (ent, mins),
				   SVvector (ent, absmin));
		VectorAdd (SVvector (ent, origin), SVvector (ent, maxs),
				   SVvector (ent, absmax));
	}

	// to make items easier to pick up and allow them to be grabbed off
	// of shelves, the abs sizes are expanded
	if ((int) SVfloat (ent, flags) & FL_ITEM) {
		SVvector (ent, absmin)[0] -= 15;
		SVvector (ent, absmin)[1] -= 15;
		SVvector (ent, absmax)[0] += 15;
		SVvector (ent, absmax)[1] += 15;
	} else {	// movement is clipped an epsilon away from actual edge, so we
				// must fully check even when bounding boxes don't quite touch
		SVvector (ent, absmin)[0] -= 1;
		SVvector (ent, absmin)[1] -= 1;
		SVvector (ent, absmin)[2] -= 1;
		SVvector (ent, absmax)[0] += 1;
		SVvector (ent, absmax)[1] += 1;
		SVvector (ent, absmax)[2] += 1;
	}

	// link to PVS leafs
	free_edict_leafs (&SVdata (ent)->leafs);
	if (SVfloat (ent, modelindex))
		SV_FindTouchedLeafs (ent, sv.worldmodel->brush.nodes);

	if (SVfloat (ent, solid) == SOLID_NOT)
		return;

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1) {
		if (node->axis == -1)
			break;
		if (SVvector (ent, absmin)[node->axis] > node->dist)
			node = node->children[0];
		else if (SVvector (ent, absmax)[node->axis] < node->dist)
			node = node->children[1];
		else
			break;						// crosses the node
	}

	// link it in
	if (SVfloat (ent, solid) == SOLID_TRIGGER)
		InsertLinkBefore (&SVdata (ent)->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&SVdata (ent)->area, &node->solid_edicts);

	// if touch_triggers, touch all entities at this node and descend for more
	if (touch_triggers)
		SV_TouchLinks (ent, sv_areanodes);
}

/* POINT TESTING IN HULLS */

int
SV_HullPointContents (hull_t *hull, int num, const vec3_t p)
{
	float        d;
	mclipnode_t *node;
	plane_t     *plane;

	while (num >= 0) {
		//if (num < hull->firstclipnode || num > hull->lastclipnode)
		//	Sys_Error ("SV_HullPointContents: bad node number");

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
SV_PointContents (const vec3_t p)
{
	int         cont;

	cont = SV_HullPointContents (&sv.worldmodel->brush.hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return cont;
}

int
SV_TruePointContents (const vec3_t p)
{
	return SV_HullPointContents (&sv.worldmodel->brush.hulls[0], 0, p);
}

/*
	SV_TestEntityPosition

	This could be a lot more efficient...
	A small wrapper around SV_BoxInSolidEntity that never clips against the
	supplied entity.
*/
edict_t *
SV_TestEntityPosition (edict_t *ent)
{
	trace_t		trace;

	trace =	SV_Move (SVvector (ent, origin),
					 SVvector (ent, mins),
					 SVvector (ent, maxs),
					 SVvector (ent, origin), 0, ent);

	if (trace.startsolid)
		return sv.edicts;
	return NULL;
}

/*
	SV_ClipMoveToEntity

	Handles selection or creation of a clipping hull, and offseting (and
	eventually rotation) of the end points
*/
static trace_t
SV_ClipMoveToEntity (edict_t *touched, const vec3_t start,
					 const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
	hull_t     *hull;
	trace_t     trace;
	vec3_t      offset, start_l, end_l;
	vec3_t      forward, right, up;
	int         rot = 0;
	vec3_t      temp;

	// fill in a default trace
	memset (&trace, 0, sizeof (trace_t));

	trace.fraction = 1;
	trace.allsolid = true;
	trace.type = tr_point;
	VectorCopy (end, trace.endpos);

	// get the clipping hull
	hull = SV_HullForEntity (touched, mins, maxs,
							 trace.type != tr_point ? trace.extents : 0,
							 offset);

	VectorSubtract (start, offset, start_l);
	VectorSubtract (end, offset, end_l);

	if (SVfloat (touched, solid) == SOLID_BSP
		&& !VectorIsZero (SVvector (touched, angles))
		&& touched != sv.edicts) {
		rot = 1;
		AngleVectors (SVvector (touched, angles), forward, right, up);
		VectorNegate (right, right);	// convert lhs to rhs

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	// trace a line through the apropriate clipping hull
	MOD_TraceLine (hull, hull->firstclipnode, start_l, end_l, &trace);

	// fix up trace by the rotation and offset
	if (trace.fraction != 1) {
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
	}

	// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = touched;

	return trace;
}

static always_inline __attribute__((pure)) int
ctl_pretest_everything (edict_t *touch, moveclip_t *clip)
{
	if (touch->free)
		return 0;
	if (!((int) SVfloat (touch, flags) & FL_FINDABLE_NONSOLID)) {
		if (SVfloat (touch, solid) == SOLID_NOT)
			return 0;
		if (SVfloat (touch, solid) == SOLID_TRIGGER)
			return 0;
	}
	if (touch == clip->passedict)
		return 0;
	return 1;
}

static always_inline int
ctl_pretest_triggers (edict_t *touch, moveclip_t *clip)
{
	if (SVfloat (touch, solid) != SOLID_TRIGGER)
		return 0;
	if (!((int) SVfloat (touch, flags) & FL_FINDABLE_NONSOLID))
		return 0;
	if (touch == clip->passedict)
		return 0;
	return 1;
}

static always_inline __attribute__((pure)) int
ctl_pretest_other (edict_t *touch, moveclip_t *clip)
{
	if (SVfloat (touch, solid) == SOLID_NOT)
		return 0;
	if (touch == clip->passedict)
		return 0;
	if (SVfloat (touch, solid) == SOLID_TRIGGER)
		Sys_Error ("Trigger in clipping list");

	if (clip->type == MOVE_NOMONSTERS && SVfloat (touch, solid) != SOLID_BSP)
		return 0;
	return 1;
}

static always_inline int
ctl_pretest_lagged (edict_t *touch, moveclip_t *clip)
{
	if (clip->type & MOVE_LAGGED)
		if ((unsigned) (touch->entnum - 1) < sv.maxlagents)
			if (sv.lagents[touch->entnum - 1].present)
				return 0;
	return 1;
}

static always_inline int
ctl_touch_common (edict_t *touch, moveclip_t *clip)
{
	if (clip->passedict && SVvector (clip->passedict, size)[0]
		&& !SVvector (touch, size)[0])
		return 0;						// points never interact

	// might intersect, so do an exact clip
	if (clip->passedict) {
		if (PROG_TO_EDICT (&sv_pr_state, SVentity (touch, owner))
			== clip->passedict)
			return 0;					// don't clip against own missiles
		if (PROG_TO_EDICT (&sv_pr_state,
						   SVentity (clip->passedict, owner)) == touch)
			return 0;					// don't clip against owner
	}
	return 1;
}

static always_inline int
ctl_touch_test (edict_t *touch, moveclip_t *clip)
{
	if (clip->boxmins[0] > SVvector (touch, absmax)[0]
		|| clip->boxmins[1] > SVvector (touch, absmax)[1]
		|| clip->boxmins[2] > SVvector (touch, absmax)[2]
		|| clip->boxmaxs[0] < SVvector (touch, absmin)[0]
		|| clip->boxmaxs[1] < SVvector (touch, absmin)[1]
		|| clip->boxmaxs[2] < SVvector (touch, absmin)[2])
		return 0;

	return ctl_touch_common (touch, clip);
}

static always_inline int
ctl_touch_test_origin (edict_t *touch, const vec3_t origin, moveclip_t *clip)
{
	if (clip->boxmins[0] > origin[0] + SVvector (touch, maxs)[0]
		|| clip->boxmins[1] > origin[1] + SVvector (touch, maxs)[1]
		|| clip->boxmins[2] > origin[2] + SVvector (touch, maxs)[2]
		|| clip->boxmaxs[0] < origin[0] + SVvector (touch, mins)[0]
		|| clip->boxmaxs[1] < origin[1] + SVvector (touch, mins)[1]
		|| clip->boxmaxs[2] < origin[2] + SVvector (touch, mins)[2])
		return 0;

	return ctl_touch_common (touch, clip);
}

static void
ctl_do_clip (edict_t *touch, moveclip_t *clip, trace_t *trace)
{
	if ((int) SVfloat (touch, flags) & FL_MONSTER)
		*trace = SV_ClipMoveToEntity (touch, clip->start,
									  clip->mins2, clip->maxs2, clip->end);
	else
		*trace = SV_ClipMoveToEntity (touch, clip->start,
									  clip->mins, clip->maxs, clip->end);
	if (trace->allsolid || trace->startsolid
		|| trace->fraction < clip->trace.fraction) {
		trace->ent = touch;
		if (clip->type & MOVE_ENTCHAIN) {
			SVentity (touch, chain) = EDICT_TO_PROG (&sv_pr_state,
													 clip->trace.ent
														? clip->trace.ent
														: sv.edicts);
			clip->trace.ent = touch;
		} else {
			if (clip->trace.startsolid) {
				clip->trace = *trace;
				clip->trace.startsolid = true;
			} else {
				clip->trace = *trace;
			}
		}
	}
}

/*
	SV_ClipToLinks

	Mins and maxs enclose the entire area swept by the move
*/
static void
SV_ClipToLinks (areanode_t *node, moveclip_t *clip)
{
	edict_t    *touch;
	link_t     *l, *next;
	trace_t		trace;

	if (clip->type & MOVE_EVERYTHING) {
		touch = NEXT_EDICT (&sv_pr_state, sv.edicts);
		for (unsigned i = 1; i < sv.num_edicts; i++,
			 touch = NEXT_EDICT (&sv_pr_state, touch)) {
			if (clip->trace.allsolid)
				return;
			if (!ctl_pretest_everything (touch, clip))
				continue;
			if (!ctl_pretest_lagged (touch, clip))
				continue;
			if (!ctl_touch_test (touch, clip))
				continue;
			ctl_do_clip (touch, clip, &trace);
		}
	} else if (clip->type & MOVE_TRIGGERS) {
		for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
			next = l->next;
			touch = EDICT_FROM_AREA (l);

			if (clip->trace.allsolid)
				return;
			if (!ctl_pretest_triggers (touch, clip))
				continue;
			if (!ctl_pretest_lagged (touch, clip))
				continue;
			if (!ctl_touch_test (touch, clip))
				continue;
			ctl_do_clip (touch, clip, &trace);
		}
	} else {
		// touch linked edicts
		for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
			next = l->next;
			touch = EDICT_FROM_AREA (l);

			if (clip->trace.allsolid)
				return;
			if (!ctl_pretest_other (touch, clip))
				continue;
			if (!ctl_pretest_lagged (touch, clip))
				continue;
			if (!ctl_touch_test (touch, clip))
				continue;
			ctl_do_clip (touch, clip, &trace);
		}
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (clip->boxmaxs[node->axis] > node->dist)
		SV_ClipToLinks (node->children[0], clip);
	if (clip->boxmins[node->axis] < node->dist)
		SV_ClipToLinks (node->children[1], clip);
}

static inline void
SV_MoveBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs,
			   const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
	// debug to test against everything
	boxmins[0] = boxmins[1] = boxmins[2] = -9999;
	boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int			i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

trace_t
SV_Move (const vec3_t start, const vec3_t mins, const vec3_t maxs,
		 const vec3_t end, int type, edict_t *passedict)
{
	int			i;
	moveclip_t  clip;

	memset (&clip, 0, sizeof (moveclip_t));

	// clip to world
	clip.trace = SV_ClipMoveToEntity (sv.edicts, start, mins, maxs, end);
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

	if (type == MOVE_MISSILE) {
		for (i = 0; i < 3; i++) {
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	} else {
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}

	// create the bounding box of the entire move
	SV_MoveBounds (start, clip.mins2, clip.maxs2, end, clip.boxmins,
				   clip.boxmaxs);

	// clip to entities
	if (clip.type & MOVE_LAGGED) {
		clip.type &= ~MOVE_LAGGED;
		if (passedict->entnum && passedict->entnum <= MAX_CLIENTS) {
			client_t   *cl = &svs.clients[passedict->entnum - 1];
			clip.type |= MOVE_LAGGED;
			sv.lagents = cl->laggedents;
			sv.maxlagents = cl->laggedents_count;
			sv.lagentsfrac = cl->laggedents_frac;
		} else if (PROG_TO_EDICT (&sv_pr_state, SVentity (passedict, owner))) {
			edict_t    *owner;
			owner = PROG_TO_EDICT (&sv_pr_state, SVentity (passedict, owner));
			if (owner->entnum && owner->entnum <= MAX_CLIENTS) {
				client_t   *cl = &svs.clients[owner->entnum - 1];
				clip.type |= MOVE_LAGGED;
				sv.lagents = cl->laggedents;
				sv.maxlagents = cl->laggedents_count;
				sv.lagentsfrac = cl->laggedents_frac;
			}
		}
	}
	if (clip.type & MOVE_LAGGED) {
		trace_t     trace;
		edict_t    *touch;
		vec3_t      lp;
		unsigned    li;

		SV_ClipToLinks (sv_areanodes, &clip);
		for (li = 0; li < sv.maxlagents; li++) {
			if (!sv.lagents[li].present)
				continue;
			if (clip.trace.allsolid)
				break;

			touch = EDICT_NUM (&sv_pr_state, li + 1);
			if (!ctl_pretest_other (touch, &clip))
				continue;
			VectorBlend (SVvector(touch, origin), sv.lagents[li].laggedpos,
						 sv.lagentsfrac, lp);
			if (!ctl_touch_test_origin (touch, lp, &clip))
				continue;
			ctl_do_clip (touch, &clip, &trace);
		}
	} else {
		SV_ClipToLinks (sv_areanodes, &clip);
	}

	return clip.trace;
}

edict_t *
SV_TestPlayerPosition (edict_t *ent, const vec3_t origin)
{
	edict_t    *check;
	hull_t     *hull;
	vec3_t      boxmins, boxmaxs, offset;

	// check world first
	hull = &sv.worldmodel->brush.hulls[1];
	if (SV_HullPointContents (hull, hull->firstclipnode, origin) !=
		CONTENTS_EMPTY) return sv.edicts;

	// check all entities
	VectorAdd (origin, SVvector (ent, mins), boxmins);
	VectorAdd (origin, SVvector (ent, maxs), boxmaxs);

	check = NEXT_EDICT (&sv_pr_state, sv.edicts);
	for (unsigned e = 1; e < sv.num_edicts;
		 e++, check = NEXT_EDICT (&sv_pr_state, check)) {
		if (check->free)
			continue;
		if (check == ent)
			continue;

		if (SVfloat (check, solid) != SOLID_BSP
			&& SVfloat (check, solid) != SOLID_BBOX
			&& SVfloat (check, solid) != SOLID_SLIDEBOX)
			continue;

		if (boxmins[0] > SVvector (check, absmax)[0]
			|| boxmins[1] > SVvector (check, absmax)[1]
			|| boxmins[2] > SVvector (check, absmax)[2]
			|| boxmaxs[0] < SVvector (check, absmin)[0]
			|| boxmaxs[1] < SVvector (check, absmin)[1]
			|| boxmaxs[2] < SVvector (check, absmin)[2])
			continue;

		// get the clipping hull
		hull = SV_HullForEntity (check, SVvector (ent, mins),
								 SVvector (ent, maxs), 0, offset);

		VectorSubtract (origin, offset, offset);

		// test the point
		if (SV_HullPointContents (hull, hull->firstclipnode, offset) !=
			CONTENTS_EMPTY)
			return check;
	}

	return NULL;
}
