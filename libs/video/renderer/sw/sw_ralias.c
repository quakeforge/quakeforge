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

//FIXME needed by sw_raliasa.S trivertx_t *r_apverts;

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
get_frame (animation_t *animation, void *base)
{
	// pose2 points to the frame data
	return (qfm_frame_t *) ((byte *) base + animation->pose2);
}

static bool
check_bounds (bool *zclipped, unsigned *anyclip, int *minz, qfm_frame_t *frame)
{
	unsigned    flags;
	int         i, numv;
	float       zi, basepts[8][3], v0, v1, frac;
	finalvert_t *pv0, *pv1, viewpts[16];
	auxvert_t  *pa0, *pa1, viewaux[16];
	bool        zfullyclipped;
	unsigned    allclip;


	// construct the base bounding box for this frame
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

	zfullyclipped = true;

	for (i = 0; i < 8; i++) {
		R_AliasTransformVector (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE) {
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			*zclipped = true;
		} else {
			if (viewaux[i].fv[2] < *minz)
				*minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	if (zfullyclipped) {
		return false;					// everything was near-z-clipped
	}

	numv = 8;

	if (*zclipped) {
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

		*anyclip |= flags;
		allclip &= flags;
	}

	if (allclip) {
		return false;					// trivial reject off one side
	}
	return true;
}

bool
R_AliasCheckBBox (entity_t ent)
{
	bool        zclipped = false;
	unsigned    anyclip = 0;
	int         minz = 9999;
	bool        accept = true;

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

	auto animation = Entity_GetAnimation (ent);
	if (model->anim.numclips) {
		R_AliasSetUpTransform (ent, 0, nullptr);
		auto frame = (qfm_frame_t *) ((byte *) model + model->anim.data);
		//auto frame = (qfm_frame_t *) ((byte *) model + model->anim.data);
		//frame += animation->pose2;
		if (!check_bounds (&zclipped, &anyclip, &minz, frame)) {
			accept = false;
			goto release;
		}
	} else {
		auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
		accept = false;
		for (uint32_t i = 0; i < model->meshes.count; i++) {
			R_AliasSetUpTransform (ent, 0, &meshes[i]);

			if (meshes[i].morph.numclips) {
				auto frame = get_frame (animation, &meshes[i]);
				if (check_bounds (&zclipped, &anyclip, &minz, frame)) {
					accept = true;
				}
			} else {
				qfm_frame_t frame = {
					.bounds_min = { VectorExpand (meshes[i].bounds_min) },
					.bounds_max = { VectorExpand (meshes[i].bounds_max) },
				};
				if (check_bounds (&zclipped, &anyclip, &minz, &frame)) {
					accept = true;
				}
			}
		}
		if (!accept) {
			goto release;
		}
	}

	visibility->trivial_accept = !anyclip & !zclipped;

	if (visibility->trivial_accept) {
		auto rmesh = (sw_mesh_t *) ((byte *) model + model->render_data);
		if (minz > (r_aliastransition + (rmesh->size * r_resfudge))) {
			visibility->trivial_accept |= 2;
		}
	}
release:
	if (!pmodel->model) {
		Cache_Release (&pmodel->cache);
	}
	return accept;
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
av_v48 (auxvert_t *av, const trivertx16_t *vert)
{
	vec3_t v;
	VectorScale (vert->v, 1.0/256, v);
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
av_v48_b (auxvert_t *av, const trivertx16_t *vert, mat4f_t m)
{
	vec4f_t v = { 0, 0, 0, 1 };
	VectorScale (vert->v, 1.0/256, v);
	v = mvmulf (m, v);
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
av_v96 (auxvert_t *av, const float *v)
{
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
av_v96_b (auxvert_t *av, const float *v3, mat4f_t m)
{
	auto v = loadvec3f (v3) + (vec4f_t) { 0, 0, 0, 1 };
	v = mvmulf (m, v);
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
av_v24 (auxvert_t *av, const trivertx_t *vert)
{
	auto v = vert->v;
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void
av_v24_b (auxvert_t *av, const trivertx_t *vert, mat4f_t m)
{
	vec4f_t v = { VectorExpand (vert->v), 1};
	v = mvmulf (m, v);
	av->fv[0] = DotProduct (v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct (v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct (v, aliastransform[2]) + aliastransform[2][3];
}

static void R_AliasTransformFinalVert (finalvert_t *fv, const float *normal,
									   const stvert_t *pstverts);

static inline void
fv_av_ni (finalvert_t *fv, int lightnormalindex, const stvert_t *stverts)
{
	R_AliasTransformFinalVert (fv, r_avertexnormals[lightnormalindex], stverts);
}

static inline void
fv_av_ni_b (finalvert_t *fv, int lightnormalindex, const stvert_t *stverts,
			mat4f_t m)
{
	auto n = loadvec3f (r_avertexnormals[lightnormalindex]);
	n = mvmulf (m, n) * m[3][0];
	R_AliasTransformFinalVert (fv, (vec_t*)&n, stverts);//FIXME
}

static inline void
fv_av_n96 (finalvert_t *fv, const float *normal, const stvert_t *stverts)
{
	R_AliasTransformFinalVert (fv, normal, stverts);
}

static inline void
fv_av_n96_b (finalvert_t *fv, const float *normal, const stvert_t *stverts,
			 mat4f_t m)
{
	auto n = loadvec3f (normal);
	n = mvmulf (m, n) * m[3][0];
	R_AliasTransformFinalVert (fv, (vec_t*)&n, stverts);//FIXME
}

static void
run_clipped_verts_bones (finalvert_t *fv, auxvert_t *av, qf_mesh_t *mesh,
						 uint32_t verts_offs, mat4f_t *palette)
{
	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t stoffs = mesh->vertices.offset * attrib[2].stride;
	auto stverts = (stvert_t *) ((byte*) mesh + attrib[2].offset + stoffs);

	if (attrib[0].type == qfm_u16) {
		int32_t b_diff = attrib[3].offset - attrib[0].offset;
		auto verts = (trivertx16_t *) ((byte *) mesh + verts_offs);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		for (int i = 0; i < r_anumverts; i++, fv++, av++, verts++, stverts++) {
			auto m = &palette[*joints * 2];
			av_v48_b (av, verts, m[0]);
			fv_av_ni_b (fv, verts->lightnormalindex, stverts, m[1]);
			R_AliasClipAndProjectFinalVert (fv, av);
			joints = (uint32_t *)((uintptr_t) joints + attrib[3].stride);
		}
	} else if (attrib[0].type == qfm_f32) {
		int32_t n_diff = attrib[1].offset - attrib[0].offset;
		int32_t b_diff = attrib[3].offset - attrib[0].offset;
		auto verts = (float *) ((byte *) mesh + verts_offs);
		auto norms = (float *) ((byte *) mesh + verts_offs + n_diff);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		uint32_t stride = attrib[0].stride;
		for (int i = 0; i < r_anumverts; i++, fv++, av++) {
			auto m = &palette[*joints * 2];
			av_v96_b (av, verts, m[0]);
			fv_av_n96_b (fv, norms, stverts, m[1]);
			R_AliasClipAndProjectFinalVert (fv, av);
			verts = (float *)((uintptr_t) verts + stride);
			norms = (float *)((uintptr_t) norms + stride);
			stverts = (stvert_t *)((uintptr_t) stverts + attrib[2].stride);
			joints = (uint32_t *)((uintptr_t) joints + attrib[3].stride);
		}
	} else if (attrib[0].type == qfm_u8) {
		int32_t b_diff = attrib[3].offset - attrib[0].offset;
		auto verts = (trivertx_t *) ((byte *) mesh + verts_offs);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		for (int i = 0; i < r_anumverts; i++, fv++, av++, verts++, stverts++) {
			auto m = &palette[*joints * 2];
			av_v24_b (av, verts, m[0]);
			fv_av_ni_b (fv, verts->lightnormalindex, stverts, m[1]);
			R_AliasClipAndProjectFinalVert (fv, av);
			joints = (uint32_t *)((uintptr_t) joints + attrib[3].stride);
		}
	} else {
		Sys_Error ("unspported position type");
	}
}

static void
run_clipped_verts (finalvert_t *fv, auxvert_t *av, qf_mesh_t *mesh,
				   uint32_t verts_offs)
{
	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t stoffs = mesh->vertices.offset * attrib[2].stride;
	auto stverts = (stvert_t *) ((byte*) mesh + attrib[2].offset + stoffs);

	if (attrib[0].type == qfm_u16) {
		auto verts = (trivertx16_t *) ((byte *) mesh + verts_offs);
		for (int i = 0; i < r_anumverts; i++, fv++, av++, verts++, stverts++) {
			av_v48 (av, verts);
			fv_av_ni (fv, verts->lightnormalindex, stverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	} else if (attrib[0].type == qfm_f32) {
		auto verts = (float *) ((byte *) mesh + verts_offs);
		auto norms = (float *) ((byte *) mesh + verts_offs) + 6;
		uint32_t stride = attrib[0].stride;
		for (int i = 0; i < r_anumverts; i++, fv++, av++) {
			av_v96 (av, verts);
			fv_av_n96 (fv, norms, stverts);
			R_AliasClipAndProjectFinalVert (fv, av);
			verts = (float *)((uintptr_t) verts + stride);
			norms = (float *)((uintptr_t) norms + stride);
			stverts = (stvert_t *)((uintptr_t) stverts + attrib[2].stride);
		}
	} else if (attrib[0].type == qfm_u8) {
		auto verts = (trivertx_t *) ((byte *) mesh + verts_offs);
		for (int i = 0; i < r_anumverts; i++, fv++, av++, verts++, stverts++) {
			av_v24 (av, verts);
			fv_av_ni (fv, verts->lightnormalindex, stverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	} else {
		Sys_Error ("unspported position type");
	}
}

/*
	R_AliasPreparePoints

	General clipped case
*/
static void
R_AliasPreparePoints (finalvert_t *fv, auxvert_t *av, qf_mesh_t *mesh,
					  uint32_t verts_offs, mat4f_t *palette)
{
	auto attrib = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	r_anumverts = mesh->vertices.count;

	verts_offs += attrib[0].offset;
	if (palette && mesh->attributes.count > 3
		&& attrib[3].attr == qfm_joints
		&& attrib[3].type == qfm_u32
		&& attrib[3].components == 1) {
		run_clipped_verts_bones (fv, av, mesh, verts_offs, palette);
	} else {
		run_clipped_verts (fv, av, mesh, verts_offs);
	}
}

static void
draw_clipped_triangles (qf_mesh_t *mesh)
{
	// clip and draw all triangles
	r_affinetridesc.numtriangles = 1;

	auto ptri = (dtriangle_t *) ((byte *) mesh + mesh->indices);
	for (uint32_t i = 0; i < mesh->triangle_count; i++, ptri++) {
		finalvert_t *pfv[3] = {
			&pfinalverts[ptri->vertindex[0]],
			&pfinalverts[ptri->vertindex[1]],
			&pfinalverts[ptri->vertindex[2]],
		};

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
	vec3_t      scale  = { 1, 1, 1 };
	vec3_t      scale_origin = { 0, 0, 0 };

	if (mesh) {
		VectorCopy (mesh->scale, scale);
		VectorCopy (mesh->scale_origin, scale_origin);
	}

	mat4f_t     mat;
	Transform_GetWorldMatrix (transform, mat);
	VectorCopy (mat[0], alias_forward);
	VectorCopy (mat[1], alias_left);
	VectorCopy (mat[2], alias_up);

	for (i = 0; i < 3; i++) {
		rotationmatrix[i][0] = scale[0] * alias_forward[i];
		rotationmatrix[i][1] = scale[1] * alias_left[i];
		rotationmatrix[i][2] = scale[2] * alias_up[i];
		rotationmatrix[i][3] = scale_origin[0] * alias_forward[i]
							 + scale_origin[1] * alias_left[i]
							 + scale_origin[2] * alias_up[i]
							 + r_entorigin[i] - r_refdef.frame.position[i];
	}

	R_ConcatTransforms (r_viewmatrix, rotationmatrix, aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?

	if (trivial_accept) {
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
static void
R_AliasTransformFinalVert (finalvert_t *fv, const float *normal,
						   const stvert_t *pstverts)
{
	int         temp;
	float       lightcos;

	fv->v[2] = pstverts->s;
	fv->v[3] = pstverts->t;

	fv->flags = pstverts->onseam;

	// lighting
	lightcos = DotProduct (normal, r_lightvec);
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

static void
fv_v48_n16 (finalvert_t *fv, const trivertx16_t *verts, const stvert_t *stverts)
{
	int         temp;
	float       lightcos, *plightnormal, zi;
	float       s = 1.0/256;

	for (int i = 0; i < r_anumverts; i++, fv++, verts++, stverts++) {
		// transform and project
		zi = 1.0 / (s * DotProduct (verts->v, aliastransform[2]) +
					aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((s * DotProduct (verts->v, aliastransform[0]) +
					 aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((s * DotProduct (verts->v, aliastransform[1]) +
					 aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		plightnormal = r_avertexnormals[verts->lightnormalindex];
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

static void
fv_v48_n16_b (finalvert_t *fv, const trivertx16_t *verts,
			  const stvert_t *stverts, const uint32_t *joints,
			  mat4f_t *palette, qfm_attrdesc_t *attr)
{
	int         temp;
	float       lightcos, zi;

	for (int i = 0; i < r_anumverts; i++, fv++, verts++, stverts++) {
		auto m = &palette[*joints * 2];
		joints = (uint32_t *) ((uintptr_t) joints + attr[3].stride);
		vec4f_t v = { 0, 0, 0, 1 };
		VectorScale (verts->v, 1.0/256, v);
		v = mvmulf (m[0], v);
		// transform and project
		zi = 1.0 / (DotProduct (v, aliastransform[2]) + aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (v, aliastransform[0]) +
					aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (v, aliastransform[1]) +
					aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		auto n = loadvec3f (r_avertexnormals[verts->lightnormalindex]);
		n = mvmulf (m[1], n) * m[1][3][0];
		lightcos = DotProduct (n, r_lightvec);
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

static void
fv_v24_n8 (finalvert_t *fv, const trivertx_t *verts, const stvert_t *stverts)
{
	int         temp;
	float       lightcos, *plightnormal, zi;

	for (int i = 0; i < r_anumverts; i++, fv++, verts++, stverts++) {
		// transform and project
		zi = 1.0 / (DotProduct (verts->v, aliastransform[2]) +
					aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (verts->v, aliastransform[0]) +
					 aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (verts->v, aliastransform[1]) +
					 aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		plightnormal = r_avertexnormals[verts->lightnormalindex];
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

static void
fv_v24_n8_b (finalvert_t *fv, const trivertx_t *verts, const stvert_t *stverts,
			 uint32_t *joints, mat4f_t *palette, qfm_attrdesc_t *attr)
{
	int         temp;
	float       lightcos, zi;

	for (int i = 0; i < r_anumverts; i++, fv++, verts++, stverts++) {
		auto m = &palette[*joints * 2];
		joints = (uint32_t *) ((uintptr_t) joints + attr[3].stride);

		vec4f_t v = { VectorExpand (verts->v), 1};
		v = mvmulf (m[0], v);

		// transform and project
		zi = 1.0 / (DotProduct (v, aliastransform[2]) + aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (v, aliastransform[0]) +
					aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (v, aliastransform[1]) +
					aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		auto n = loadvec3f (r_avertexnormals[verts->lightnormalindex]);
		n = mvmulf (m[1], n) * m[1][3][0];
		lightcos = DotProduct (n, r_lightvec);
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

static void
fv_v96_n96 (finalvert_t *fv, const float *position, const float *normal,
			uint32_t stride, const stvert_t *stverts)
{
	int         temp;
	float       lightcos, zi;

	for (int i = 0; i < r_anumverts; i++, fv++) {
		// transform and project
		zi = 1.0 / (DotProduct (position, aliastransform[2]) +
					aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (position, aliastransform[0]) +
					 aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (position, aliastransform[1]) +
					 aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		lightcos = DotProduct (normal, r_lightvec);
		temp = r_ambientlight;

		if (lightcos < 0) {
			temp += (int) (r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading
			// light, we don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->v[4] = temp;
		position = (float *) ((uintptr_t) position + stride);
		normal = (float *) ((uintptr_t) normal + stride);
		stverts = (stvert_t *) ((uintptr_t) stverts + stride);
	}
}

static void
fv_v96_n96_b (finalvert_t *fv, const float *position, const float *normal,
			  const stvert_t *stverts, const uint32_t *joints,
			  mat4f_t *palette, qfm_attrdesc_t *attr)
{
	int         temp;
	float       lightcos, zi;

	for (int i = 0; i < r_anumverts; i++, fv++) {
		auto m = &palette[*joints * 2];
		auto v = loadvec3f (position) + (vec4f_t) { 0, 0, 0, 1 };
		v = mvmulf (m[0], v);
		// transform and project
		zi = 1.0 / (DotProduct (v, aliastransform[2]) + aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (v, aliastransform[0]) +
					aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (v, aliastransform[1]) +
					aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = stverts->s;
		fv->v[3] = stverts->t;
		fv->flags = stverts->onseam;

		// lighting
		auto n = loadvec3f (normal);
		n = mvmulf (m[1], n) * m[3][0];
		lightcos = DotProduct (n, r_lightvec);
		temp = r_ambientlight;

		if (lightcos < 0) {
			temp += (int) (r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading
			// light, we don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->v[4] = temp;
		position = (float *) ((uintptr_t) position + attr[0].stride);
		normal = (float *) ((uintptr_t) normal + attr[1].stride);
		stverts = (stvert_t *) ((uintptr_t) stverts + attr[2].stride);
		joints = (uint32_t *) ((uintptr_t) joints + attr[3].stride);
	}
}

static void
run_unclipped_verts_bones (finalvert_t *fv, qf_mesh_t *mesh,
						   uint32_t verts_offs, mat4f_t *palette)
{
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t stoffs = mesh->vertices.offset * attr[2].stride;
	auto stverts = (stvert_t *) ((byte*) mesh + attr[2].offset + stoffs);

	if (attr[0].type == qfm_u16 && attr[0].components == 3
	    && attr[1].type == qfm_u16 && attr[1].components == 1) {
		int32_t b_diff = attr[3].offset - attr[0].offset;
		auto verts = (trivertx16_t *) ((byte*) mesh + verts_offs);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		fv_v48_n16_b (fv, verts, stverts, joints, palette, attr);
	} else if (attr[0].type == qfm_f32 && attr[0].components == 3
			   && attr[1].type == qfm_f32 && attr[1].components == 3
			   && attr[0].stride == attr[1].stride) {
		int32_t n_diff = attr[1].offset - attr[0].offset;
		int32_t b_diff = attr[3].offset - attr[0].offset;
		auto position = (float *) ((byte*) mesh + verts_offs);
		auto normal = (float *) ((byte*) mesh + verts_offs + n_diff);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		fv_v96_n96_b (fv, position, normal, stverts, joints, palette, attr);
	} else if (attr[0].type == qfm_u8 && attr[0].components == 3
			   && attr[1].type == qfm_u8 && attr[1].components == 1) {
		int32_t b_diff = attr[3].offset - attr[0].offset;
		auto verts = (trivertx_t *) ((byte*) mesh + verts_offs);
		auto joints = (uint32_t *) ((byte *) mesh + verts_offs + b_diff);
		fv_v24_n8_b (fv, verts, stverts, joints, palette, attr);
	}
}

static void
run_unclipped_verts (finalvert_t *fv, qf_mesh_t *mesh, uint32_t verts_offs)
{
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t stoffs = mesh->vertices.offset * attr[2].stride;
	auto stverts = (stvert_t *) ((byte*) mesh + attr[2].offset + stoffs);

	if (attr[0].type == qfm_u16 && attr[0].components == 3
	    && attr[1].type == qfm_u16 && attr[1].components == 1) {
		auto verts = (trivertx16_t *) ((byte*) mesh + verts_offs);
		fv_v48_n16 (fv, verts, stverts);
	} else if (attr[0].type == qfm_f32 && attr[0].components == 3
			   && attr[1].type == qfm_f32 && attr[1].components == 3
			   && attr[0].stride == attr[1].stride) {
		int32_t n_diff = attr[1].offset - attr[0].offset;
		auto position = (float *) ((byte*) mesh + verts_offs);
		auto normal = (float *) ((byte*) mesh + verts_offs + n_diff);
		fv_v96_n96 (fv, position, normal, attr[0].stride, stverts);
	} else if (attr[0].type == qfm_u8 && attr[0].components == 3
			   && attr[1].type == qfm_u8 && attr[1].components == 1) {
		auto verts = (trivertx_t *) ((byte*) mesh + verts_offs);
		fv_v24_n8 (fv, verts, stverts);
	}
}

//#ifndef USE_INTEL_ASM
static void
R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, qf_mesh_t *mesh,
									  uint32_t verts_offs, mat4f_t *palette)
{
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	if (attr[0].attr != qfm_position
		|| attr[1].attr != qfm_normal
		|| attr[2].attr != qfm_texcoord) {
		Sys_Error ("unsupported layout");
	}
	if (attr[2].type != qfm_s32 || attr[2].components != 3) {
		Sys_Error ("unsupported texture coords (expecting stvert_t)");
	}

	verts_offs += attr[0].offset;
	if (palette && mesh->attributes.count > 3
		&& attr[3].attr == qfm_joints
		&& attr[3].type == qfm_u32
		&& attr[3].components == 1) {
		run_unclipped_verts_bones (fv, mesh, verts_offs, palette);
	} else {
		run_unclipped_verts (fv, mesh, verts_offs);
	}
}
//#endif

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
R_AliasPrepareUnclippedPoints (finalvert_t *fv, qf_mesh_t *mesh,
							   uint32_t verts_offs, mat4f_t *palette)
{
	r_anumverts = mesh->vertices.count;

	R_AliasTransformAndProjectFinalVerts (fv, mesh, verts_offs, palette);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (pfinalverts, r_anumverts);

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
			r_affinetridesc.seamfixupX16 = (tex->width >> 1) << 16;
			r_affinetridesc.skinheight = tex->height;
		}
	}
	if (!r_affinetridesc.skin) {
		qpic_t *skinpic;
		if (renderer->skindesc) {
			skinpic = (qpic_t *) ((byte *) mesh + renderer->skindesc);
		} else {
			skinpic = (qpic_t *) ((byte *) mesh + mesh->skin.data);
		}
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

static uint32_t
R_AliasSetupFrame (entity_t ent, qf_mesh_t *mesh)
{
	auto animation = Entity_GetAnimation (ent);
	auto frame = get_frame (animation, mesh);
	return frame->data;
}

static void
draw_mesh (entity_t ent, alight_t *lighting, qf_mesh_t *mesh,
		   finalvert_t *fv, auxvert_t *av, mat4f_t *palette)
{
	R_AliasSetupSkin (ent, mesh);
	visibility_t *visibility = Ent_GetComponent (ent.id,
												 ent.base + scene_visibility,
												 ent.reg);
	R_AliasSetUpTransform (ent, visibility->trivial_accept, mesh);
	R_AliasSetupLighting (lighting);
	uint32_t verts_offs = mesh->vertices.offset * mesh->vertex_stride;
	if (mesh->morph.numclips) {
		verts_offs = R_AliasSetupFrame (ent, mesh);
	}

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

	if (visibility->trivial_accept) {
		R_AliasPrepareUnclippedPoints (fv, mesh, verts_offs, palette);
	} else {
		R_AliasPreparePoints (fv, av, mesh, verts_offs, palette);
		draw_clipped_triangles (mesh);
	}
}

void
R_AliasDrawModel (entity_t ent, alight_t *lighting)
{
	r_amodels_drawn++;

	auto renderer = Entity_GetRenderer (ent);
	if (renderer->onlyshadows) {
		return;
	}
	auto model = renderer->model->model;
	if (!model) {
		model = Cache_Get (&renderer->model->cache);
	}
	auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	auto rmesh = (sw_mesh_t *) ((byte *) model + model->render_data);

	size_t size = (CACHE_SIZE - 1)
				+ sizeof (finalvert_t) * (rmesh->numverts + 1)
				+ sizeof (auxvert_t) * rmesh->numverts;
	finalvert_t *finalverts;
	mat4f_t *palette = nullptr;
	if (rmesh->palette_size) {
		auto blend_palette = (qfm_blend_t *) ((byte *) model
											  + rmesh->blend_palette);
		auto anim = Entity_GetAnimation (ent);
		float blend = R_IQMGetLerpedFrames (vr_data.realtime, anim, model);
		palette = R_IQMBlendPalette (model, anim->pose1, anim->pose2, blend,
									 size, blend_palette, rmesh->palette_size);
		if (!palette) {
			Sys_Error ("R_AliasDrawModel: out of memory");
		}
		finalverts = (finalvert_t *) &palette[2 * rmesh->palette_size];
	} else {
		finalverts = Hunk_TempAlloc (0, size);
		if (!finalverts) {
			Sys_Error ("R_AliasDrawModel: out of memory");
		}
	}
#if 0
	for (size_t i = 0; i < size / 4; i++) {
		((uint32_t*)finalverts)[i] = 0xdeadbeef;
	}
#endif
	// cache align
	pfinalverts = (finalvert_t *)RUP ((intptr_t)finalverts, CACHE_SIZE);
	pauxverts = (auxvert_t *) &pfinalverts[rmesh->numverts + 1];

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		auto fv = pfinalverts + meshes[i].vertices.offset;
		auto av = pauxverts + meshes[i].vertices.offset;
		draw_mesh (ent, lighting, &meshes[i], fv, av, palette);
	}

	if (!renderer->model->model) {
		Cache_Release (&renderer->model->cache);
	}
}
