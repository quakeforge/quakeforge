/*
	sw_riqm.c

	SW IQM rendering

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/18

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
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "d_ifacea.h"
#include "r_internal.h"

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#define LIGHT_MIN	5					// lowest light value we'll allow, to
										// avoid the need for inner-loop light
										// clamping

static vec3_t r_lightvec;
static int    r_ambientlight;
static float  r_shadelight;

static inline int
calc_light (float *normal)
{
	float       lightcos = DotProduct (normal, r_lightvec);
	int         temp = r_ambientlight;

	if (lightcos < 0) {
		temp += (int) (r_shadelight * lightcos);

		// clamp; because we limited the minimum ambient and shading
		// light, we don't have to clamp low light, just bright
		if (temp < 0)
			temp = 0;
	}
	return temp;
}

static void
R_IQMTransformAndProjectFinalVerts (iqm_t *iqm, swiqm_t *sw, iqmframe_t *frame)
{
	finalvert_t *fv = pfinalverts;
	float       zi;
	int         i;

	for (i = 0; i < iqm->num_verts; i++, fv++) {
		byte       *vert = iqm->vertices + i * iqm->stride;
		uint32_t    bind = *(uint32_t *) (vert + sw->bindices->offset);
		vec_t      *mat = (vec_t *) &frame[bind];
		float      *position = (float *) (vert + sw->position->offset);
		float      *normal = (float *) (vert + sw->normal->offset);
		int32_t    *texcoord = (int32_t *) (vert + sw->texcoord->offset);
		vec3_t      tv, tn;
		Mat4MultVec (mat, position, tv);
		Mat4as3MultVec (mat, normal, tn);
		zi = 1.0 / (DotProduct (tv, aliastransform[2])
					+ aliastransform[2][3]);
		fv->v[5] = zi;
		fv->v[0] = (DotProduct (tv, aliastransform[0])
					+ aliastransform[0][3]) * zi + aliasxcenter;
		fv->v[1] = (DotProduct (tv, aliastransform[1])
					+ aliastransform[1][3]) * zi + aliasxcenter;
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->v[4] = calc_light (tn);
	}
}

static void
iqm_setup_skin (swiqm_t *sw, int skinnum)
{
	tex_t      *skin = sw->skins[skinnum];

	r_affinetridesc.pskin = skin->data;
	r_affinetridesc.skinwidth = skin->width;
	r_affinetridesc.skinheight = skin->height;
	r_affinetridesc.seamfixupX16 = (skin->width >> 1) << 16;

	if (r_affinetridesc.drawtype) {
		D_PolysetUpdateTables ();		// FIXME: precalc...
	} else {
#ifdef USE_INTEL_ASM
		D_Aff8Patch (acolormap);
#endif
	}
}

static void
R_IQMPrepareUnclippedPoints (iqm_t *iqm, swiqm_t *sw, iqmframe_t *frame)
{
	int         i;

	R_IQMTransformAndProjectFinalVerts (iqm, sw, frame);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (pfinalverts, iqm->num_verts);

	r_affinetridesc.pfinalverts = pfinalverts;
	for (i = 0; i < iqm->num_meshes; i++) {
		iqmmesh    *mesh = &iqm->meshes[i];
		uint16_t   *tris;

		iqm_setup_skin (sw, i);

		tris = iqm->elements + mesh->first_triangle;
		r_affinetridesc.ptriangles = (mtriangle_t *) tris;
		r_affinetridesc.numtriangles = mesh->num_triangles;
		D_PolysetDraw ();
	}
}

static void
R_IQMPreparePoints (iqm_t *iqm, swiqm_t *sw, iqmframe_t *frame)
{
	finalvert_t *fv = pfinalverts;
	auxvert_t  *av = pauxverts;
	int         i;
	uint32_t    j;
	finalvert_t *pfv[3];

	for (i = 0; i < iqm->num_verts; i++, fv++, av++) {
		byte       *vert = iqm->vertices + i * iqm->stride;
		uint32_t    bind = *(uint32_t *) (vert + sw->bindices->offset);
		vec_t      *mat = (vec_t *) &frame[bind];
		float      *position = (float *) (vert + sw->position->offset);
		float      *normal = (float *) (vert + sw->normal->offset);
		int32_t    *texcoord = (int32_t *) (vert + sw->texcoord->offset);
		vec3_t      tv, tn;
		Mat4MultVec (mat, position, tv);
		Mat4as3MultVec (mat, normal, tn);
		av->fv[0] = DotProduct (tv, aliastransform[0]) + aliastransform[0][3];
		av->fv[1] = DotProduct (tv, aliastransform[1]) + aliastransform[1][3];
		av->fv[2] = DotProduct (tv, aliastransform[2]) + aliastransform[2][3];
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->flags = 0;
		fv->v[4] = calc_light (tn);
		R_AliasClipAndProjectFinalVert (fv, av);
	}

	for (i = 0; i < iqm->num_meshes; i++) {
		iqmmesh    *mesh = &iqm->meshes[i];
		mtriangle_t *mtri;

		iqm_setup_skin (sw, i);

		mtri = (mtriangle_t *) iqm->elements + mesh->first_triangle;
		r_affinetridesc.numtriangles = 1;
		for (j = 0; j < mesh->num_triangles; j++, mtri++) {
			pfv[0] = &pfinalverts[mtri->vertindex[0]];
			pfv[1] = &pfinalverts[mtri->vertindex[1]];
			pfv[2] = &pfinalverts[mtri->vertindex[2]];

			if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags
				& (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
				continue;					// completely clipped

			if (!((pfv[0]->flags | pfv[1]->flags | pfv[2]->flags)
				  & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))) {// totally unclipped
				r_affinetridesc.pfinalverts = pfinalverts;
				r_affinetridesc.ptriangles = mtri;
				D_PolysetDraw ();
			} else {						// partially clipped
				R_AliasClipTriangle (mtri);
			}
		}
	}
}

static void
R_IQMSetupLighting (entity_t ent, alight_t *plighting)
{
	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't
	// have to clamp off the bottom
	r_ambientlight = plighting->ambientlight;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_shadelight = plighting->shadelight;

	if (r_shadelight < 0)
		r_shadelight = 0;

	r_shadelight *= VID_GRADES;

	// rotate the lighting vector into the model's frame of reference
	mat4f_t     mat;
	transform_t transform = Entity_Transform (ent);
	Transform_GetWorldMatrix (transform, mat);
	//FIXME vectorize
	r_lightvec[0] = DotProduct (plighting->lightvec, mat[0]);
	r_lightvec[1] = DotProduct (plighting->lightvec, mat[1]);
	r_lightvec[2] = DotProduct (plighting->lightvec, mat[2]);
}

static void
R_IQMSetUpTransform (entity_t ent, int trivial_accept)
{
	int         i;
	float       rotationmatrix[3][4];
	vec3_t      forward, left, up;

	mat4f_t     mat;
	transform_t transform = Entity_Transform (ent);
	Transform_GetWorldMatrix (transform, mat);
	VectorCopy (mat[0], forward);
	VectorCopy (mat[1], left);
	VectorCopy (mat[2], up);

// TODO: can do this with simple matrix rearrangement

	for (i = 0; i < 3; i++) {
		rotationmatrix[i][0] = forward[i];
		rotationmatrix[i][1] = left[i];
		rotationmatrix[i][2] = up[i];
	}

	rotationmatrix[0][3] = r_entorigin[0] - r_refdef.frame.position[0];
	rotationmatrix[1][3] = r_entorigin[1] - r_refdef.frame.position[1];
	rotationmatrix[2][3] = r_entorigin[2] - r_refdef.frame.position[2];

	R_ConcatTransforms (r_viewmatrix, rotationmatrix, aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?

	if (trivial_accept) {
		for (i = 0; i < 4; i++) {
			aliastransform[0][i] *= aliasxscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[1][i] *= aliasyscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[2][i] *= 1.0 / ((float) 0x8000 * 0x10000);
		}
	}
}

void
R_IQMDrawModel (entity_t ent, alight_t *plighting)
{
	renderer_t *renderer = Ent_GetComponent (ent.id, ent.base + scene_renderer, ent.reg);
	model_t    *model = renderer->model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;
	swiqm_t    *sw = (swiqm_t *) iqm->extra_data;
	int         size;
	float       blend;
	iqmframe_t *frame;

	size = (CACHE_SIZE - 1)
		+ sizeof (finalvert_t) * (iqm->num_verts + 1)
		+ sizeof (auxvert_t) * iqm->num_verts;

	animation_t *animation = Ent_GetComponent (ent.id, ent.base + scene_animation,
											   ent.reg);
	blend = R_IQMGetLerpedFrames (animation, iqm);
	frame = R_IQMBlendPalette (iqm, animation->pose1, animation->pose2,
							   blend, size, sw->blend_palette,
							   sw->palette_size);

	pfinalverts = (finalvert_t *) &frame[sw->palette_size];
	pfinalverts = (finalvert_t *)
		(((intptr_t) &pfinalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = (auxvert_t *) &pfinalverts[iqm->num_verts + 1];

	visibility_t *visibility = Ent_GetComponent (ent.id, ent.base + scene_visibility,
												 ent.reg);
	R_IQMSetUpTransform (ent, visibility->trivial_accept);

	R_IQMSetupLighting (ent, plighting);
	r_affinetridesc.drawtype = (visibility->trivial_accept == 3) &&
			r_recursiveaffinetriangles;

	//if (!acolormap)
		acolormap = r_colormap;

	//FIXME depth hack
	if (ent.id != vr_data.view_model.id)
		ziscale = (float) 0x8000 *(float) 0x10000;
	else
		ziscale = (float) 0x8000 *(float) 0x10000 *3.0;

	if (visibility->trivial_accept)
		R_IQMPrepareUnclippedPoints (iqm, sw, frame);
	else
		R_IQMPreparePoints (iqm, sw, frame);
}
