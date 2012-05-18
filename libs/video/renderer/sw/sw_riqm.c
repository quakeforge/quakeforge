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
		vec3_t      tv;
		Mat4MultVec (mat, position, tv);
		zi = 1.0 / tv[2];
		fv->v[5] = zi;
		fv->v[0] = tv[0] * zi + aliasxcenter;
		fv->v[1] = tv[1] * zi + aliasxcenter;
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->v[4] = calc_light (normal);
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
		tex_t      *skin = sw->skins[i];
		maliasskindesc_t skindesc = {0, 0, 0, 0};

		// setup skin
		r_affinetridesc.pskindesc = &skindesc;
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

		Mat4MultVec (mat, position, av->fv);
		fv->v[2] = texcoord[0];
		fv->v[3] = texcoord[1];
		fv->flags = 0;
		fv->v[4] = calc_light (normal);
		R_AliasClipAndProjectFinalVert (fv, av);
	}

	for (i = 0; i < iqm->num_meshes; i++) {
		iqmmesh    *mesh = &iqm->meshes[i];
		mtriangle_t *mtri;
		tex_t      *skin = sw->skins[i];
		maliasskindesc_t skindesc = {0, 0, 0, 0};

		// setup skin
		r_affinetridesc.pskindesc = &skindesc;
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

// NOTE: in1 is row major but in2 is column major
// WARNING: will NOT work if in2 and out are the same location
static void
iqm_concat_transforms (float in1[3][4], const mat4_t in2, mat4_t out)
{
	out[0]  = DotProduct (in1[0], in2 + 0);
	out[1]  = DotProduct (in1[1], in2 + 0);
	out[2]  = DotProduct (in1[2], in2 + 0);
	out[3]  = 0;
	out[4]  = DotProduct (in1[0], in2 + 4);
	out[5]  = DotProduct (in1[1], in2 + 4);
	out[6]  = DotProduct (in1[2], in2 + 4);
	out[7]  = 0;
	out[8]  = DotProduct (in1[0], in2 + 8);
	out[9]  = DotProduct (in1[1], in2 + 8);
	out[10] = DotProduct (in1[2], in2 + 8);
	out[11] = 0;
	out[12] = DotProduct (in1[0], in2 + 12) + in1[0][3];
	out[13] = DotProduct (in1[1], in2 + 12) + in1[1][3];
	out[14] = DotProduct (in1[2], in2 + 12) + in1[2][3];
	out[15] = 1;
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
	r_plightvec[0] = DotProduct (plighting->plightvec, ent->transform + 0);
	r_plightvec[1] = DotProduct (plighting->plightvec, ent->transform + 4);
	r_plightvec[2] = DotProduct (plighting->plightvec, ent->transform + 8);
}

void
R_IQMDrawModel (alight_t *plighting)
{
	entity_t   *ent = currententity;
	model_t    *model = ent->model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;
	swiqm_t    *sw = (swiqm_t *) iqm->extra_data;
	int         size;
	float       blend;
	iqmframe_t *frame;
	int         i, j;

	size = (sw->palette_size - iqm->num_joints) * sizeof (iqmframe_t);
	size += (CACHE_SIZE - 1)
		+ sizeof (finalvert_t) * (iqm->num_verts + 1)
		+ sizeof (auxvert_t) * iqm->num_verts;
	blend = R_IQMGetLerpedFrames (ent, iqm);
	frame = R_IQMBlendFrames (iqm, ent->pose1, ent->pose2, blend, size);
	pfinalverts = (finalvert_t *) &frame[sw->palette_size];
	pfinalverts = (finalvert_t *)
		(((intptr_t) &pfinalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = (auxvert_t *) &pfinalverts[iqm->num_verts + 1];
	
	R_AliasSetUpTransform (ent->trivial_accept);

	for (i = iqm->num_joints; i < sw->palette_size; i++) {
		vec_t      *mat = (vec_t *) &frame[i];
		mat4_t      tmat;
		iqmblend_t *blend = &sw->blend_palette[i];
		vec_t      *f;

		f = (vec_t *) &frame[blend->indices[0]];
		Mat4Scale (f, blend->weights[0] / 255.0, tmat);
		for (j = 1; j < 4; j++) {
			if (!blend->weights[j])
				break;
			f = (vec_t *) &frame[blend->indices[j]];
			Mat4MultAdd (tmat, blend->weights[j] / 255.0, f, tmat);
		}
		iqm_concat_transforms (aliastransform, tmat, mat);
	}

	R_IQMSetupLighting (ent, plighting);
	r_affinetridesc.drawtype = (ent->trivial_accept == 3) &&
			r_recursiveaffinetriangles;

	//if (!acolormap)
		acolormap = vid.colormap8;

	if (ent != vr_data.view_model)
		ziscale = (float) 0x8000 *(float) 0x10000;
	else
		ziscale = (float) 0x8000 *(float) 0x10000 *3.0;

	if (ent->trivial_accept)
		R_IQMPrepareUnclippedPoints (iqm, sw, frame);
	else
		R_IQMPreparePoints (iqm, sw, frame);
}
