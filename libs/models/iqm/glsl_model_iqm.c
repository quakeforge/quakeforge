/*
	glsl_model_iqm.c

	iqm model processing for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/04/27

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

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_mesh.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "r_shared.h"

static byte null_texture[] = {
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
};
#if 0
static byte null_normmap[] = {
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
};
#endif
static void
glsl_iqm_clear (model_t *mod, void *data)
{
	auto model = mod->model;
	auto rmesh = (glsl_mesh_t *) ((byte *) model + model->render_data);
	GLuint      bufs[2];

	mod->needload = true;

	bufs[0] = rmesh->vertices;
	bufs[1] = rmesh->indices;
	qfeglDeleteBuffers (2, bufs);

	auto textures = (GLuint *) (&rmesh[1]);
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		GLSL_ReleaseTexture (textures[i]);
	}
}

static void
glsl_iqm_load_textures (qf_model_t *model)
{
	dstring_t  *str = dstring_new ();
	tex_t      *tex;

	auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	auto text = (const char *) model + model->text.offset;
	auto skinframe = (keyframe_t *) ((byte *) meshes + meshes->skin.keyframes);
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		dstring_copystr (str, text + meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		uint32_t texid;
		if ((tex = LoadImage (va (0, "textures/%s", str->str), 1))) {
			texid = GLSL_LoadRGBATexture (str->str, tex->width, tex->height,
										  tex->data);
		} else {
			texid = GLSL_LoadRGBATexture ("", 2, 2, null_texture);
		}
		skinframe[i].data = texid;
#if 0
		if ((tex = LoadImage (va (0, "textures/%s_norm", str->str), 1)))
			normmaps[i] = GLSL_LoadRGBATexture (str->str, tex->width,
												tex->height, tex->data);
		else
			normmaps[i] = GLSL_LoadRGBATexture ("", 2, 2, null_normmap);
#endif
	}
	dstring_delete (str);
}

static void
glsl_iqm_load_arrays (qf_model_t *model, byte *vertices, uint32_t vertex_size,
					  void *indices, uint32_t index_size)
{
	auto rmesh = (glsl_mesh_t *) ((byte *) model + model->render_data);
	GLuint      bufs[2];

	qfeglGenBuffers (2, bufs);
	rmesh->vertices = bufs[0];
	rmesh->indices = bufs[1];
	qfeglBindBuffer (GL_ARRAY_BUFFER, rmesh->vertices);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, rmesh->indices);
	qfeglBufferData (GL_ARRAY_BUFFER, vertex_size, vertices, GL_STATIC_DRAW);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER, index_size, indices,
					 GL_STATIC_DRAW);
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
glsl_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx)
{
	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;
	iqm_ctx->mod->clear = glsl_iqm_clear;
	uint16_t num_tex = model->meshes.count * 2;
	auto iqm = iqm_ctx->hdr;

	size_t size = sizeof (glsl_mesh_t)
				+ sizeof (qfm_attrdesc_t[iqm->num_vertexarrays])
				+ sizeof (keyframedesc_t[model->meshes.count])
				+ sizeof (keyframe_t[model->meshes.count]);
	const char *name = iqm_ctx->mod->name;
	glsl_mesh_t *rmesh = Hunk_AllocName (nullptr, size, name);
	auto attribs = (qfm_attrdesc_t *) &((GLuint *) &rmesh[1])[num_tex];
	auto skindescs = (keyframedesc_t *) &attribs[iqm->num_vertexarrays];
	auto skinframes = (keyframe_t *) &skindescs[model->meshes.count];

	model->render_data = (byte *) rmesh - (byte *) model;
	*rmesh = (glsl_mesh_t) {
		.skinwidth = 1,
		.skinheight = 1,
	};

	uint32_t index_count = iqm->num_triangles * 3;
	auto index_type = mesh_index_type (iqm->num_vertexes);
	uint32_t index_size = mesh_type_size (index_type);
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		meshes[i].triangle_count = iqm_ctx->meshes[i].num_triangles;
		meshes[i].index_type = index_type;
		meshes[i].indices = iqm_ctx->meshes[i].first_triangle * index_size * 3;
		meshes[i].attributes = (qfm_loc_t) {
			.offset = (byte *) attribs - (byte *) &meshes[i],
			.count = 4,
		};
		meshes[i].skin = (anim_t) {
			.numdesc = 1,
			.descriptors = (byte *) skindescs - (byte *) &meshes[i],
			.keyframes = (byte *) skinframes - (byte *) &meshes[i],
		};
		skindescs[i] = (keyframedesc_t) {
			.numframes = 1,
		};
	}
	uint32_t max_offs =  0u;
	uint32_t min_offs = ~0u;
	uint32_t num_verts = iqm->num_vertexes;
	for (uint32_t i = 0; i < iqm->num_vertexarrays; i++) {
		auto a = iqm_ctx->vtxarr[i];
		attribs[i] = iqm_mesh_attribute (a, a.offset);
		if (attribs[i].offset < min_offs) {
			min_offs = attribs[i].offset;
		}
		if (attribs[i].offset + num_verts * attribs[i].stride > max_offs) {
			max_offs = attribs[i].offset + num_verts * attribs[i].stride;
		}
	}
	for (uint32_t i = 0; i < iqm->num_vertexarrays; i++) {
		attribs[i].offset -= min_offs;
	}
	auto vertices = (byte *) iqm + min_offs;
	uint32_t vertex_size = max_offs - min_offs;
	auto indices = (uint32_t *) ((byte *) iqm + iqm->ofs_triangles);
	auto index_bytes = pack_indices (indices, index_count, index_type);
	glsl_iqm_load_arrays (model, vertices, vertex_size, indices, index_bytes);

	glsl_iqm_load_textures (model);
}
