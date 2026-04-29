/*
	gl_mod_mesh.c

	Draw mesh models

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/skin.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_mesh.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_gl.h"

#define s_dynlight (r_refdef.scene->base + scene_dynlight)

typedef struct {
	vec3_t      normal;
	vec3_t      vert;
} blended_vert_t;

typedef struct vert_order_s {
	blended_vert_t *verts;
	int        *order;
	tex_coord_t *tex_coord;
	uint32_t    count;
	uint32_t    base_vert;
	void       *indices;
	void      (*draw_single) (const struct vert_order_s *vo);
	void      (*draw_multi) (const struct vert_order_s *vo);
	void      (*draw_shadow) (transform_t transform, const qf_mesh_t *mesh,
							  const struct vert_order_s *vo);
	void      (*bv_blend) (blended_vert_t *bv, float blend,
						   const void *a1, const void *a2);
	void      (*bv_bones) (blended_vert_t *bv, float blend,
						   const void *a1, const void *a2,
						   const uint32_t *joint, mat4f_t *palette);
	mat4f_t    *palette;
} vert_order_t;

static vec3_t		shadevector;

static void
gl_draw_single_index8 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint8_t    *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_single_index16 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint16_t   *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_single_index32 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint32_t   *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_single_order (const vert_order_t *vo)
{
	int         count;
	int        *order = vo->order;
	blended_vert_t *verts = vo->verts;

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else {
			qfglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			qfglTexCoord2fv ((float *) order);
			order += 2;

			// normals and vertices come from the frame list
			qfglNormal3fv (verts->normal);
			qfglVertex3fv (verts->vert);
			verts++;
		} while (--count);

		qfglEnd ();
	}
}

static void
gl_draw_multi_index8 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint8_t    *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_multi_index16 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint16_t   *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_multi_index32 (const vert_order_t *vo)
{
	int         count = vo->count;
	uint32_t   *index = vo->indices;

	qfglBegin (GL_TRIANGLES);
	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;
		auto tc = vo->tex_coord + ind;

		qglMultiTexCoord2fv (gl_mtex_enum + 0, tc->st);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, tc->st);

		qfglNormal3fv (bv->normal);
		qfglVertex3fv (bv->vert);
	} while (--count);
	qfglEnd ();
}

static void
gl_draw_multi_order (const vert_order_t *vo)
{
	int         count;
	int        *order = vo->order;
	blended_vert_t *verts = vo->verts;

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else {
			qfglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			qglMultiTexCoord2fv (gl_mtex_enum + 0, (float *) order);
			qglMultiTexCoord2fv (gl_mtex_enum + 1, (float *) order);
			order += 2;

			// normals and vertices come from the frame list
			qfglNormal3fv (verts->normal);
			qfglVertex3fv (verts->vert);
			verts++;
		} while (--count);

		qfglEnd ();
	}
}

static void
gl_draw_shadow_index8 (transform_t transform, const qf_mesh_t *mesh,
					   const vert_order_t *vo)
{
	int         count = vo->count;
	uint8_t    *index = vo->indices;

	vec4f_t     entorigin = Transform_GetWorldPosition (transform);
	float       lheight = entorigin[2] - lightspot[2];
	float       height = -lheight + 1.0;

	const vec_t *scale = mesh->scale;
	vec3_t      scale_origin = { VectorExpand (mesh->scale_origin) };
	scale_origin[2] += lheight;

	qfglBegin (GL_TRIANGLES);

	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;

		vec3_t point;
		VectorCompMultAdd (scale_origin, bv->vert, scale, point);

		point[0] -= shadevector[0] * point[2];
		point[1] -= shadevector[1] * point[2];
		point[2] = height;
		qfglVertex3fv (point);
	} while (--count);

	qfglEnd ();
}

static void
gl_draw_shadow_index16 (transform_t transform, const qf_mesh_t *mesh,
					   const vert_order_t *vo)
{
	int         count = vo->count;
	uint16_t   *index = vo->indices;

	vec4f_t     entorigin = Transform_GetWorldPosition (transform);
	float       lheight = entorigin[2] - lightspot[2];
	float       height = -lheight + 1.0;

	const vec_t *scale = mesh->scale;
	vec3_t      scale_origin = { VectorExpand (mesh->scale_origin) };
	scale_origin[2] += lheight;

	qfglBegin (GL_TRIANGLES);

	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;

		vec3_t point;
		VectorCompMultAdd (scale_origin, bv->vert, scale, point);

		point[0] -= shadevector[0] * point[2];
		point[1] -= shadevector[1] * point[2];
		point[2] = height;
		qfglVertex3fv (point);
	} while (--count);

	qfglEnd ();
}

static void
gl_draw_shadow_index32 (transform_t transform, const qf_mesh_t *mesh,
					   const vert_order_t *vo)
{
	int         count = vo->count;
	uint32_t   *index = vo->indices;

	vec4f_t     entorigin = Transform_GetWorldPosition (transform);
	float       lheight = entorigin[2] - lightspot[2];
	float       height = -lheight + 1.0;

	const vec_t *scale = mesh->scale;
	vec3_t      scale_origin = { VectorExpand (mesh->scale_origin) };
	scale_origin[2] += lheight;

	qfglBegin (GL_TRIANGLES);

	do {
		uint32_t ind = *index++ - vo->base_vert;
		auto bv = vo->verts + ind;

		vec3_t point;
		VectorCompMultAdd (scale_origin, bv->vert, scale, point);

		point[0] -= shadevector[0] * point[2];
		point[1] -= shadevector[1] * point[2];
		point[2] = height;
		qfglVertex3fv (point);
	} while (--count);

	qfglEnd ();
}

/*
	GL_DrawAliasShadow

	Standard shadow drawing
*/
static void
gl_draw_shadow_order (transform_t transform, const qf_mesh_t *mesh,
				      const vert_order_t *vo)
{
	const int  *order = vo->order;
	vec4f_t     entorigin = Transform_GetWorldPosition (transform);
	float       lheight = entorigin[2] - lightspot[2];
	float       height = -lheight + 1.0;

	const vec_t *scale = mesh->scale;
	vec3_t      scale_origin = { VectorExpand (mesh->scale_origin) };
	scale_origin[2] += lheight;

	auto bv = vo->verts;
	int         count;
	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else {
			qfglBegin (GL_TRIANGLE_STRIP);
		}

		order += 2 * count;		// skip texture coords
		do {
			vec3_t point;
			VectorCompMultAdd (scale_origin, bv->vert, scale, point);

			point[0] -= shadevector[0] * point[2];
			point[1] -= shadevector[1] * point[2];
			point[2] = height;
			qfglVertex3fv (point);
			bv++;
		} while (--count);

		qfglEnd ();
	}
}

