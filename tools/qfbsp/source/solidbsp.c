/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include <stdlib.h>

#include "QF/sys.h"

#include "brush.h"
#include "csg4.h"
#include "bsp5.h"
#include "draw.h"
#include "solidbsp.h"
#include "surfaces.h"
#include "winding.h"

int         leaffaces;
int         nodefaces;
int         splitnodes;

int         c_solid, c_empty, c_water;

qboolean    usemidsplit;


/*
	FaceSide

	For BSP hueristic
*/
static int
FaceSide (face_t *in, plane_t *split)
{
	int         frontcount, backcount, i;
	vec_t       dot;
	vec_t      *p;
	winding_t  *inp = in->points;

	frontcount = backcount = 0;

	// axial planes are fast
	if (split->type < 3) {
		for (i = 0, p = inp->points[0] + split->type; i < inp->numpoints;
			 i++, p += 3) {
			if (*p > split->dist + ON_EPSILON) {
				if (backcount)
					return SIDE_ON;
				frontcount = 1;
			} else if (*p < split->dist - ON_EPSILON) {
				if (frontcount)
					return SIDE_ON;
				backcount = 1;
			}
		}
	} else {
		// sloping planes take longer
		for (i = 0, p = inp->points[0]; i < inp->numpoints; i++, p += 3) {
			dot = DotProduct (p, split->normal);
			dot -= split->dist;
			if (dot > ON_EPSILON) {
				if (backcount)
					return SIDE_ON;
				frontcount = 1;
			} else if (dot < -ON_EPSILON) {
				if (frontcount)
					return SIDE_ON;
				backcount = 1;
			}
		}
	}

	if (!frontcount)
		return SIDE_BACK;
	if (!backcount)
		return SIDE_FRONT;

	return SIDE_ON;
}

/*
	ChooseMidPlaneFromList

	The clipping hull BSP doesn't worry about avoiding splits
*/
static surface_t *
ChooseMidPlaneFromList (surface_t *surfaces, vec3_t mins, vec3_t maxs)
{
	int         j, l;
	plane_t    *plane;
	surface_t  *p, *bestsurface;
	vec_t       bestvalue, value, dist;

	// pick the plane that splits the least
	bestvalue = 6 * 8192 * 8192;
	bestsurface = NULL;

	for (p = surfaces; p; p = p->next) {
		if (p->onnode)
			continue;

		plane = &planes[p->planenum];

		// check for axis aligned surfaces
		l = plane->type;
		if (l > PLANE_Z)
			continue;

		// calculate the split metric along axis l, smaller values are better
		value = 0;

		dist = plane->dist * plane->normal[l];
		for (j = 0; j < 3; j++) {
			if (j == l) {
				value += (maxs[l] - dist) * (maxs[l] - dist);
				value += (dist - mins[l]) * (dist - mins[l]);
			} else
				value += 2 * (maxs[j] - mins[j]) * (maxs[j] - mins[j]);
		}

		if (value > bestvalue)
			continue;

		// currently the best!
		bestvalue = value;
		bestsurface = p;
	}

	if (!bestsurface) {
		for (p = surfaces; p; p = p->next)
			if (!p->onnode)
				return p;				// first valid surface
		Sys_Error ("ChooseMidPlaneFromList: no valid planes");
	}

	return bestsurface;
}

