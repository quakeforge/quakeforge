/*
	sw32_r_draw.c

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

#include "QF/render.h"

#include "compat.h"
#include "r_internal.h"

#define MAXLEFTCLIPEDGES		100

// !!! if these are changed, they must be changed in asm_draw.h too !!!
#define FULLY_CLIPPED_CACHED	0x80000000
#define FRAMECOUNT_MASK			0x7FFFFFFF

static unsigned int cacheoffset;

int         sw32_c_faceclip;					// number of faces clipped

zpointdesc_t sw32_r_zpointdesc;

static polydesc_t  r_polydesc;

clipplane_t sw32_view_clipplanes[4];

medge_t    *sw32_r_pedge;

qboolean    sw32_r_leftclipped, sw32_r_rightclipped;
static qboolean makeleftedge, makerightedge;
qboolean    sw32_r_nearzionly;

int         sw32_sintable[SIN_BUFFER_SIZE];
int         sw32_intsintable[SIN_BUFFER_SIZE];

mvertex_t   sw32_r_leftenter, sw32_r_leftexit;
mvertex_t   sw32_r_rightenter, sw32_r_rightexit;

typedef struct {
	float       u, v;
	int         ceilv;
} evert_t;

int         sw32_r_emitted;
float       sw32_r_nearzi;
float       sw32_r_u1, sw32_r_v1, sw32_r_lzi1;
int         sw32_r_ceilv1;

qboolean    sw32_r_lastvertvalid;


void
sw32_R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1)
{
	edge_t     *edge, *pcheck;
	int         u_check;
	float       u, u_step;
	vec3_t      local, transformed;
	float      *world;
	int         v, v2, ceilv0;
	float       scale, lzi0, u0, v0;
	int         side;

	if (sw32_r_lastvertvalid) {
		u0 = sw32_r_u1;
		v0 = sw32_r_v1;
		lzi0 = sw32_r_lzi1;
		ceilv0 = sw32_r_ceilv1;
	} else {
		world = &pv0->position[0];

		// transform and project
		VectorSubtract (world, modelorg, local);
		sw32_TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		lzi0 = 1.0 / transformed[2];

		// FIXME: build x/sw32_yscale into transform?
		scale = sw32_xscale * lzi0;
		u0 = (sw32_xcenter + scale * transformed[0]);
		if (u0 < r_refdef.fvrectx_adj)
			u0 = r_refdef.fvrectx_adj;
		if (u0 > r_refdef.fvrectright_adj)
			u0 = r_refdef.fvrectright_adj;

		scale = sw32_yscale * lzi0;
		v0 = (sw32_ycenter - scale * transformed[1]);
		if (v0 < r_refdef.fvrecty_adj)
			v0 = r_refdef.fvrecty_adj;
		if (v0 > r_refdef.fvrectbottom_adj)
			v0 = r_refdef.fvrectbottom_adj;

		ceilv0 = (int) ceil (v0);
	}

	world = &pv1->position[0];

	// transform and project
	VectorSubtract (world, modelorg, local);
	sw32_TransformVector (local, transformed);

	if (transformed[2] < NEAR_CLIP)
		transformed[2] = NEAR_CLIP;

	sw32_r_lzi1 = 1.0 / transformed[2];

	scale = sw32_xscale * sw32_r_lzi1;
	sw32_r_u1 = (sw32_xcenter + scale * transformed[0]);
	if (sw32_r_u1 < r_refdef.fvrectx_adj)
		sw32_r_u1 = r_refdef.fvrectx_adj;
	if (sw32_r_u1 > r_refdef.fvrectright_adj)
		sw32_r_u1 = r_refdef.fvrectright_adj;

	scale = sw32_yscale * sw32_r_lzi1;
	sw32_r_v1 = (sw32_ycenter - scale * transformed[1]);
	if (sw32_r_v1 < r_refdef.fvrecty_adj)
		sw32_r_v1 = r_refdef.fvrecty_adj;
	if (sw32_r_v1 > r_refdef.fvrectbottom_adj)
		sw32_r_v1 = r_refdef.fvrectbottom_adj;

	if (sw32_r_lzi1 > lzi0)
		lzi0 = sw32_r_lzi1;

	if (lzi0 > sw32_r_nearzi)				// for mipmap finding
		sw32_r_nearzi = lzi0;

	// for right edges, all we want is the effect on 1/z
	if (sw32_r_nearzionly)
		return;

	sw32_r_emitted = 1;

	sw32_r_ceilv1 = (int) ceil (sw32_r_v1);

	// create the edge
	if (ceilv0 == sw32_r_ceilv1) {
		// we cache unclipped horizontal edges as fully clipped
		if (cacheoffset != 0x7FFFFFFF) {
			cacheoffset = FULLY_CLIPPED_CACHED |
				(r_framecount & FRAMECOUNT_MASK);
		}

		return;							// horizontal edge
	}

	side = ceilv0 > sw32_r_ceilv1;

	edge = sw32_edge_p++;

	edge->owner = sw32_r_pedge;

	edge->nearzi = lzi0;

	if (side == 0) {
		// trailing edge (go from p1 to p2)
		v = ceilv0;
		v2 = sw32_r_ceilv1 - 1;

		edge->surfs[0] = sw32_surface_p - sw32_surfaces;
		edge->surfs[1] = 0;

		u_step = ((sw32_r_u1 - u0) / (sw32_r_v1 - v0));
		u = u0 + ((float) v - v0) * u_step;
	} else {
		// leading edge (go from p2 to p1)
		v2 = ceilv0 - 1;
		v = sw32_r_ceilv1;

		edge->surfs[0] = 0;
		edge->surfs[1] = sw32_surface_p - sw32_surfaces;

		u_step = ((u0 - sw32_r_u1) / (v0 - sw32_r_v1));
		u = sw32_r_u1 + ((float) v - sw32_r_v1) * u_step;
	}

	edge->u_step = u_step * 0x100000;
	edge->u = u * 0x100000 + 0xFFFFF;

	// we need to do this to avoid stepping off the edges if a very nearly
	// horizontal edge is less than epsilon above a scan, and numeric error
	// causes it to incorrectly extend to the scan, and the extension of the
	// line goes off the edge of the screen
	// FIXME: is this actually needed?
	if (edge->u < r_refdef.vrect_x_adj_shift20)
		edge->u = r_refdef.vrect_x_adj_shift20;
	if (edge->u > r_refdef.vrectright_adj_shift20)
		edge->u = r_refdef.vrectright_adj_shift20;

	// sort the edge in normally
	u_check = edge->u;
	if (edge->surfs[0])
		u_check++;						// sort trailers after leaders

	if (!sw32_newedges[v] || sw32_newedges[v]->u >= u_check) {
		edge->next = sw32_newedges[v];
		sw32_newedges[v] = edge;
	} else {
		pcheck = sw32_newedges[v];
		while (pcheck->next && pcheck->next->u < u_check)
			pcheck = pcheck->next;
		edge->next = pcheck->next;
		pcheck->next = edge;
	}

	edge->nextremove = sw32_removeedges[v2];
	sw32_removeedges[v2] = edge;
}


void
sw32_R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip)
{
	float       d0, d1, f;
	mvertex_t   clipvert;

	if (clip) {
		do {
			d0 = DotProduct (pv0->position, clip->normal) - clip->dist;
			d1 = DotProduct (pv1->position, clip->normal) - clip->dist;

			if (d0 >= 0) {
				// point 0 is unclipped
				if (d1 >= 0) {
					// both points are unclipped
					continue;
				}
				// only point 1 is clipped

				// we don't cache clipped edges
				cacheoffset = 0x7FFFFFFF;

				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
					f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
					f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
					f * (pv1->position[2] - pv0->position[2]);

				if (clip->leftedge) {
					sw32_r_leftclipped = true;
					sw32_r_leftexit = clipvert;
				} else if (clip->rightedge) {
					sw32_r_rightclipped = true;
					sw32_r_rightexit = clipvert;
				}

				sw32_R_ClipEdge (pv0, &clipvert, clip->next);
				return;
			} else {
				// point 0 is clipped
				if (d1 < 0) {
					// both points are clipped
					// we do cache fully clipped edges
					if (!sw32_r_leftclipped)
						cacheoffset = FULLY_CLIPPED_CACHED |
							(r_framecount & FRAMECOUNT_MASK);
					return;
				}
				// only point 0 is clipped
				sw32_r_lastvertvalid = false;

				// we don't cache partially clipped edges
				cacheoffset = 0x7FFFFFFF;

				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
					f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
					f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
					f * (pv1->position[2] - pv0->position[2]);

				if (clip->leftedge) {
					sw32_r_leftclipped = true;
					sw32_r_leftenter = clipvert;
				} else if (clip->rightedge) {
					sw32_r_rightclipped = true;
					sw32_r_rightenter = clipvert;
				}

				sw32_R_ClipEdge (&clipvert, pv1, clip->next);
				return;
			}
		} while ((clip = clip->next) != NULL);
	}
	// add the edge
	sw32_R_EmitEdge (pv0, pv1);
}


static void
R_EmitCachedEdge (void)
{
	edge_t     *pedge_t;

	pedge_t = (edge_t *) ((intptr_t) sw32_r_edges + sw32_r_pedge->cachededgeoffset);

	if (!pedge_t->surfs[0])
		pedge_t->surfs[0] = sw32_surface_p - sw32_surfaces;
	else
		pedge_t->surfs[1] = sw32_surface_p - sw32_surfaces;

	if (pedge_t->nearzi > sw32_r_nearzi)		// for mipmap finding
		sw32_r_nearzi = pedge_t->nearzi;

	sw32_r_emitted = 1;
}


void
sw32_R_RenderFace (msurface_t *fa, int clipflags)
{
	int         i, lindex;
	unsigned int mask;
	plane_t    *pplane;
	float       distinv;
	vec3_t      p_normal;
	medge_t    *pedges, tedge;
	clipplane_t *pclip;
	mod_brush_t *brush = &currententity->model->brush;

	// skip out if no more surfs
	if ((sw32_surface_p) >= sw32_surf_max) {
		sw32_r_outofsurfaces++;
		return;
	}
	// ditto if not enough edges left, or switch to auxedges if possible
	if ((sw32_edge_p + fa->numedges + 4) >= sw32_edge_max) {
		sw32_r_outofedges += fa->numedges;
		return;
	}

	sw32_c_faceclip++;

	// set up clip planes
	pclip = NULL;

	for (i = 3, mask = 0x08; i >= 0; i--, mask >>= 1) {
		if (clipflags & mask) {
			sw32_view_clipplanes[i].next = pclip;
			pclip = &sw32_view_clipplanes[i];
		}
	}

	// push the edges through
	sw32_r_emitted = 0;
	sw32_r_nearzi = 0;
	sw32_r_nearzionly = false;
	makeleftedge = makerightedge = false;
	pedges = brush->edges;
	sw32_r_lastvertvalid = false;

	for (i = 0; i < fa->numedges; i++) {
		lindex = brush->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			sw32_r_pedge = &pedges[lindex];

			// if the edge is cached, we can just reuse the edge
			if (!insubmodel) {
				if (sw32_r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED) {
					if ((sw32_r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						(unsigned int) r_framecount) {
						sw32_r_lastvertvalid = false;
						continue;
					}
				} else {
					if ((((uintptr_t) sw32_edge_p - (uintptr_t) sw32_r_edges) >
						 sw32_r_pedge->cachededgeoffset) &&
						(((edge_t *) ((intptr_t) sw32_r_edges +
									  sw32_r_pedge->cachededgeoffset))->owner ==
						 sw32_r_pedge)) {
						R_EmitCachedEdge ();
						sw32_r_lastvertvalid = false;
						continue;
					}
				}
			}
			// assume it's cacheable
			cacheoffset = (byte *) sw32_edge_p - (byte *) sw32_r_edges;
			sw32_r_leftclipped = sw32_r_rightclipped = false;
			sw32_R_ClipEdge (&r_pcurrentvertbase[sw32_r_pedge->v[0]],
						&r_pcurrentvertbase[sw32_r_pedge->v[1]], pclip);
			sw32_r_pedge->cachededgeoffset = cacheoffset;

			if (sw32_r_leftclipped)
				makeleftedge = true;
			if (sw32_r_rightclipped)
				makerightedge = true;
			sw32_r_lastvertvalid = true;
		} else {
			lindex = -lindex;
			sw32_r_pedge = &pedges[lindex];
			// if the edge is cached, we can just reuse the edge
			if (!insubmodel) {
				if (sw32_r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED) {
					if ((sw32_r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						(unsigned int) r_framecount) {
						sw32_r_lastvertvalid = false;
						continue;
					}
				} else {
					// it's cached if the cached edge is valid and is owned
					// by this medge_t
					if ((((uintptr_t) sw32_edge_p - (uintptr_t) sw32_r_edges) >
						 sw32_r_pedge->cachededgeoffset) &&
						(((edge_t *) ((intptr_t) sw32_r_edges +
									  sw32_r_pedge->cachededgeoffset))->owner ==
						 sw32_r_pedge)) {
						R_EmitCachedEdge ();
						sw32_r_lastvertvalid = false;
						continue;
					}
				}
			}
			// assume it's cacheable
			cacheoffset = (byte *) sw32_edge_p - (byte *) sw32_r_edges;
			sw32_r_leftclipped = sw32_r_rightclipped = false;
			sw32_R_ClipEdge (&r_pcurrentvertbase[sw32_r_pedge->v[1]],
						&r_pcurrentvertbase[sw32_r_pedge->v[0]], pclip);
			sw32_r_pedge->cachededgeoffset = cacheoffset;

			if (sw32_r_leftclipped)
				makeleftedge = true;
			if (sw32_r_rightclipped)
				makerightedge = true;
			sw32_r_lastvertvalid = true;
		}
	}

	// if there was a clip off the left edge, add that edge too
	// FIXME: faster to do in screen space?
	// FIXME: share clipped edges?
	if (makeleftedge) {
		sw32_r_pedge = &tedge;
		sw32_r_lastvertvalid = false;
		sw32_R_ClipEdge (&sw32_r_leftexit, &sw32_r_leftenter, pclip->next);
	}
	// if there was a clip off the right edge, get the right sw32_r_nearzi
	if (makerightedge) {
		sw32_r_pedge = &tedge;
		sw32_r_lastvertvalid = false;
		sw32_r_nearzionly = true;
		sw32_R_ClipEdge (&sw32_r_rightexit, &sw32_r_rightenter, sw32_view_clipplanes[1].next);
	}
	// if no edges made it out, return without posting the surface
	if (!sw32_r_emitted)
		return;

	sw32_r_polycount++;

	sw32_surface_p->data = (void *) fa;
	sw32_surface_p->nearzi = sw32_r_nearzi;
	sw32_surface_p->flags = fa->flags;
	sw32_surface_p->insubmodel = insubmodel;
	sw32_surface_p->spanstate = 0;
	sw32_surface_p->entity = currententity;
	sw32_surface_p->key = sw32_r_currentkey++;
	sw32_surface_p->spans = NULL;

	pplane = fa->plane;
// FIXME: cache this?
	sw32_TransformVector (pplane->normal, p_normal);
// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));
	distinv = min (distinv, 1.0);

	sw32_surface_p->d_zistepu = p_normal[0] * sw32_xscaleinv * distinv;
	sw32_surface_p->d_zistepv = -p_normal[1] * sw32_yscaleinv * distinv;
	sw32_surface_p->d_ziorigin = p_normal[2] * distinv -
		sw32_xcenter * sw32_surface_p->d_zistepu - sw32_ycenter * sw32_surface_p->d_zistepv;

//JDC   VectorCopy (r_worldmodelorg, sw32_surface_p->modelorg);
	sw32_surface_p++;
}


void
sw32_R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf)
{
	int         i;
	unsigned int mask;
	plane_t    *pplane;
	float       distinv;
	vec3_t      p_normal;
	medge_t     tedge;
	clipplane_t *pclip;

	// skip out if no more surfs
	if (sw32_surface_p >= sw32_surf_max) {
		sw32_r_outofsurfaces++;
		return;
	}
	// ditto if not enough edges left, or switch to auxedges if possible
	if ((sw32_edge_p + psurf->numedges + 4) >= sw32_edge_max) {
		sw32_r_outofedges += psurf->numedges;
		return;
	}

	sw32_c_faceclip++;

	// this is a dummy to give the caching mechanism someplace to write to
	sw32_r_pedge = &tedge;

	// set up clip planes
	pclip = NULL;

	for (i = 3, mask = 0x08; i >= 0; i--, mask >>= 1) {
		if (sw32_r_clipflags & mask) {
			sw32_view_clipplanes[i].next = pclip;
			pclip = &sw32_view_clipplanes[i];
		}
	}

	// push the edges through
	sw32_r_emitted = 0;
	sw32_r_nearzi = 0;
	sw32_r_nearzionly = false;
	makeleftedge = makerightedge = false;
	// FIXME: keep clipped bmodel edges in clockwise order so last vertex
	// caching can be used?
	sw32_r_lastvertvalid = false;

	for (; pedges; pedges = pedges->pnext) {
		sw32_r_leftclipped = sw32_r_rightclipped = false;
		sw32_R_ClipEdge (pedges->v[0], pedges->v[1], pclip);

		if (sw32_r_leftclipped)
			makeleftedge = true;
		if (sw32_r_rightclipped)
			makerightedge = true;
	}

	// if there was a clip off the left edge, add that edge too
	// FIXME: faster to do in screen space?
	// FIXME: share clipped edges?
	if (makeleftedge) {
		sw32_r_pedge = &tedge;
		sw32_R_ClipEdge (&sw32_r_leftexit, &sw32_r_leftenter, pclip->next);
	}
	// if there was a clip off the right edge, get the right sw32_r_nearzi
	if (makerightedge) {
		sw32_r_pedge = &tedge;
		sw32_r_nearzionly = true;
		sw32_R_ClipEdge (&sw32_r_rightexit, &sw32_r_rightenter, sw32_view_clipplanes[1].next);
	}
	// if no edges made it out, return without posting the surface
	if (!sw32_r_emitted)
		return;

	sw32_r_polycount++;

	sw32_surface_p->data = (void *) psurf;
	sw32_surface_p->nearzi = sw32_r_nearzi;
	sw32_surface_p->flags = psurf->flags;
	sw32_surface_p->insubmodel = true;
	sw32_surface_p->spanstate = 0;
	sw32_surface_p->entity = currententity;
	sw32_surface_p->key = sw32_r_currentbkey;
	sw32_surface_p->spans = NULL;

	pplane = psurf->plane;
// FIXME: cache this?
	sw32_TransformVector (pplane->normal, p_normal);
// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));
	distinv = min (distinv, 1.0);

	sw32_surface_p->d_zistepu = p_normal[0] * sw32_xscaleinv * distinv;
	sw32_surface_p->d_zistepv = -p_normal[1] * sw32_yscaleinv * distinv;
	sw32_surface_p->d_ziorigin = p_normal[2] * distinv -
		sw32_xcenter * sw32_surface_p->d_zistepu - sw32_ycenter * sw32_surface_p->d_zistepv;

//JDC   VectorCopy (r_worldmodelorg, sw32_surface_p->modelorg);
	sw32_surface_p++;
}


void
sw32_R_RenderPoly (msurface_t *fa, int clipflags)
{
	int         i, lindex, lnumverts, s_axis, t_axis;
	float       dist, lastdist, lzi, scale, u, v, frac;
	unsigned int mask;
	vec3_t      local, transformed;
	clipplane_t *pclip;
	medge_t    *pedges;
	plane_t    *pplane;
	mvertex_t   verts[2][100];			// FIXME: do real number
	polyvert_t  pverts[100];			// FIXME: do real number, safely
	int         vertpage, newverts, newpage, lastvert;
	qboolean    visible;
	mod_brush_t *brush = &currententity->model->brush;

	// FIXME: clean this up and make it faster
	// FIXME: guard against running out of vertices

	s_axis = t_axis = 0;				// keep compiler happy

	// set up clip planes
	pclip = NULL;

	for (i = 3, mask = 0x08; i >= 0; i--, mask >>= 1) {
		if (clipflags & mask) {
			sw32_view_clipplanes[i].next = pclip;
			pclip = &sw32_view_clipplanes[i];
		}
	}

	// reconstruct the polygon
	// FIXME: these should be precalculated and loaded off disk
	pedges = brush->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	for (i = 0; i < lnumverts; i++) {
		lindex = brush->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			sw32_r_pedge = &pedges[lindex];
			verts[0][i] = r_pcurrentvertbase[sw32_r_pedge->v[0]];
		} else {
			sw32_r_pedge = &pedges[-lindex];
			verts[0][i] = r_pcurrentvertbase[sw32_r_pedge->v[1]];
		}
	}

	// clip the polygon, done if not visible
	while (pclip) {
		lastvert = lnumverts - 1;
		lastdist = DotProduct (verts[vertpage][lastvert].position,
							   pclip->normal) - pclip->dist;

		visible = false;
		newverts = 0;
		newpage = vertpage ^ 1;

		for (i = 0; i < lnumverts; i++) {
			dist = DotProduct (verts[vertpage][i].position, pclip->normal) -
				pclip->dist;

			if ((lastdist > 0) != (dist > 0)) {
				frac = dist / (dist - lastdist);
				verts[newpage][newverts].position[0] =
					verts[vertpage][i].position[0] +
					((verts[vertpage][lastvert].position[0] -
					  verts[vertpage][i].position[0]) * frac);
				verts[newpage][newverts].position[1] =
					verts[vertpage][i].position[1] +
					((verts[vertpage][lastvert].position[1] -
					  verts[vertpage][i].position[1]) * frac);
				verts[newpage][newverts].position[2] =
					verts[vertpage][i].position[2] +
					((verts[vertpage][lastvert].position[2] -
					  verts[vertpage][i].position[2]) * frac);
				newverts++;
			}

			if (dist >= 0) {
				verts[newpage][newverts] = verts[vertpage][i];
				newverts++;
				visible = true;
			}

			lastvert = i;
			lastdist = dist;
		}

		if (!visible || (newverts < 3))
			return;

		lnumverts = newverts;
		vertpage ^= 1;
		pclip = pclip->next;
	}

	// transform and project, remembering the z values at the vertices and
	// sw32_r_nearzi, and extract the s and t coordinates at the vertices
	pplane = fa->plane;
	switch (pplane->type) {
		case PLANE_X:
		case PLANE_ANYX:
			s_axis = 1;
			t_axis = 2;
			break;
		case PLANE_Y:
		case PLANE_ANYY:
			s_axis = 0;
			t_axis = 2;
			break;
		case PLANE_Z:
		case PLANE_ANYZ:
			s_axis = 0;
			t_axis = 1;
			break;
	}

	sw32_r_nearzi = 0;

	for (i = 0; i < lnumverts; i++) {
		// transform and project
		VectorSubtract (verts[vertpage][i].position, modelorg, local);
		sw32_TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		lzi = 1.0 / transformed[2];

		if (lzi > sw32_r_nearzi)				// for mipmap finding
			sw32_r_nearzi = lzi;

		// FIXME: build x/sw32_yscale into transform?
		scale = sw32_xscale * lzi;
		u = (sw32_xcenter + scale * transformed[0]);
		if (u < r_refdef.fvrectx_adj)
			u = r_refdef.fvrectx_adj;
		if (u > r_refdef.fvrectright_adj)
			u = r_refdef.fvrectright_adj;

		scale = sw32_yscale * lzi;
		v = (sw32_ycenter - scale * transformed[1]);
		if (v < r_refdef.fvrecty_adj)
			v = r_refdef.fvrecty_adj;
		if (v > r_refdef.fvrectbottom_adj)
			v = r_refdef.fvrectbottom_adj;

		pverts[i].u = u;
		pverts[i].v = v;
		pverts[i].zi = lzi;
		pverts[i].s = verts[vertpage][i].position[s_axis];
		pverts[i].t = verts[vertpage][i].position[t_axis];
	}

	// build the polygon descriptor, including fa, sw32_r_nearzi, and u, v, s, t,
	// and z for each vertex
	r_polydesc.numverts = lnumverts;
	r_polydesc.nearzi = sw32_r_nearzi;
	r_polydesc.pcurrentface = fa;
	r_polydesc.pverts = pverts;

	// draw the polygon
	sw32_D_DrawPoly ();
}


void
sw32_R_ZDrawSubmodelPolys (model_t *model)
{
	int         i, numsurfaces;
	msurface_t *psurf;
	float       dot;
	plane_t    *pplane;
	mod_brush_t *brush = &model->brush;

	psurf = &brush->surfaces[brush->firstmodelsurface];
	numsurfaces = brush->nummodelsurfaces;

	for (i = 0; i < numsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			// FIXME: use bounding-box-based frustum clipping info?
			sw32_R_RenderPoly (psurf, 15);
		}
	}
}