static inline void
bv_v24_n8 (blended_vert_t *bv, float blend, const void *a1, const void *a2)
{
	const trivertx_t *verts1 = a1;
	const trivertx_t *verts2 = a2;

	VectorBlend (verts1->v, verts2->v, blend, bv->vert);
	float *n1 = r_avertexnormals[verts1->lightnormalindex];
	float *n2 = r_avertexnormals[verts1->lightnormalindex];
	VectorBlend (n1, n2, blend, bv->normal);
	if (VectorIsZero (bv->normal)) {
		if (blend < 0.5) {
			VectorCopy (n1, bv->normal);
		} else {
			VectorCopy (n2, bv->normal);
		}
	}
}

static inline void
bv_v24_n8_b (blended_vert_t *bv, float blend, const void *a1, const void *a2,
			 const uint32_t *joint, mat4f_t *palette)
{
	bv_v24_n8 (bv, blend, a1, a2);

	auto m = &palette[*joint * 2];
	auto v = loadvec3f (bv->vert) + (vec4f_t) { 0, 0, 0, 1 };
	storevec3f (bv->vert, mvmulf (m[0], v));
	auto n = loadvec3f (bv->normal);
	storevec3f (bv->normal, mvmulf (m[1], n));
}

static inline void
bv_v48_n16 (blended_vert_t *bv, float blend, const void *a1, const void *a2)
{
	const trivertx16_t *verts1 = a1;
	const trivertx16_t *verts2 = a2;

	VectorBlend (verts1->v, verts2->v, blend, bv->vert);
	float *n1 = r_avertexnormals[verts1->lightnormalindex];
	float *n2 = r_avertexnormals[verts1->lightnormalindex];
	VectorBlend (n1, n2, blend, bv->normal);
	if (VectorIsZero (bv->normal)) {
		if (blend < 0.5) {
			VectorCopy (n1, bv->normal);
		} else {
			VectorCopy (n2, bv->normal);
		}
	}
}

