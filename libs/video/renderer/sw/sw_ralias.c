/*
	sw_ralias.c

	routines for setting up to draw mesh models

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

#include <stdlib.h>
#include <string.h>

#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "d_ifacea.h"
#include "mod_internal.h"
#include "r_internal.h"

#define LIGHT_MIN	5					// lowest light value we'll allow, to
										// avoid the need for inner-loop light
										// clamping

affinetridesc_t r_affinetridesc;

const byte *acolormap;					// FIXME: should go away

trivertx_t *r_apverts;

// TODO: these probably will go away with optimized rasterization
vec3_t      r_lightvec;
int         r_ambientlight;
float       r_shadelight;
finalvert_t *pfinalverts;
auxvert_t  *pauxverts;
float ziscale;
static model_t *pmodel;

static vec3_t alias_forward, alias_left, alias_up;

int         r_amodels_drawn;
int         r_anumverts;

float       aliastransform[3][4];

typedef struct {
	int         index0;
	int         index1;
} aedge_t;

static aedge_t aedges[12] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 0},
	{4, 5}, {5, 6}, {6, 7}, {7, 4},
	{0, 5}, {1, 4}, {2, 7}, {3, 6}
};

static void R_AliasSetUpTransform (entity_t ent, int trivial_accept,
								   qf_mesh_t *mesh);

static qfm_frame_t *
get_frame (double time, animation_t *animation, qf_mesh_t *mesh)
{
	// pose2 points to the frame data
	return (qfm_frame_t *) ((byte *) mesh + animation->pose2);
}

bool
R_AliasCheckBBox (entity_t ent)
{
	int         i, flags, numv;
	float       zi, basepts[8][3], v0, v1, frac;
	finalvert_t *pv0, *pv1, viewpts[16];
	auxvert_t  *pa0, *pa1, viewaux[16];
	bool        zclipped, zfullyclipped;
	unsigned int anyclip, allclip;
	int         minz;

	// expand, rotate, and translate points into worldspace
	visibility_t *visibility = Ent_GetComponent (ent.id,
												 ent.base + scene_visibility,
												 ent.reg);
	visibility->trivial_accept = 0;

	auto renderer = Entity_GetRenderer (ent);
	pmodel = renderer->model;
	auto model = pmodel->model;
	if (!model) {
		model = Cache_Get (&pmodel->cache);
	}
	auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);

	auto rmesh = (sw_alias_mesh_t *) ((byte *) model + model->render_data);
	R_AliasSetUpTransform (ent, 0, mesh);

	// construct the base bounding box for this frame
	auto animation = Entity_GetAnimation (ent);
	auto frame = get_frame (vr_data.realtime, animation, mesh);

	// x worldspace coordinates
	basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =
		frame->bounds_min[0];
	basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =
		frame->bounds_max[0];

	// y worldspace coordinates
	basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =
		frame->bounds_min[1];
	basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =
		frame->bounds_max[1];

	// z worldspace coordinates
	basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] =
		frame->bounds_min[2];
	basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] =
		frame->bounds_max[2];

	zclipped = false;
	zfullyclipped = true;

	minz = 9999;
	for (i = 0; i < 8; i++) {
		R_AliasTransformVector (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE) {
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		} else {
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	if (zfullyclipped) {
		if (!pmodel->model)
			Cache_Release (&pmodel->cache);
		return false;					// everything was near-z-clipped
	}

	numv = 8;

	if (zclipped) {
		// organize points by edges, use edges to get new points (possible
		// trivial reject)
		for (i = 0; i < 12; i++) {
			// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

			// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags) {
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
					(pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] +
					(pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] +
					(pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}
	// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

// TODO: probably should do this loop in ASM, especially if we use floats
	for (i = 0; i < numv; i++) {
		// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

		// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip) {
		if (!pmodel->model)
			Cache_Release (&pmodel->cache);
		return false;					// trivial reject off one side
	}

	visibility->trivial_accept = !anyclip & !zclipped;

	if (visibility->trivial_accept) {
		if (minz > (r_aliastransition + (rmesh->size * r_resfudge))) {
			visibility->trivial_accept |= 2;
		}
	}

	if (!pmodel->model)
		Cache_Release (&pmodel->cache);
	return true;
}

void
R_AliasTransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct (in, aliastransform[0]) + aliastransform[0][3];
	out[1] = DotProduct (in, aliastransform[1]) + aliastransform[1][3];
	out[2] = DotProduct (in, aliastransform[2]) + aliastransform[2][3];
}

void
R_AliasClipAndProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	if (av->fv[2] < ALIAS_Z_CLIP_PLANE) {
		fv->flags |= ALIAS_Z_CLIP;
		return;
	}

	R_AliasProjectFinalVert (fv, av);

	if (fv->v[0] < r_refdef.aliasvrectleft)
		fv->flags |= ALIAS_LEFT_CLIP;
	if (fv->v[1] < r_refdef.aliasvrecttop)
		fv->flags |= ALIAS_TOP_CLIP;
	if (fv->v[0] > r_refdef.aliasvrectright)
		fv->flags |= ALIAS_RIGHT_CLIP;
	if (fv->v[1] > r_refdef.aliasvrectbottom)
		fv->flags |= ALIAS_BOTTOM_CLIP;
}

static void
R_AliasTransformFinalVert16 (auxvert_t *av, trivertx16_t *vert)
{
	vec3_t      v;

	VectorScale (vert->v, 1.0f/256, v);
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
R_AliasTransformFinalVert8 (auxvert_t *av, trivertx_t *pverts)
{
	av->fv[0] = DotProduct (pverts->v, aliastransform[0]) +
		aliastransform[0][3];
	av->fv[1] = DotProduct (pverts->v, aliastransform[1]) +
		aliastransform[1][3];
	av->fv[2] = DotProduct (pverts->v, aliastransform[2]) +
		aliastransform[2][3];
}

/*
	R_AliasPreparePoints

	General clipped case
*/
static void
R_AliasPreparePoints (qf_mesh_t *mesh)
{
	int         i;
	stvert_t   *pstverts;
	finalvert_t *fv;
	auxvert_t  *av;
	dtriangle_t *ptri;
	finalvert_t *pfv[3];

	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	pstverts = (stvert_t *) ((byte *) mesh + attrib[2].offset);
	r_anumverts = mesh->vertices.count;
	fv = pfinalverts;
	av = pauxverts;

	if (attrib[0].type == qfm_u16) {
		auto verts = (trivertx16_t *) ((byte *) mesh + attrib[0].offset);
		for (i = 0; i < r_anumverts; i++, fv++, av++, verts++, pstverts++) {
			R_AliasTransformFinalVert16 (av, verts);
			R_AliasTransformFinalVert (fv, verts->lightnormalindex, pstverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	} else {
		auto verts = (trivertx_t *) ((byte *) mesh + attrib[0].offset);
		for (i = 0; i < r_anumverts; i++, fv++, av++, verts++, pstverts++) {
			R_AliasTransformFinalVert8 (av, verts);
			R_AliasTransformFinalVert (fv, verts->lightnormalindex, pstverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	}

	// clip and draw all triangles
	r_affinetridesc.numtriangles = 1;

	ptri = (dtriangle_t *) ((byte *) mesh + mesh->indices);
	for (uint32_t i = 0; i < mesh->triangle_count; i++, ptri++) {
		pfv[0] = &pfinalverts[ptri->vertindex[0]];
		pfv[1] = &pfinalverts[ptri->vertindex[1]];
		pfv[2] = &pfinalverts[ptri->vertindex[2]];

		if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags
			& (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
			continue;					// completely clipped

		if (!((pfv[0]->flags | pfv[1]->flags | pfv[2]->flags)
			  & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))) {	// totally unclipped
			r_affinetridesc.finalverts = pfinalverts;
			r_affinetridesc.triangles = ptri;
			D_PolysetDraw ();
		} else {						// partially clipped
			R_AliasClipTriangle (ptri);
		}
	}
}

static void
R_AliasSetUpTransform (entity_t ent, int trivial_accept, qf_mesh_t *mesh)
{
	int         i;
	float       rotationmatrix[3][4];
	transform_t transform = Entity_Transform (ent);

	mat4f_t     mat;
	Transform_GetWorldMatrix (transform, mat);
	VectorCopy (mat[0], alias_forward);
	VectorCopy (mat[1], alias_left);
	VectorCopy (mat[2], alias_up);

	for (i = 0; i < 3; i++) {
		rotationmatrix[i][0] = mesh->scale[0] * alias_forward[i];
		rotationmatrix[i][1] = mesh->scale[1] * alias_left[i];
		rotationmatrix[i][2] = mesh->scale[2] * alias_up[i];
		rotationmatrix[i][3] = mesh->scale_origin[0] * alias_forward[i]
							 + mesh->scale_origin[1] * alias_left[i]
							 + mesh->scale_origin[2] * alias_up[i]
							 + r_entorigin[i] - r_refdef.frame.position[i];
	}

	R_ConcatTransforms (r_viewmatrix, rotationmatrix, aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?

	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	if (trivial_accept && attrib[0].type != qfm_u16) {
		constexpr float conv = (1.0 / ((float) 0x8000 * 0x10000));
		for (i = 0; i < 4; i++) {
			aliastransform[0][i] *= aliasxscale * conv;
			aliastransform[1][i] *= aliasyscale * conv;
			aliastransform[2][i] *= conv;
		}
	}
}

/*
	R_AliasTransformFinalVert

	now this function just copies the texture coordinates and calculates
	lighting actual 3D transform is done by R_AliasTransformFinalVert8/16
	functions above */
void
R_AliasTransformFinalVert (finalvert_t *fv, int lightnormalindex,
						   stvert_t *pstverts)
{
	int         temp;
	float       lightcos, *plightnormal;

	fv->v[2] = pstverts->s;
	fv->v[3] = pstverts->t;

	fv->flags = pstverts->onseam;

	// lighting
	plightnormal = r_avertexnormals[lightnormalindex];
	lightcos = DotProduct (plightnormal, r_lightvec);
	temp = r_ambientlight;

	if (lightcos < 0) {
		temp += (int) (r_shadelight * lightcos);

		// clamp; because we limited the minimum ambient and shading light,
		// we don't have to clamp low light, just bright
		if (temp < 0)
			temp = 0;
	}

	fv->v[4] = temp;
}

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifndef USE_INTEL_ASM
void
R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts)
{
	int         i, temp;
	float       lightcos, *plightnormal, zi;
	trivertx_t *pverts;

	pverts = r_apverts;

	for (i = 0; i < r_anumverts; i++, fv++, pverts++, pstverts++) {
		// transform and project
		zi = 1.0 / (DotProduct (pverts->v, aliastransform[2]) +
					aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (pverts->v, aliastransform[0]) +
					 aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (pverts->v, aliastransform[1]) +
					 aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = pstverts->s;
		fv->v[3] = pstverts->t;
		fv->flags = pstverts->onseam;

		// lighting
		plightnormal = r_avertexnormals[pverts->lightnormalindex];
		lightcos = DotProduct (plightnormal, r_lightvec);
		temp = r_ambientlight;

		if (lightcos < 0) {
			temp += (int) (r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading
			// light, we don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->v[4] = temp;
	}
}
#endif

void
R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	float       zi;

	// project points
	zi = 1.0 / av->fv[2];

	fv->v[5] = zi * ziscale;

	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}

static void
R_AliasPrepareUnclippedPoints (qf_mesh_t *mesh)
{
	stvert_t   *pstverts;
	finalvert_t *fv;

	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	pstverts = (stvert_t *) ((byte *) mesh + attrib[2].offset);
	r_anumverts = mesh->vertices.offset;
// FIXME: just use pfinalverts directly?
	fv = pfinalverts;

	R_AliasTransformAndProjectFinalVerts (fv, pstverts);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (fv, r_anumverts);

	r_affinetridesc.finalverts = pfinalverts;
	r_affinetridesc.triangles = (dtriangle_t *) ((byte *) mesh + mesh->indices);
	r_affinetridesc.numtriangles = mesh->triangle_count;

	D_PolysetDraw ();
}

static void
R_AliasSetupSkin (entity_t ent, qf_mesh_t *mesh)
{
	r_affinetridesc.skin = nullptr;

	auto renderer = Entity_GetRenderer (ent);
	if (renderer->skin) {
		auto skin = Skin_Get (renderer->skin);
		if (skin) {
			tex_t      *tex = skin->tex;
			r_affinetridesc.skin = tex->data;
			r_affinetridesc.skinwidth = tex->width;
			r_affinetridesc.skinheight = tex->height;
		}
	}
	if (!r_affinetridesc.skin) {
		auto skinpic = (qpic_t *) ((byte *) mesh + renderer->skindesc);
		r_affinetridesc.skin = skinpic->data;
		r_affinetridesc.skinwidth = skinpic->width;
		r_affinetridesc.seamfixupX16 = (skinpic->width >> 1) << 16;
		r_affinetridesc.skinheight = skinpic->height;
	}
}


static void
R_AliasSetupLighting (alight_t *lighting)
{
	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't
	// have to clamp off the bottom
	r_ambientlight = lighting->ambientlight;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_shadelight = lighting->shadelight;

	if (r_shadelight < 0)
		r_shadelight = 0;

	r_shadelight *= VID_GRADES;

	// rotate the lighting vector into the model's frame of reference
	r_lightvec[0] = DotProduct (lighting->lightvec, alias_forward);
	r_lightvec[1] = DotProduct (lighting->lightvec, alias_left);
	r_lightvec[2] = DotProduct (lighting->lightvec, alias_up);
}

/*
	R_AliasSetupFrame

	set r_apverts
*/
static void
R_AliasSetupFrame (entity_t ent, qf_mesh_t *mesh)
{
	qfm_frame_t *frame;

	auto animation = Entity_GetAnimation (ent);
	frame = get_frame (vr_data.realtime, animation, mesh);
	r_apverts = (trivertx_t *) ((byte *) mesh + frame->data);
}


void
R_AliasDrawModel (entity_t ent, alight_t *lighting)
{
	int          size;
	finalvert_t *finalverts;

	r_amodels_drawn++;

	auto renderer = Entity_GetRenderer (ent);
	if (renderer->onlyshadows) {
		return;
	}
	auto model = renderer->model->model;
	if (!model) {
		model = Cache_Get (&renderer->model->cache);
	}
	auto mesh = (qf_mesh_t *) ((byte *) model + model->meshes.offset);

	size = (CACHE_SIZE - 1)
		+ sizeof (finalvert_t) * (mesh->vertices.count + 1)
		+ sizeof (auxvert_t) * mesh->vertices.count;
	finalverts = (finalvert_t *) Hunk_TempAlloc (0, size);
	if (!finalverts)
		Sys_Error ("R_AliasDrawModel: out of memory");

	// cache align
	pfinalverts = (finalvert_t *)
		(((intptr_t) &finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = (auxvert_t *) &pfinalverts[mesh->vertices.count + 1];

	R_AliasSetupSkin (ent, mesh);
	visibility_t *visibility = Ent_GetComponent (ent.id,
												 ent.base + scene_visibility,
												 ent.reg);
	R_AliasSetUpTransform (ent, visibility->trivial_accept, mesh);
	R_AliasSetupLighting (lighting);
	R_AliasSetupFrame (ent, mesh);

	r_affinetridesc.drawtype = ((visibility->trivial_accept == 3)
								&& r_recursiveaffinetriangles);

	acolormap = r_colormap;
	auto cmap = Entity_GetColormap (ent);
	if (cmap) {
		acolormap = sw_Skin_Colormap (cmap);
	}

	if (r_affinetridesc.drawtype) {
		D_PolysetUpdateTables ();		// FIXME: precalc...
	} else {
#ifdef USE_INTEL_ASM
		D_Aff8Patch (acolormap);
#endif
	}

	//FIXME depth hack
	if (ent.id != vr_data.view_model.id)
		ziscale = (float) 0x8000 *(float) 0x10000;
	else
		ziscale = (float) 0x8000 *(float) 0x10000 *3.0;

	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	if (visibility->trivial_accept && attrib[0].type != qfm_u16) {
		R_AliasPrepareUnclippedPoints (mesh);
	} else {
		R_AliasPreparePoints (mesh);
	}

	if (!renderer->model->model) {
		Cache_Release (&renderer->model->cache);
	}
}
