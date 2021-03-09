/*
	sw32_riqm.c

	32bit SW IQM rendering

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

#define NH_DEFINE
#include "namehack.h"

#include "QF/cvar.h"
#include "QF/entity.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "d_ifacea.h"
#include "r_internal.h"

#define LIGHT_MIN	5					// lowest light value we'll allow, to
										// avoid the need for inner-loop light
										// clamping

static vec3_t r_plightvec;
static int    r_ambientlight;
static float  r_shadelight;

static inline int
calc_light (float *normal)
{
	float       lightcos = DotProduct (normal, r_plightvec);
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
		zi = 1.0 / (DotProduct (tv, sw32_aliastransform[2])
					+ sw32_aliastransform[2][3]);
		fv->v[5] = zi;
		fv->v[0] = (DotProduct (tv, sw32_aliastransform[0])
					+ sw32_aliastransform[0][3]) * zi + aliasxcenter;
		fv->v[1] = (DotProduct (tv, sw32_aliastransform[1])
					+ sw32_aliastransform[1][3]) * zi + aliasxcenter;
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->v[4] = calc_light (tn);
	}
}

static void
iqm_setup_skin (swiqm_t *sw, int skinnum)
{
	tex_t      *skin = sw->skins[skinnum];

	sw32_r_affinetridesc.pskin = skin->data;
	sw32_r_affinetridesc.skinwidth = skin->width;
	sw32_r_affinetridesc.skinheight = skin->height;
	sw32_r_affinetridesc.seamfixupX16 = (skin->width >> 1) << 16;
}

static void
R_IQMPrepareUnclippedPoints (iqm_t *iqm, swiqm_t *sw, iqmframe_t *frame)
{
	int         i;

	R_IQMTransformAndProjectFinalVerts (iqm, sw, frame);

	sw32_r_affinetridesc.pfinalverts = pfinalverts;
	for (i = 0; i < iqm->num_meshes; i++) {
		iqmmesh    *mesh = &iqm->meshes[i];
		uint16_t   *tris;

		iqm_setup_skin (sw, i);

		tris = iqm->elements + mesh->first_triangle;
		sw32_r_affinetridesc.ptriangles = (mtriangle_t *) tris;
		sw32_r_affinetridesc.numtriangles = mesh->num_triangles;
		sw32_D_PolysetDraw ();
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
		av->fv[0] = DotProduct (tv, sw32_aliastransform[0])
			+ sw32_aliastransform[0][3];
		av->fv[1] = DotProduct (tv, sw32_aliastransform[1])
			+ sw32_aliastransform[1][3];
		av->fv[2] = DotProduct (tv, sw32_aliastransform[2])
			+ sw32_aliastransform[2][3];
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->flags = 0;
		fv->v[4] = calc_light (tn);
		sw32_R_AliasClipAndProjectFinalVert (fv, av);
	}

	for (i = 0; i < iqm->num_meshes; i++) {
		iqmmesh    *mesh = &iqm->meshes[i];
		mtriangle_t *mtri;

		iqm_setup_skin (sw, i);

		mtri = (mtriangle_t *) iqm->elements + mesh->first_triangle;
		sw32_r_affinetridesc.numtriangles = 1;
		for (j = 0; j < mesh->num_triangles; j++, mtri++) {
			pfv[0] = &pfinalverts[mtri->vertindex[0]];
			pfv[1] = &pfinalverts[mtri->vertindex[1]];
			pfv[2] = &pfinalverts[mtri->vertindex[2]];

			if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags
				& (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
				continue;					// completely clipped

			if (!((pfv[0]->flags | pfv[1]->flags | pfv[2]->flags)
				  & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))) {// totally unclipped
				sw32_r_affinetridesc.pfinalverts = pfinalverts;
				sw32_r_affinetridesc.ptriangles = mtri;
				sw32_D_PolysetDraw ();
			} else {						// partially clipped
				sw32_R_AliasClipTriangle (mtri);
			}
		}
	}
}

static void
R_IQMSetupLighting (entity_t *ent, alight_t *plighting)
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
	Transform_GetWorldMatrix (ent->transform, mat);
	//FIXME vectorize
	r_plightvec[0] = DotProduct (plighting->plightvec, mat[0]);
	r_plightvec[1] = DotProduct (plighting->plightvec, mat[1]);
	r_plightvec[2] = DotProduct (plighting->plightvec, mat[2]);
}

static void
R_IQMSetUpTransform (int trivial_accept)
{
	int         i;
	float       rotationmatrix[3][4];
	static float viewmatrix[3][4];
	vec3_t      forward, left, up;

	mat4f_t     mat;
	Transform_GetWorldMatrix (currententity->transform, mat);
	VectorCopy (mat[0], forward);
	VectorCopy (mat[1], left);
	VectorCopy (mat[2], up);

// TODO: can do this with simple matrix rearrangement

	for (i = 0; i < 3; i++) {
		rotationmatrix[i][0] = forward[i];
		rotationmatrix[i][1] = left[i];
		rotationmatrix[i][2] = up[i];
	}

	rotationmatrix[0][3] = -modelorg[0];
	rotationmatrix[1][3] = -modelorg[1];
	rotationmatrix[2][3] = -modelorg[2];

// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorCopy (vup, viewmatrix[1]);
	VectorNegate (viewmatrix[1], viewmatrix[1]);
	VectorCopy (vpn, viewmatrix[2]);

//	viewmatrix[0][3] = 0;
//	viewmatrix[1][3] = 0;
//	viewmatrix[2][3] = 0;

	R_ConcatTransforms (viewmatrix, rotationmatrix, sw32_aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?

	if (trivial_accept) {
		for (i = 0; i < 4; i++) {
			sw32_aliastransform[0][i] *= aliasxscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			sw32_aliastransform[1][i] *= aliasyscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			sw32_aliastransform[2][i] *= 1.0 / ((float) 0x8000 * 0x10000);
		}
	}
}

void
sw32_R_IQMDrawModel (alight_t *plighting)
{
	entity_t   *ent = currententity;
	model_t    *model = ent->renderer.model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;
	swiqm_t    *sw = (swiqm_t *) iqm->extra_data;
	int         size;
	float       blend;
	iqmframe_t *frame;

	size = (CACHE_SIZE - 1)
		+ sizeof (finalvert_t) * (iqm->num_verts + 1)
		+ sizeof (auxvert_t) * iqm->num_verts;
	blend = R_IQMGetLerpedFrames (ent, iqm);
	frame = R_IQMBlendPalette (iqm, ent->animation.pose1, ent->animation.pose2,
							   blend, size,
							   sw->blend_palette, sw->palette_size);

	pfinalverts = (finalvert_t *) &frame[sw->palette_size];
	pfinalverts = (finalvert_t *)
		(((intptr_t) &pfinalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = (auxvert_t *) &pfinalverts[iqm->num_verts + 1];

	R_IQMSetUpTransform (ent->visibility.trivial_accept);

	R_IQMSetupLighting (ent, plighting);

	//if (!sw32_acolormap)
		sw32_acolormap = vid.colormap8;

	if (ent != vr_data.view_model)
		sw32_ziscale = (float) 0x8000 *(float) 0x10000;
	else
		sw32_ziscale = (float) 0x8000 *(float) 0x10000 *3.0;

	if (ent->visibility.trivial_accept)
		R_IQMPrepareUnclippedPoints (iqm, sw, frame);
	else
		R_IQMPreparePoints (iqm, sw, frame);
}