static inline void
bv_v48_n16_b (blended_vert_t *bv, float blend, const void *a1, const void *a2,
			  const uint32_t *joint, mat4f_t *palette)
{
	bv_v48_n16 (bv, blend, a1, a2);

	auto m = &palette[*joint * 2];
	auto v = loadvec3f (bv->vert) + (vec4f_t) { 0, 0, 0, 1 };
	storevec3f (bv->vert, mvmulf (m[0], v));
	auto n = loadvec3f (bv->normal);
	storevec3f (bv->normal, mvmulf (m[1], n));
}

static inline void
bv_v96_n96 (blended_vert_t *bv, float blend, const void *a1, const void *a2)
{
	const float *position = a1;
	const float *normal = a2;

	VectorCopy (position, bv->vert);
	VectorCopy (normal, bv->normal);
}

static inline void
bv_v96_n96_b (blended_vert_t *bv, float blend, const void *a1, const void *a2,
			  const uint32_t *joint, mat4f_t *palette)
{
	const float *position = a1;
	const float *normal = a2;

	auto m = &palette[*joint * 2];
	auto v = loadvec3f (position) + (vec4f_t) { 0, 0, 0, 1 };
	storevec3f (bv->vert, mvmulf (m[0], v));
	auto n = loadvec3f (normal);
	storevec3f (bv->normal, mvmulf (m[1], n) * m[3][0]);
}

static size_t
size_v24_n8 (uint32_t count)
{
	size_t size = sizeof (vert_order_t)
				+ sizeof (blended_vert_t[count]);
	return size;
}

static size_t
size_v48_n16 (uint32_t count)
{
	size_t size = sizeof (vert_order_t)
				+ sizeof (blended_vert_t[count]);
	return size;
}

static size_t
size_v96_n96 (uint32_t count)
{
	size_t size = sizeof (vert_order_t)
				+ sizeof (tex_coord_t[count])
				+ sizeof (blended_vert_t[count]);
	return size;
}

static glskin_t
gl_get_skin (entity_t e, renderer_t *renderer, qf_mesh_t *mesh)
{
	auto colormap = Entity_GetColormap (e);
	if (!gl_nocolors) {
		skin_t *skin = renderer->skin ? Skin_Get (renderer->skin) : nullptr;
		if (skin) {
			return gl_Skin_Get (skin->tex, colormap, (byte *) mesh);
		}
	}

	tex_t *tex = nullptr;
	if (renderer->skindesc) {
		tex = (tex_t *) ((byte *) mesh + renderer->skindesc);
	} else {
		auto skinframe = (keyframe_t *) ((byte *) mesh + mesh->skin.keyframes);
		tex = (tex_t *) ((byte *) mesh + skinframe->data);
	}
	return gl_Skin_Get (tex, colormap, (byte *) mesh);
}

static inline void *
attr_ptr (qf_mesh_t *mesh, qfm_attrdesc_t *attr, uint32_t index)
{
	return (byte *) mesh + attr->offset + index * attr->stride;
}