/*
	ChoosePlaneFromList

	The real BSP hueristic
*/
static surface_t *
ChoosePlaneFromList (surface_t *surfaces, vec3_t mins, vec3_t maxs,
					 qboolean usefloors, qboolean usedetail)
{
	face_t     *f;
	int         j, k, l, ishint;
	plane_t    *plane;
	surface_t  *p, *p2, *bestsurface;
	vec_t       bestvalue, bestdistribution, value, dist;

	// pick the plane that splits the least
	bestvalue = 999999;
	bestsurface = NULL;
	bestdistribution = 9e30;

	for (p = surfaces; p; p = p->next) {
		if (p->onnode)
			continue;

		for (f = p->faces; f; f = f->next)
			if (f->texturenum == TEX_HINT)
				break;
		ishint = f != 0;

		if (p->has_struct && usedetail)
			continue;
		if (!p->has_struct && !usedetail)
			continue;

		plane = &planes[p->planenum];
		k = 0;

		if (!usefloors && plane->normal[2] == 1)
			continue;

		for (p2 = surfaces; p2; p2 = p2->next) {
			if (p2 == p)
				continue;
			if (p2->onnode)
				continue;

			for (f = p2->faces; f; f = f->next) {
				if (FaceSide (f, plane) == SIDE_ON) {
					if (!ishint && f->texturenum == TEX_HINT)
						k += 9999;
					else
						k++;
					if (k >= bestvalue)
						break;
				}

			}
			if (k > bestvalue)
				break;
		}

		if (k > bestvalue)
			continue;

		// if equal numbers, axial planes win, then decide on spatial
		// subdivision

		if (k < bestvalue || (k == bestvalue && plane->type < PLANE_ANYX)) {
			// check for axis aligned surfaces
			l = plane->type;

			if (l <= PLANE_Z) {			// axial aligned
				// calculate the split metric along axis l
				value = 0;

				for (j = 0; j < 3; j++) {
					if (j == l) {
						dist = plane->dist * plane->normal[l];
						value += (maxs[l] - dist) * (maxs[l] - dist);
						value += (dist - mins[l]) * (dist - mins[l]);
					} else
						value += 2 * (maxs[j] - mins[j]) * (maxs[j] - mins[j]);
				}

				if (value > bestdistribution && k == bestvalue)
					continue;
				bestdistribution = value;
			}
			// currently the best!
			bestvalue = k;
			bestsurface = p;
		}
	}

	return bestsurface;
}

/*
	SelectPartition

	Selects a surface from a linked list of surfaces to split the group on
	returns NULL if the surface list can not be divided any more (a leaf)
*/
static surface_t *
SelectPartition (surface_t *surfaces, int *detail)
{
	int         i, j;
	surface_t  *p, *bestsurface;
	vec3_t      mins, maxs;

	*detail = 0;

	// count onnode surfaces
	i = 0;
	bestsurface = NULL;
	for (p = surfaces; p; p = p->next) {
		if (!p->onnode) {
			i++;
			bestsurface = p;
		}
	}

	if (i == 0)
		return NULL;

	if (i == 1) {
		if (!bestsurface->has_struct && !usemidsplit)
			*detail = 1;
		return bestsurface;				// this is a final split
	}

	// calculate a bounding box of the entire surfaceset
	for (i = 0; i < 3; i++) {
		mins[i] = BOGUS_RANGE;
		maxs[i] = -BOGUS_RANGE;
	}

	for (p = surfaces; p; p = p->next) {
		for (j = 0; j < 3; j++) {
			if (p->mins[j] < mins[j])
				mins[j] = p->mins[j];
			if (p->maxs[j] > maxs[j])
				maxs[j] = p->maxs[j];
		}
	}

	if (usemidsplit)					// do fast way for clipping hull
		return ChooseMidPlaneFromList (surfaces, mins, maxs);

	// do slow way to save poly splits for drawing hull
#if 0
	bestsurface = ChoosePlaneFromList (surfaces, mins, maxs, false);
	if (bestsurface)
		return bestsurface;
#endif
	bestsurface = ChoosePlaneFromList (surfaces, mins, maxs, true, false);
	if (bestsurface)
		return bestsurface;
	*detail = 1;
	return ChoosePlaneFromList (surfaces, mins, maxs, true, true);
}

//============================================================================

/*
	CalcSurfaceInfo

	Calculates the bounding box
*/
void
CalcSurfaceInfo (surface_t *surf)
{
	face_t     *f;
	int         i, j;

	if (!surf->faces)
		Sys_Error ("CalcSurfaceInfo: surface without a face");

	// calculate a bounding box
	for (i = 0; i < 3; i++) {
		surf->mins[i] = BOGUS_RANGE;
		surf->maxs[i] = -BOGUS_RANGE;
	}

	for (f = surf->faces; f; f = f->next) {
		winding_t  *fp = f->points;
		if (f->contents[0] >= 0 || f->contents[1] >= 0)
			Sys_Error ("Bad contents");
		for (i = 0; i < fp->numpoints; i++) {
			for (j = 0; j < 3; j++) {
				if (fp->points[i][j] < surf->mins[j])
					surf->mins[j] = fp->points[i][j];
				if (fp->points[i][j] > surf->maxs[j])
					surf->maxs[j] = fp->points[i][j];
			}
		}
	}
}

