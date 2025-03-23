/*
	gl_model_iqm.c

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

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/GL/qf_mesh.h"
#include "QF/GL/qf_textures.h"

#include "mod_internal.h"

static byte null_texture[] = {
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
};

static void
gl_iqm_clear (model_t *mod, void *data)
{
	mod->needload = true;
}

static void
gl_iqm_load_textures (mod_iqm_ctx_t *iqm_ctx)
{
	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;
	dstring_t  *str = dstring_new ();

	size_t size = sizeof (tex_t[model->meshes.count])
				+ sizeof (glskin_t[model->meshes.count]);

	auto skintex = (tex_t *) Hunk_AllocName (nullptr, size, iqm_ctx->mod->name);
	auto glskin = (glskin_t *) &skintex[model->meshes.count];
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		auto mesh = &meshes[i];
		dstring_copystr (str, iqm_ctx->text + mesh->material);
		QFS_StripExtension (str->str, str->str);
		GLuint texid;
		tex_t *tex;
		if ((tex = LoadImage (va (0, "textures/%s", str->str), 1))) {
			texid = GL_LoadTexture (str->str, tex->width, tex->height,
									tex->data, true, false,
									tex->format > 2 ? tex->format : 1);
			skintex[i] = *tex;
		} else {
			texid = GL_LoadTexture ("", 2, 2, null_texture, true, false, 4);
			skintex[i] = (tex_t) {
				.width = 2,
				.height = 2,
				.format = tex_rgba,
			};
		}
		auto skinframe = (keyframe_t *) ((byte *) mesh + mesh->skin.keyframes);
		skinframe->data = (byte *) &skintex[i] - (byte *) mesh;
		skintex[i].relative = true;
		skintex[i].external = true;
		skintex[i].data = (byte *) ((byte *) &glskin[i] - (byte *) &skintex[i]);
		glskin[i] = (glskin_t) { .id = texid };
	}
	dstring_delete (str);
}

void
gl_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx)
{
	auto iqm = iqm_ctx->hdr;
	iqm_ctx->mod->clear = gl_iqm_clear;

	uint32_t palette_size;
	auto blend_palette = Mod_IQMBuildBlendPalette (iqm_ctx, &palette_size);

	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;

	size_t size = sizeof (qfm_attrdesc_t[3 * model->meshes.count])
				+ sizeof (keyframedesc_t[model->meshes.count])
				+ sizeof (keyframe_t[model->meshes.count])
				+ sizeof (mesh_vrt_t[iqm->num_vertexes])
				+ sizeof (qfm_blend_t[palette_size]);
	qfm_attrdesc_t *attribs = Hunk_AllocName (0, size, iqm_ctx->mod->name);
	auto skindescs = (keyframedesc_t *) &attribs[3 * model->meshes.count];
	auto skinframes = (keyframe_t *) &skindescs[model->meshes.count];
	auto verts = (mesh_vrt_t *) &skinframes[model->meshes.count];
	auto blend = (qfm_blend_t *) &verts[iqm->num_vertexes];

	memcpy (blend, blend_palette, sizeof (qfm_blend_t) * palette_size);

	attribs[0] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, vertex),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_position,
		.type = qfm_f32,
		.components = 3,
	};
	attribs[1] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, normal),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_normal,
		.type = qfm_f32,
		.components = 3,
	};
	attribs[2] = (qfm_attrdesc_t) {
		.offset = offsetof (mesh_vrt_t, st),
		.stride = sizeof (mesh_vrt_t),
		.attr = qfm_texcoord,
		.type = qfm_f32,
		.components = 2,
	};
	for (uint32_t i = 1; i < model->meshes.count; i++) {
		memcpy (&attribs[3 * i], attribs, sizeof (qfm_attrdesc_t[3]));
	}
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		for (int j = 0; j < 3; j++) {
			attribs[i * 3 + j].offset += (byte *) verts - (byte *) &meshes[i];
		}
		meshes[i].skin = (anim_t) {
			.numdesc = 1,
			.descriptors = (byte *) &skindescs[i] - (byte *) &meshes[i],
			.keyframes = (byte *) &skinframes[i] - (byte *) &meshes[i],
		};
		skindescs[i] = (keyframedesc_t) {
			.firstframe = 0,
			.numframes = 1,
		};
	}

	float *iqm_position = nullptr;
	float *iqm_normal = nullptr;
	float *iqm_texcoord = nullptr;

	for (uint32_t i = 0; i < iqm->num_vertexarrays; i++) {
		auto va = &iqm_ctx->vtxarr[i];
		if (va->type == IQM_POSITION && va->format == IQM_FLOAT
			&& va->size == 3) {
			iqm_position = (float *) ((byte *) iqm + va->offset);
		}
		if (va->type == IQM_TEXCOORD && va->format == IQM_FLOAT
			&& va->size == 2) {
			iqm_texcoord = (float *) ((byte *) iqm + va->offset);
		}
		if (va->type == IQM_NORMAL && va->format == IQM_FLOAT
			&& va->size == 3) {
			iqm_normal = (float *) ((byte *) iqm + va->offset);
		}
	}
	if (!iqm_position || !iqm_texcoord || !iqm_normal) {
		Sys_Error ("unsupported IQM format");
	}

	for (uint32_t i = 0; i < iqm->num_vertexes; i++) {
		verts[i] = (mesh_vrt_t) {
			.st = { iqm_texcoord[0], iqm_texcoord[1] },
			.normal = { VectorExpand (iqm_normal) },
			.vertex = { VectorExpand (iqm_position) },
		};
		iqm_texcoord += 2;
		iqm_normal += 3;
		iqm_position += 3;
	}

	gl_iqm_load_textures (iqm_ctx);
}