static void
gl_alias_draw_mesh (qf_model_t *model, qf_mesh_t *mesh, vert_order_t *vo,
					uint32_t base_vert,
					entity_t ent, renderer_t *renderer, transform_t transform,
					float *color, float *dark)
{
	vec3_t      scale;
	bool is_fullbright = (renderer->model->fullbright || renderer->fullbright);

	gl_ctx->alias_polys += mesh->triangle_count;

	// if the model has a colorised/external skin, use it, otherwise use
	// the skin embedded in the model data
	auto glskin = gl_get_skin (ent, renderer, mesh);
	if (!gl_fb_models || is_fullbright) {
		glskin.fb = 0;
	}

	qfm_attrdesc_t *position = nullptr;
	qfm_attrdesc_t *normal = nullptr;
	qfm_attrdesc_t *texcoord = nullptr;
	qfm_attrdesc_t *joints = nullptr;
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	for (uint32_t i = 0; i < mesh->attributes.count; i++) {
		if (attr[i].attr == qfm_position) {
			position = &attr[i];
		} else if (attr[i].attr == qfm_normal) {
			normal = &attr[i];
		} else if (attr[i].attr == qfm_texcoord) {
			texcoord = &attr[i];
		} else if (attr[i].attr == qfm_joints) {
			joints = &attr[i];
		}
	}
	if (!position || !normal || !texcoord) {
		Sys_Error ("missing vertex attribute");
	}
	if (texcoord->type != qfm_special && texcoord->type != qfm_f32) {
		Sys_Error ("unsupported texcoord format");
	}

	auto animation = Entity_GetAnimation (ent);
	float blend = animation->blend;
	qfm_attrdesc_t attrs[4] = {};
	if (position->type == qfm_u16) {
		// because we multipled by 256 when we loaded the verts, we have to
		// scale by 1/256 when drawing.
		// FIXME this should be done in the loader
		VectorScale (mesh->scale, 1 / 256.0, scale);
		attrs[0] = *position;
		attrs[1] = *position;
		attrs[0].offset = animation->pose1;
		attrs[1].offset = animation->pose2;
	} else if (position->type == qfm_u8) {
		VectorScale (mesh->scale, 1, scale);
		attrs[0] = *position;
		attrs[1] = *position;
		attrs[0].offset = animation->pose1;
		attrs[1].offset = animation->pose2;
	} else if (position->type == qfm_f32) {
		VectorScale (mesh->scale, 1, scale);
		attrs[0] = *position;
		attrs[1] = *normal;
		attrs[2] = *texcoord;
	} else {
		// the type has already been checked, this just prevents an
		// uninitialized variable warning
		return;
	}
	if (joints) {
		attrs[3] = *joints;
	}

	if (mesh->index_type == qfm_u32) {
		vo->draw_single = gl_draw_single_index32;
		vo->draw_multi = gl_draw_multi_index32;
		vo->draw_shadow = gl_draw_shadow_index32;
	} else if (mesh->index_type == qfm_u16) {
		vo->draw_single = gl_draw_single_index16;
		vo->draw_multi = gl_draw_multi_index16;
		vo->draw_shadow = gl_draw_shadow_index16;
	} else if (mesh->index_type == qfm_u8) {
		vo->draw_single = gl_draw_single_index8;
		vo->draw_multi = gl_draw_multi_index8;
		vo->draw_shadow = gl_draw_shadow_index8;
	} else if (mesh->index_type == qfm_special) {
		vo->draw_single = gl_draw_single_order;
		vo->draw_multi = gl_draw_multi_order;
		vo->draw_shadow = gl_draw_shadow_order;
		vo->count = 0;
	} else {
		Sys_Error ("unsupported index type");
	}

	vo->count = mesh->triangle_count * 3;
	vo->indices = (byte *) mesh + mesh->indices;
	vo->base_vert = base_vert;

#define VERT_LOOP(bv_func, do_tc, ...) \
	do { \
		blended_vert_t *bv = vo->verts; \
		tex_coord_t *tc = vo->tex_coord; \
		for (uint32_t i = 0; i < mesh->vertices.count; i++, bv++, tc++) { \
			uint32_t vert_ind = base_vert + i; \
			bv_func (bv, blend, \
					 attr_ptr (mesh, &attrs[0], vert_ind), \
					 attr_ptr (mesh, &attrs[1], vert_ind) \
					 __VA_OPT__(,) __VA_ARGS__); \
			if (do_tc) { \
				*tc = *(tex_coord_t *) attr_ptr (mesh, &attrs[2], vert_ind); \
			}; \
		} \
	} while (0)

	bool do_tex = vo->bv_bones == bv_v96_n96_b;
	if (vo->palette && joints) {
		VERT_LOOP (vo->bv_bones, do_tex, attr_ptr (mesh, &attrs[3], vert_ind),
				   vo->palette);
	} else {
		VERT_LOOP (vo->bv_blend, do_tex);
	}

	// setup the transform
	qfglPushMatrix ();
	gl_R_RotateForEntity (Transform_GetWorldMatrixPtr (transform));

	qfglTranslatef (mesh->scale_origin[0],
					mesh->scale_origin[1],
					mesh->scale_origin[2]);
	qfglScalef (scale[0], scale[1], scale[2]);

	if (gl_modelalpha < 1.0)
		qfglDepthMask (GL_FALSE);

	// draw all the triangles
	if (is_fullbright) {
		qfglBindTexture (GL_TEXTURE_2D, glskin.id);

		if (gl_vector_light) {
			qfglDisable (GL_LIGHTING);
			if (!gl_tess)
				qfglDisable (GL_NORMALIZE);
		}

		vo->draw_single (vo);

		if (gl_vector_light) {
			if (!gl_tess)
				qfglEnable (GL_NORMALIZE);
			qfglEnable (GL_LIGHTING);
		}
	} else if (!glskin.fb) {
		// Model has no fullbrights, don't bother with multi
		qfglBindTexture (GL_TEXTURE_2D, glskin.id);
		vo->draw_single (vo);
	} else {	// try multitexture
		if (gl_mtex_active_tmus >= 2) {	// set up the textures
			qglActiveTexture (gl_mtex_enum + 0);
			qfglBindTexture (GL_TEXTURE_2D, glskin.id);

			qglActiveTexture (gl_mtex_enum + 1);
			qfglEnable (GL_TEXTURE_2D);
			qfglBindTexture (GL_TEXTURE_2D, glskin.fb);

			// do the heavy lifting
			vo->draw_multi (vo);

			// restore the settings
			qfglDisable (GL_TEXTURE_2D);
			qglActiveTexture (gl_mtex_enum + 0);
		} else {
			qfglBindTexture (GL_TEXTURE_2D, glskin.id);
			vo->draw_single (vo);

			if (gl_vector_light) {
				qfglDisable (GL_LIGHTING);
				if (!gl_tess)
					qfglDisable (GL_NORMALIZE);
			}

			qfglColor4fv (renderer->colormod);

			qfglBindTexture (GL_TEXTURE_2D, glskin.fb);
			vo->draw_single (vo);

			if (gl_vector_light) {
				qfglEnable (GL_LIGHTING);
				if (!gl_tess)
					qfglEnable (GL_NORMALIZE);
			}
		}
	}

	qfglPopMatrix ();

	// torches, grenades, and lightning bolts do not have shadows
	if (r_shadows && renderer->model->shadow_alpha) {
		mat4f_t     shadow_mat;

		//FIXME There's something weird with using Transform_GetWorldMatrix,
		//Transform_GetWorldMatrixPtr, and maybe mat4ftranspose and m3vmulf:
		//gcc-15 complaines about shadow_mat being uninitialized but gcc-14
		//is silent. Moving the shadevector code into its own function makes
		//both versions of gcc complain. code marked with XXX are the relevant
		//lines for checking (this version gets past both versions of gcc).
		Transform_GetWorldMatrix (transform, shadow_mat);//XXX
		qfglPushMatrix ();
		gl_R_RotateForEntity (shadow_mat);//XXX
		//XXX gl_R_RotateForEntity (Transform_GetWorldMatrixPtr (transform));

		//FIXME fully vectorize
		vec4f_t     vec = { 0.707106781, 0, 0.707106781, 0 };
		//XXX Transform_GetWorldMatrix (transform, shadow_mat);
		mat4ftranspose (shadow_mat, shadow_mat);
		vec = m3vmulf (shadow_mat, vec);
		VectorCopy (vec, shadevector);

		if (!gl_tess)
			qfglDisable (GL_NORMALIZE);
		qfglDisable (GL_LIGHTING);
		qfglDisable (GL_TEXTURE_2D);
		qfglDepthMask (GL_FALSE);

		if (gl_modelalpha < 1.0) {
			VectorBlend (renderer->colormod, dark, 0.5, color);
			color[3] = gl_modelalpha * (renderer->model->shadow_alpha / 255.0);
			qfglColor4fv (color);
		} else {
			color_black[3] = renderer->model->shadow_alpha;
			qfglColor4ubv (color_black);
		}
		vo->draw_shadow (transform, mesh, vo);

		qfglDepthMask (GL_TRUE);
		qfglEnable (GL_TEXTURE_2D);
		qfglEnable (GL_LIGHTING);
		if (!gl_tess)
			qfglEnable (GL_NORMALIZE);
		qfglPopMatrix ();
	} else if (gl_modelalpha < 1.0) {
		qfglDepthMask (GL_TRUE);
	}
}