static void
DividePlane (surface_t *in, plane_t *split, surface_t **front,
			 surface_t **back)
{
	face_t     *facet, *next;
	face_t     *frontlist, *backlist;
	face_t     *frontfrag, *backfrag;
	plane_t    *inplane;
	surface_t  *news;

	int         have[2][2];	// [front|back][detail|struct]

	inplane = &planes[in->planenum];

	// parallel case is easy
	if (_VectorCompare (inplane->normal, split->normal)) {
		// check for exactly on node
		if (inplane->dist == split->dist) {
			// divide the facets to the front and back sides
			news = AllocSurface ();
			*news = *in;

			facet = in->faces;
			in->faces = NULL;
			news->faces = NULL;
			in->onnode = news->onnode = true;
			in->has_detail = in->has_struct = false;

			for (; facet; facet = next) {
				next = facet->next;
				if (facet->planeside == 1) {
					facet->next = news->faces;
					news->faces = facet;

					if (facet->detail)
						news->has_detail = true;
					else
						news->has_struct = true;
				} else {
					facet->next = in->faces;
					in->faces = facet;

					if (facet->detail)
						in->has_detail = true;
					else
						in->has_struct = true;
				}
			}

			if (in->faces)
				*front = in;
			else
				*front = NULL;
			if (news->faces)
				*back = news;
			else
				*back = NULL;
			return;
		}

		if (inplane->dist > split->dist) {
			*front = in;
			*back = NULL;
		} else {
			*front = NULL;
			*back = in;
		}
		return;
	}
	// do a real split.  may still end up entirely on one side
	// OPTIMIZE: use bounding box for fast test
	frontlist = NULL;
	backlist = NULL;

	memset (have, 0, sizeof (have));
	for (facet = in->faces; facet; facet = next) {
		next = facet->next;
		SplitFace (facet, split, &frontfrag, &backfrag);
		if (frontfrag) {
			frontfrag->next = frontlist;
			frontlist = frontfrag;
			have[0][frontfrag->detail] = 1;
		}
		if (backfrag) {
			backfrag->next = backlist;
			backlist = backfrag;
			have[1][backfrag->detail] = 1;
		}
	}

	// if nothing actually got split, just move the in plane
	if (frontlist == NULL) {
		*front = NULL;
		*back = in;
		in->faces = backlist;
		return;
	}
	if (backlist == NULL) {
		*front = in;
		*back = NULL;
		in->faces = frontlist;
		return;
	}

	// stuff got split, so allocate one new plane and reuse in
	news = AllocSurface ();
	*news = *in;
	news->faces = backlist;
	*back = news;

	in->faces = frontlist;
	*front = in;

	in->has_struct = have[0][0];
	in->has_detail = have[0][1];

	news->has_struct = have[1][0];
	news->has_detail = have[1][1];

	// recalc bboxes and flags
	CalcSurfaceInfo (news);
	CalcSurfaceInfo (in);
}

/*
	LinkConvexFaces

	Determines the contents of the leaf and creates the final list of
	original faces that have some fragment inside this leaf
*/
static void
LinkConvexFaces (surface_t *planelist, node_t *leafnode)
{
	face_t     *f, *next;
	int         i, count;
	surface_t  *surf, *pnext;

	leafnode->faces = NULL;
	leafnode->contents = 0;
	leafnode->planenum = -1;

	count = 0;
	for (surf = planelist; surf; surf = surf->next) {
		for (f = surf->faces; f; f = f->next) {
			if (f->texturenum < 0)
				continue;
			count++;
			if (!leafnode->contents)
				leafnode->contents = f->contents[0];
			else if (leafnode->contents != f->contents[0])
				Sys_Error ("Mixed face contents in leafnode");
		}
	}

	if (!leafnode->contents)
		leafnode->contents = CONTENTS_SOLID;

	switch (leafnode->contents) {
		case CONTENTS_EMPTY:
			c_empty++;
			break;
		case CONTENTS_SOLID:
			c_solid++;
			break;
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
		case CONTENTS_SKY:
			c_water++;
			break;
		default:
			Sys_Error ("LinkConvexFaces: bad contents number");
	}

	// write the list of faces, and free the originals
	leaffaces += count;
	leafnode->markfaces = malloc (sizeof (face_t *) * (count + 1));
	i = 0;
	for (surf = planelist; surf; surf = pnext) {
		pnext = surf->next;
		for (f = surf->faces; f; f = next) {
			next = f->next;
			if (f->texturenum >= 0) {
				leafnode->markfaces[i] = f->original;
				i++;
			}
			FreeFace (f);
		}
		FreeSurface (surf);
	}
	leafnode->markfaces[i] = NULL;		// sentinal
}

