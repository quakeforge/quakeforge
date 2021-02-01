/*
	sw32_r_bsp.c

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

#define NH_DEFINE
#include "namehack.h"

#include <math.h>
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/render.h"
#include "QF/sys.h"

#include "r_internal.h"

// current entity info
qboolean    sw32_insubmodel;
vec3_t      sw32_r_worldmodelorg;
static float       entity_rotation[3][3];

int         sw32_r_currentbkey;

typedef enum { touchessolid, drawnode, nodrawnode } solidstate_t;

#define MAX_BMODEL_VERTS	500			// 6K
#define MAX_BMODEL_EDGES	1000		// 12K

static mvertex_t *pbverts;
static bedge_t *pbedges;
static int  numbverts, numbedges;

int         sw32_numbtofpolys;
static btofpoly_t *pbtofpolys;

static mvertex_t *pfrontenter, *pfrontexit;

static qboolean makeclippededge;


static void
R_EntityRotate (vec3_t vec)
{
	vec3_t      tvec;

	VectorCopy (vec, tvec);
	vec[0] = DotProduct (entity_rotation[0], tvec);
	vec[1] = DotProduct (entity_rotation[1], tvec);
	vec[2] = DotProduct (entity_rotation[2], tvec);
}


void
sw32_R_RotateBmodel (void)
{
	VectorCopy (currententity->transform + 0, entity_rotation[0]);
	VectorCopy (currententity->transform + 4, entity_rotation[1]);
	VectorCopy (currententity->transform + 8, entity_rotation[2]);

	// rotate modelorg and the transformation matrix
	R_EntityRotate (modelorg);
	R_EntityRotate (vpn);
	R_EntityRotate (vright);
	R_EntityRotate (vup);

	sw32_R_TransformFrustum ();
}


static void
R_RecursiveClipBPoly (bedge_t *pedges, mnode_t *pnode, msurface_t *psurf)
{
	bedge_t    *psideedges[2], *pnextedge, *ptedge;
	int         i, side, lastside;
	float       dist, frac, lastdist;
	plane_t    *splitplane, tplane;
	mvertex_t  *pvert, *plastvert, *ptvert;
	mnode_t    *pn;

	psideedges[0] = psideedges[1] = NULL;

	makeclippededge = false;

	// transform the BSP plane into model space
	// FIXME: cache these?
	splitplane = pnode->plane;
	tplane.dist = splitplane->dist -
		DotProduct (r_entorigin, splitplane->normal);
	tplane.normal[0] = DotProduct (entity_rotation[0], splitplane->normal);
	tplane.normal[1] = DotProduct (entity_rotation[1], splitplane->normal);
	tplane.normal[2] = DotProduct (entity_rotation[2], splitplane->normal);

	// clip edges to BSP plane
	for (; pedges; pedges = pnextedge) {
		pnextedge = pedges->pnext;

		// set the status for the last point as the previous point
		// FIXME: cache this stuff somehow?
		plastvert = pedges->v[0];
		lastdist = DotProduct (plastvert->position, tplane.normal) -
			tplane.dist;

		if (lastdist > 0)
			lastside = 0;
		else
			lastside = 1;

		pvert = pedges->v[1];

		dist = DotProduct (pvert->position, tplane.normal) - tplane.dist;

		if (dist > 0)
			side = 0;
		else
			side = 1;

		if (side != lastside) {
			// clipped
			if (numbverts >= MAX_BMODEL_VERTS)
				return;

			// generate the clipped vertex
			frac = lastdist / (lastdist - dist);
			ptvert = &pbverts[numbverts++];
			ptvert->position[0] = plastvert->position[0] +
				frac * (pvert->position[0] - plastvert->position[0]);
			ptvert->position[1] = plastvert->position[1] +
				frac * (pvert->position[1] - plastvert->position[1]);
			ptvert->position[2] = plastvert->position[2] +
				frac * (pvert->position[2] - plastvert->position[2]);

			// split into two edges, one on each side, and remember entering
			// and exiting points
			// FIXME: share the clip edge by having a winding direction flag?
			if (numbedges >= (MAX_BMODEL_EDGES - 1)) {
				Sys_Printf ("Out of edges for bmodel\n");
				return;
			}

			ptedge = &pbedges[numbedges];
			ptedge->pnext = psideedges[lastside];
			psideedges[lastside] = ptedge;
			ptedge->v[0] = plastvert;
			ptedge->v[1] = ptvert;

			ptedge = &pbedges[numbedges + 1];
			ptedge->pnext = psideedges[side];
			psideedges[side] = ptedge;
			ptedge->v[0] = ptvert;
			ptedge->v[1] = pvert;

			numbedges += 2;

			if (side == 0) {
				// entering for front, exiting for back
				pfrontenter = ptvert;
				makeclippededge = true;
			} else {
				pfrontexit = ptvert;
				makeclippededge = true;
			}
		} else {
			// add the edge to the appropriate side
			pedges->pnext = psideedges[side];
			psideedges[side] = pedges;
		}
	}

	// if anything was clipped, reconstitute and add the edges along the clip
	// plane to both sides (but in opposite directions)
	if (makeclippededge) {
		if (numbedges >= (MAX_BMODEL_EDGES - 2)) {
			Sys_Printf ("Out of edges for bmodel\n");
			return;
		}

		ptedge = &pbedges[numbedges];
		ptedge->pnext = psideedges[0];
		psideedges[0] = ptedge;
		ptedge->v[0] = pfrontexit;
		ptedge->v[1] = pfrontenter;

		ptedge = &pbedges[numbedges + 1];
		ptedge->pnext = psideedges[1];
		psideedges[1] = ptedge;
		ptedge->v[0] = pfrontenter;
		ptedge->v[1] = pfrontexit;

		numbedges += 2;
	}
	// draw or recurse further
	for (i = 0; i < 2; i++) {
		if (psideedges[i]) {
			// draw if we've reached a non-solid leaf, done if all that's left
			// is a solid leaf, and continue down the tree if it's not a leaf
			pn = pnode->children[i];

			// we're done with this branch if the node or leaf isn't in the PVS
			if (pn->visframe == r_visframecount) {
				if (pn->contents < 0) {
					if (pn->contents != CONTENTS_SOLID) {
						sw32_r_currentbkey = ((mleaf_t *) pn)->key;
						sw32_R_RenderBmodelFace (psideedges[i], psurf);
					}
				} else {
					R_RecursiveClipBPoly (psideedges[i], pnode->children[i],
										  psurf);
				}
			}
		}
	}
}


void
sw32_R_DrawSolidClippedSubmodelPolygons (model_t *model)
{
	int         i, j, lindex;
	vec_t       dot;
	msurface_t *psurf;
	int         numsurfaces;
	plane_t    *pplane;
	mvertex_t   bverts[MAX_BMODEL_VERTS];
	bedge_t     bedges[MAX_BMODEL_EDGES], *pbedge;
	medge_t    *pedge, *pedges;
	mod_brush_t *brush = &model->brush;

	// FIXME: use bounding-box-based frustum clipping info?

	psurf = &brush->surfaces[brush->firstmodelsurface];
	numsurfaces = brush->nummodelsurfaces;
	pedges = brush->edges;

	for (i = 0; i < numsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			// FIXME: use bounding-box-based frustum clipping info?

			// copy the edges to bedges, flipping if necessary so always
			// clockwise winding
			// FIXME: if edges and vertices get caches, these assignments must
			// move outside the loop, and overflow checking must be done here
			pbverts = bverts;
			pbedges = bedges;
			numbverts = numbedges = 0;

			if (psurf->numedges > 0) {
				pbedge = &bedges[numbedges];
				numbedges += psurf->numedges;

				for (j = 0; j < psurf->numedges; j++) {
					lindex = brush->surfedges[psurf->firstedge + j];

					if (lindex > 0) {
						pedge = &pedges[lindex];
						pbedge[j].v[0] = &r_pcurrentvertbase[pedge->v[0]];
						pbedge[j].v[1] = &r_pcurrentvertbase[pedge->v[1]];
					} else {
						lindex = -lindex;
						pedge = &pedges[lindex];
						pbedge[j].v[0] = &r_pcurrentvertbase[pedge->v[1]];
						pbedge[j].v[1] = &r_pcurrentvertbase[pedge->v[0]];
					}

					pbedge[j].pnext = &pbedge[j + 1];
				}

				pbedge[j - 1].pnext = NULL;	// mark end of edges

				R_RecursiveClipBPoly (pbedge, currententity->topnode, psurf);
			} else {
				Sys_Error ("no edges in bmodel");
			}
		}
	}
}


void
sw32_R_DrawSubmodelPolygons (model_t *model, int clipflags)
{
	int         i;
	vec_t       dot;
	msurface_t *psurf;
	int         numsurfaces;
	plane_t    *pplane;
	mod_brush_t *brush = &model->brush;

	// FIXME: use bounding-box-based frustum clipping info?

	psurf = &brush->surfaces[brush->firstmodelsurface];
	numsurfaces = brush->nummodelsurfaces;

	for (i = 0; i < numsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			sw32_r_currentkey = ((mleaf_t *) currententity->topnode)->key;

			// FIXME: use bounding-box-based frustum clipping info?
			sw32_R_RenderFace (psurf, clipflags);
		}
	}
}

static inline void
visit_leaf (mleaf_t *leaf)
{
	// deal with model fragments in this leaf
	if (leaf->efrags)
		R_StoreEfrags (leaf->efrags);
	leaf->key = sw32_r_currentkey;
	sw32_r_currentkey++;				// all bmodels in a leaf share the same key
}

static inline int
get_side (mnode_t *node)
{
	// find which side of the node we are on
	plane_t    *plane = node->plane;

	if (plane->type < 3)
		return (modelorg[plane->type] - plane->dist) < 0;
	return (DotProduct (modelorg, plane->normal) - plane->dist) < 0;
}

static void
visit_node (mod_brush_t *brush, mnode_t *node, int side, int clipflags)
{
	int         c;
	msurface_t *surf;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	side = (~side + 1) & SURF_PLANEBACK;
	// draw stuff
	if ((c = node->numsurfaces)) {
		surf = brush->surfaces + node->firstsurface;
		for (; c; c--, surf++) {
			if (surf->visframe != r_visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;				// wrong side

			if (sw32_r_drawpolys) {
				if (sw32_r_worldpolysbacktofront) {
					if (sw32_numbtofpolys < MAX_BTOFPOLYS) {
						pbtofpolys[sw32_numbtofpolys].clipflags = clipflags;
						pbtofpolys[sw32_numbtofpolys].psurf = surf;
						sw32_numbtofpolys++;
					}
				} else {
					sw32_R_RenderPoly (surf, clipflags);
				}
			} else {
				sw32_R_RenderFace (surf, clipflags);
			}
		}
		// all surfaces on the same node share the same sequence number
		sw32_r_currentkey++;
	}
}

static inline int
test_node (mnode_t *node, int *clipflags)
{
	int         i, *pindex;
	vec3_t      acceptpt, rejectpt;
	double      d;

	if (node->contents < 0)
		return 0;
	if (node->visframe != r_visframecount)
		return 0;
	// cull the clipping planes if not trivial accept
	// FIXME: the compiler is doing a lousy job of optimizing here; it could be
	// twice as fast in ASM
	if (*clipflags) {
		for (i = 0; i < 4; i++) {
			if (!(*clipflags & (1 << i)))
				continue;				// don't need to clip against it

			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the
			// sign bit of the floating point values

			pindex = sw32_pfrustum_indexes[i];

			rejectpt[0] = (float) node->minmaxs[pindex[0]];
			rejectpt[1] = (float) node->minmaxs[pindex[1]];
			rejectpt[2] = (float) node->minmaxs[pindex[2]];

			d = DotProduct (rejectpt, sw32_view_clipplanes[i].normal);
			d -= sw32_view_clipplanes[i].dist;

			if (d <= 0)
				return 0;

			acceptpt[0] = (float) node->minmaxs[pindex[3 + 0]];
			acceptpt[1] = (float) node->minmaxs[pindex[3 + 1]];
			acceptpt[2] = (float) node->minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, sw32_view_clipplanes[i].normal);
			d -= sw32_view_clipplanes[i].dist;
			if (d >= 0)
				*clipflags &= ~(1 << i);	// node is entirely on screen
		}
	}
	return 1;
}

static void
R_VisitWorldNodes (mod_brush_t *brush, int clipflags)
{
	typedef struct {
		mnode_t    *node;
		int         side, clipflags;
	} rstack_t;
	rstack_t   *node_ptr;
	rstack_t   *node_stack;
	mnode_t    *node;
	mnode_t    *front;
	int         side, cf;

	node = brush->nodes;
	// +2 for paranoia
	node_stack = alloca ((brush->depth + 2) * sizeof (rstack_t));
	node_ptr = node_stack;

	cf = clipflags;
	while (1) {
		while (test_node (node, &cf)) {
			cf = clipflags;
			side = get_side (node);
			front = node->children[side];
			if (test_node (front, &cf)) {
				node_ptr->node = node;
				node_ptr->side = side;
				node_ptr->clipflags = clipflags;
				node_ptr++;
				clipflags = cf;
				node = front;
				continue;
			}
			if (front->contents < 0 && front->contents != CONTENTS_SOLID)
				visit_leaf ((mleaf_t *) front);
			visit_node (brush, node, side, clipflags);
			node = node->children[!side];
		}
		if (node->contents < 0 && node->contents != CONTENTS_SOLID)
			visit_leaf ((mleaf_t *) node);
		if (node_ptr != node_stack) {
			node_ptr--;
			node = node_ptr->node;
			side = node_ptr->side;
			clipflags = node_ptr->clipflags;
			visit_node (brush, node, side, clipflags);
			node = node->children[!side];
			continue;
		}
		break;
	}
	if (node->contents < 0 && node->contents != CONTENTS_SOLID)
		visit_leaf ((mleaf_t *) node);
}

void
sw32_R_RenderWorld (void)
{
	int         i;
	btofpoly_t  btofpolys[MAX_BTOFPOLYS];
	mod_brush_t *brush;

	pbtofpolys = btofpolys;

	currententity = &r_worldentity;
	VectorCopy (r_origin, modelorg);
	brush = &currententity->model->brush;
	r_pcurrentvertbase = brush->vertexes;

	R_VisitWorldNodes (brush, 15);

	// if the driver wants the polygons back to front, play the visible ones
	// back in that order
	if (sw32_r_worldpolysbacktofront) {
		for (i = sw32_numbtofpolys - 1; i >= 0; i--) {
			sw32_R_RenderPoly (btofpolys[i].psurf, btofpolys[i].clipflags);
		}
	}
}