static void
get_fn_ptrs (qf_mesh_t *mesh, size_t (**get_size) (uint32_t count),
			 void (**bv_blend) (blended_vert_t *bv, float blend,
								const void *a1, const void *a2),
			 void (**bv_bones) (blended_vert_t *bv, float blend,
								const void *a1, const void *a2,
								const uint32_t *joint, mat4f_t *palette))
{
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	if (attr[0].attr != qfm_position
		|| attr[1].attr != qfm_normal
		|| attr[2].attr != qfm_texcoord) {
		Sys_Error ("unsupported layout");
	}
	auto position = &attr[0];
	auto normal = &attr[1];

	if (position->type == qfm_u16 && position->components == 3
		&& normal->type == qfm_u16 && normal->components == 1) {
		*get_size = size_v48_n16;
		*bv_blend = bv_v48_n16;
		*bv_bones = bv_v48_n16_b;
	} else if (position->type == qfm_u8 && position->components == 3
			   && normal->type == qfm_u8 && normal->components == 1) {
		*get_size = size_v24_n8;
		*bv_blend = bv_v24_n8;
		*bv_bones = bv_v24_n8_b;
	} else if (position->type == qfm_f32 && position->components == 3
			   && normal->type == qfm_f32 && normal->components == 3) {
		if ((mesh->index_type != qfm_u32
			 && mesh->index_type != qfm_u16
			 && mesh->index_type != qfm_u8) || !mesh->indices) {
			Sys_Error ("unsupported index format");
		}
		*get_size = size_v96_n96;
		*bv_blend = bv_v96_n96;
		*bv_bones = bv_v96_n96_b;
	} else {
		Sys_Error ("unsupported vertex format");
	}
}