/*
	LinkNodeFaces

	Returns a duplicated list of all faces on surface
*/
static face_t *
LinkNodeFaces (surface_t *surface)
{
	face_t     *list, *new, **prevptr, *f;

	list = NULL;

	// subdivide
	prevptr = &surface->faces;
	while (1) {
		f = *prevptr;
		if (!f)
			break;
		SubdivideFace (f, prevptr);
		f = *prevptr;
		prevptr = &f->next;
	}

	// copy
	for (f = surface->faces; f; f = f->next) {
		nodefaces++;
		new = AllocFace ();
		*new = *f;
		new->points = CopyWinding (f->points);
		f->original = new;
		new->next = list;
		list = new;
	}

	return list;
}

static void
PartitionSurfaces (surface_t *surfaces, node_t *node)
{
	surface_t  *split, *p, *next;
	surface_t  *frontlist, *backlist;
	surface_t  *frontfrag, *backfrag;
	plane_t    *splitplane;

	split = SelectPartition (surfaces, &node->detail);
	if (!split) {						// this is a leaf node
		node->detail = 0;
		node->planenum = PLANENUM_LEAF;
		LinkConvexFaces (surfaces, node);
		return;
	}

	splitnodes++;
	node->faces = LinkNodeFaces (split);
	node->children[0] = AllocNode ();
	node->children[1] = AllocNode ();
	node->planenum = split->planenum;

	splitplane = &planes[split->planenum];

	// multiple surfaces, so split all the polysurfaces into front and back
	// lists
	frontlist = NULL;
	backlist = NULL;

	for (p = surfaces; p; p = next) {
		next = p->next;
		DividePlane (p, splitplane, &frontfrag, &backfrag);
		if (frontfrag && backfrag) {
			// the plane was split, which may expose oportunities to merge
			// adjacent faces into a single face
//			MergePlaneFaces (frontfrag);
//			MergePlaneFaces (backfrag);
		}

		if (frontfrag) {
			if (!frontfrag->faces)
				Sys_Error ("surface with no faces");
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}
		if (backfrag) {
			if (!backfrag->faces)
				Sys_Error ("surface with no faces");
			backfrag->next = backlist;
			backlist = backfrag;
		}
	}

	PartitionSurfaces (frontlist, node->children[0]);
	PartitionSurfaces (backlist, node->children[1]);
}

node_t *
SolidBSP (surface_t *surfhead, qboolean midsplit)
{
	int         i;
	node_t     *headnode;

	qprintf ("----- SolidBSP -----\n");

	headnode = AllocNode ();
	usemidsplit = midsplit;

	// calculate a bounding box for the entire model
	for (i = 0; i < 3; i++) {
		headnode->mins[i] = brushset->mins[i] - SIDESPACE;
		headnode->maxs[i] = brushset->maxs[i] + SIDESPACE;
	}

	// recursively partition everything
	Draw_ClearWindow ();
	splitnodes = 0;
	leaffaces = 0;
	nodefaces = 0;
	c_solid = c_empty = c_water = 0;

	PartitionSurfaces (surfhead, headnode);

	qprintf ("%5i split nodes\n", splitnodes);
	qprintf ("%5i solid leafs\n", c_solid);
	qprintf ("%5i empty leafs\n", c_empty);
	qprintf ("%5i water leafs\n", c_water);
	qprintf ("%5i leaffaces\n", leaffaces);
	qprintf ("%5i nodefaces\n", nodefaces);

	return headnode;
}