void
gl_R_DrawAliasModel (entity_t e)
{
	float       radius, minlight, d;
	float       position[4] = {0.0, 0.0, 0.0, 1.0},
				color[4] = {0.0, 0.0, 0.0, 1.0},
				dark[4] = {0.0, 0.0, 0.0, 1.0},
				emission[4] = {0.0, 0.0, 0.0, 1.0};
	int         gl_light;
	int         used_lights = 0;
	bool        is_fullbright = false;
	vec3_t      dist, scale;
	vec4f_t     origin;
	auto renderer = Entity_GetRenderer (e);

	if (renderer->onlyshadows) {
		return;
	}

	radius = renderer->model->radius;
	transform_t transform = Entity_Transform (e);
	origin = Transform_GetWorldPosition (transform);
	VectorCopy (Transform_GetWorldScale (transform), scale);
	//FIXME assumes uniform scale
	if (scale[0] != 1.0) {
		radius *= scale[0];
	}
	if (radius && //FIXME calculate radius for iqm
		R_CullSphere (r_refdef.frustum, (vec_t*)&origin, radius)) {//FIXME
		return;
	}

	gl_modelalpha = renderer->colormod[3];

	is_fullbright = (renderer->model->fullbright || renderer->fullbright);
	minlight = max (renderer->model->min_light, renderer->min_light);

	qfglColor4fv (renderer->colormod);

	if (!is_fullbright) {
		float lightadj;

		// get lighting information
		R_LightPoint (&r_refdef.worldmodel->brush, origin);//FIXME

		lightadj = (ambientcolor[0] + ambientcolor[1] + ambientcolor[2]) / 765.0;

		// Do minlight stuff here since that's how software does it :)

		if (lightadj > 0) {
			if (lightadj < minlight)
				lightadj = minlight / lightadj;
			else
				lightadj = 1.0;

			// 255 is fullbright
			VectorScale (ambientcolor, lightadj / 255.0, ambientcolor);
		} else {
			ambientcolor[0] = ambientcolor[1] = ambientcolor[2] = minlight;
		}

		if (gl_vector_light) {
			auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
			auto dlight_data = (dlight_t *) dlight_pool->data;
			for (uint32_t i = 0; i < dlight_pool->count; i++) {
				auto l = &dlight_data[i];
				VectorSubtract (l->origin, origin, dist);
				if ((d = DotProduct (dist, dist)) >	// Out of range
					((l->radius + radius) * (l->radius + radius))) {
					continue;
				}


				if (used_lights >= gl_max_lights) {
					// For solid lighting, multiply by 0.5 since it's cos
					// 60 and 60 is a good guesstimate at the average
					// incident angle. Seems to match vector lighting
					// best, too.
					VectorMultAdd (emission,
						0.5 / ((d * 0.01 / l->radius) + 0.5),
						l->color, emission);
					continue;
				}

				VectorCopy (l->origin, position);

				VectorCopy (l->color, color);
				color[3] = 1.0;

				gl_light = GL_LIGHT0 + used_lights;
				qfglEnable (gl_light);
				qfglLightfv (gl_light, GL_POSITION, position);
				qfglLightfv (gl_light, GL_AMBIENT, color);
				qfglLightfv (gl_light, GL_DIFFUSE, color);
				qfglLightfv (gl_light, GL_SPECULAR, color);
				// 0.01 is used here because it just seemed to match
				// the bmodel lighting best. it's over r instead of r*r
				// so that larger-radiused lights will be brighter
				qfglLightf (gl_light, GL_QUADRATIC_ATTENUATION,
							0.01 / (l->radius));
				used_lights++;
			}

			VectorAdd (ambientcolor, emission, emission);
			d = max (emission[0], max (emission[1], emission[2]));
			// 1.5 to allow some pastelization (curb darkness from dlight)
			if (d > 1.5) {
				VectorScale (emission, 1.5 / d, emission);
			}

			qfglMaterialfv (GL_FRONT, GL_EMISSION, emission);
		} else {
			VectorCopy (ambientcolor, emission);

			auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
			auto dlight_data = (dlight_t *) dlight_pool->data;
			for (uint32_t i = 0; i < dlight_pool->count; i++) {
				auto l = &dlight_data[i];
				VectorSubtract (l->origin, origin, dist);

				if ((d = DotProduct (dist, dist)) > (l->radius + radius) *
					(l->radius + radius)) {
						continue;
				}

				// For solid lighting, multiply by 0.5 since it's cos 60
				// and 60 is a good guesstimate at the average incident
				// angle. Seems to match vector lighting best, too.
				VectorMultAdd (emission,
					(0.5 / ((d * 0.01 / l->radius) + 0.5)),
					l->color, emission);
			}

			d = max (emission[0], max (emission[1], emission[2]));
			// 1.5 to allow some fading (curb emission making stuff dark)
			if (d > 1.5) {
				VectorScale (emission, 1.5 / d, emission);
			}

			emission[0] *= renderer->colormod[0];
			emission[1] *= renderer->colormod[1];
			emission[2] *= renderer->colormod[2];
			emission[3] *= renderer->colormod[3];

			qfglColor4fv (emission);
		}
	}

	// locate the proper data
	auto model = renderer->model->model;
	if (!model) {
		model = Cache_Get (&renderer->model->cache);
	}
	auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	auto rmesh = (gl_mesh_t *) ((byte *) model + model->render_data);

	auto get_size = size_v24_n8;
	auto bv_blend = bv_v24_n8;
	auto bv_bones = bv_v24_n8_b;
	get_fn_ptrs (&meshes[0], &get_size, &bv_blend, &bv_bones);

	size_t size = get_size (rmesh->numverts);
	vert_order_t *vo = nullptr;
	mat4f_t *palette = nullptr;
	uint32_t c_matrix_palette = e.base + scene_matrix_palette;
	if (rmesh->palette_size
		&& Ent_HasComponent (e.id, c_matrix_palette, e.reg)) {
		auto mp = *(qfm_motor_t **) Ent_GetComponent (e.id, c_matrix_palette,
													  e.reg);
		auto blend_palette = (qfm_blend_t *) ((byte *) model
											  + rmesh->blend_palette);
		palette = Mod_BlendPalette (blend_palette, rmesh->palette_size,
									mp, model->joints.count, size);
		if (!palette) {
			Sys_Error ("gl_alias_draw_mesh: out of memory");
		}
		vo = (vert_order_t *) &palette[2 * rmesh->palette_size];
	} else {
		vo = Hunk_TempAlloc (0, size);
		if (!vo) {
			Sys_Error ("gl_alias_draw_mesh: out of memory");
		}
	}

	auto texc = (tex_coord_t *) &vo[1];
	auto verts = (blended_vert_t *) &texc[rmesh->numverts];
	int *order = nullptr;
	if (bv_bones != bv_v96_n96_b) {
		auto mesh = &meshes[0];
		auto attr = (qfm_attrdesc_t *)((byte *) mesh + mesh->attributes.offset);
		auto texcoord = &attr[2];
		if (texcoord->type != qfm_special) {
			texc = (tex_coord_t *) ((byte *) mesh + texcoord->offset);
		} else {
			order = (int *) ((byte *) mesh + texcoord->offset);
			texc = nullptr;
		}
		verts = (blended_vert_t *) &vo[1];
	}
	*vo = (vert_order_t) {
		.verts = verts,
		.tex_coord = texc,
		.order = order,
		.bv_blend = bv_blend,
		.bv_bones = bv_bones,
		.palette = palette,
	};

	uint32_t base_vert = 0;
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		if (!(renderer->submesh_mask & (1 << i))) {
			gl_alias_draw_mesh (model, &meshes[i], vo, base_vert,
								e, renderer, transform, color, dark);
		}
		base_vert += meshes[i].vertices.count;
	}

	while (used_lights--) {
		qfglDisable (GL_LIGHT0 + used_lights);
	}

	if (!renderer->model->model) {
		Cache_Release (&renderer->model->cache);
	}
}
