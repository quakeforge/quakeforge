/*
	sw_model_iqm.c

	iqm model processing for SW

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

#include "qfalloca.h"

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "d_iface.h"
#include "mod_internal.h"
#include "r_internal.h"

static byte null_data[] = {15, 15, 15, 15};
static tex_t null_texture = {
	.width = 2,
	.height = 2,
	.format = tex_palette,
	.loaded = true,
	.palette =0,
	.data = null_data,
};
constexpr size_t null_texture_size = sizeof (null_texture) + sizeof (null_data);

static void
sw_iqm_clear (model_t *mod, void *data)
{
	mod->needload = true;
}

static inline void
convert_coord (int32_t *tc, int size)
{
	*(int32_t *) tc = (int32_t) (*(float *) tc * (size - 1)) << 16;
}

static inline uint32_t
get_vertex_index (qf_mesh_t *mesh, uint32_t index)
{
	auto indices = (byte *) mesh + mesh->indices;
	switch (mesh->index_type) {
		case qfm_s8:
		case qfm_u8:
			return ((uint8_t *) indices)[index];
		case qfm_s16:
		case qfm_u16:
			return ((uint16_t *) indices)[index];
		case qfm_s32:
		case qfm_u32:
			return ((uint32_t *) indices)[index];
		case qfm_special:
			return ((dtriangle_t *) indices)[index / 3].vertindex[index % 3];
		default:
			Sys_Error ("unsupported index type");
	}
}

static inline int32_t *
get_texcoord (qf_mesh_t *mesh, uint32_t index)
{
	auto attr = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t tc_offset = index * attr[2].stride;
	return (int32_t *) ((byte *) mesh + attr[2].offset + tc_offset);
}

static void
sw_iqm_load_textures (mod_iqm_ctx_t *iqm_ctx)
{
	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;
	auto text = (const char *) ((byte *) model + model->text.offset);
	uint32_t num_verts = 0;
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		num_verts += meshes[i].vertices.count;
	}
	dstring_t  *str = dstring_new ();
	set_t       done_verts = SET_STATIC_INIT (num_verts, alloca);
	set_empty (&done_verts);

	for (uint32_t i = 0; i < model->meshes.count; i++) {
		uint32_t j;
		for (j = 0; j < i; j++) {
			if (meshes[j].material == meshes[i].material) {
				meshes[i].skin = meshes[j].skin;
				break;
			}
		}
		if (j < i) {
			continue;
		}
		dstring_copystr (str, text + meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		auto tex = LoadImage (va (0, "textures/%s", str->str), 1);
		if (tex) {
			tex = ConvertImage (tex, vid.basepal, str->str);
		} else {
			tex = Hunk_AllocName (nullptr, null_texture_size, "null texture");
			*tex = null_texture;
			memcpy (tex + 1, null_data, sizeof (null_data));
			tex->data = (byte *) &tex[1];
		}
		meshes[i].skin = (anim_t) {
			.data = (byte *) tex - (byte *) &meshes[i],
		};
		for (j = 0; j < meshes[i].triangle_count * 3; j++) {
			uint32_t    vind = get_vertex_index (&meshes[i], j);
			int32_t    *tc = get_texcoord (&meshes[i], vind);

			if (set_is_member (&done_verts, vind)) {
				continue;
			}
			set_add (&done_verts, vind);

			convert_coord (tc + 1, tex->width);
			convert_coord (tc + 2, tex->height);
		}
	}
	dstring_delete (str);
}

static void
sw_iqm_convert_tris (mod_iqm_ctx_t *iqm_ctx, dtriangle_t *tris)
{
	auto iqmtris = iqm_ctx->triangles;
	uint32_t num_tris = iqm_ctx->hdr->num_triangles;
	for (uint32_t i = 0; i < num_tris; i++) {
		tris[i] = (dtriangle_t) {
			.facesfront = 1,
			.vertindex = { VectorExpand (iqmtris[i].vertex) },
		};
	}
}

void
sw_Mod_IQMFinish (mod_iqm_ctx_t *iqm_ctx)
{
	iqm_ctx->mod->clear = sw_iqm_clear;
	//sw->blend_palette = Mod_IQMBuildBlendPalette (iqm, &sw->palette_size);

	auto model = iqm_ctx->qf_model;
	auto meshes = iqm_ctx->qf_meshes;

	uint32_t palette_size;
	auto blend_palette = Mod_IQMBuildBlendPalette (iqm_ctx, &palette_size);

	int         numv = iqm_ctx->hdr->num_vertexes;
	int         numt = iqm_ctx->hdr->num_triangles;
	size_t      size = sizeof (sw_alias_mesh_t)
						+ sizeof (qfm_blend_t[palette_size])
						+ sizeof (qfm_attrdesc_t[4 * model->meshes.count])
						+ sizeof (dtriangle_t[numt])
						+ sizeof (float[numv * 9]);

	const char *name = iqm_ctx->mod->name;
	sw_alias_mesh_t *rmesh = Hunk_AllocName (nullptr, size, name);
	auto blend = (qfm_blend_t *) &rmesh[1];
	auto attribs = (qfm_attrdesc_t *) &blend[palette_size];
	auto tris     = (dtriangle_t *) &attribs[4 * model->meshes.count];
	auto verts = (void *) &tris[numt];
	auto position = (float *) verts;
	auto texcoord = (float *) &position[3];	// converted to int32_t later
	auto normal = (float *) &texcoord[2];
	auto matind = (uint32_t *) &normal[3];
	size_t stride = 9;

	memcpy (blend, blend_palette, sizeof (qfm_blend_t) * palette_size);

	attribs[0] = (qfm_attrdesc_t) {
		.offset = (byte *) position - (byte *) position,
		.stride = stride * sizeof (float),
		.attr = qfm_position,
		.abs = 1,
		.type = qfm_f32,
		.components = 3,
	};
	attribs[1] = (qfm_attrdesc_t) {
		.offset = (byte *) texcoord - (byte *) position,
		.stride = stride * sizeof (float),
		.attr = qfm_texcoord,
		.abs = 1,
		.type = qfm_s32,
		.components = 2,
	};
	attribs[2] = (qfm_attrdesc_t) {
		.offset = (byte *) normal - (byte *) position,
		.stride = stride * sizeof (float),
		.attr = qfm_normal,
		.abs = 1,
		.type = qfm_f32,
		.components = 3,
	};
	attribs[3] = (qfm_attrdesc_t) {
		.offset = (byte *) matind - (byte *) position,
		.stride = stride * sizeof (float),
		.attr = qfm_joints,
		.abs = 1,
		.type = qfm_u32,
		.components = 1,
	};
	for (uint32_t i = 1; i < model->meshes.count; i++) {
		memcpy (&attribs[4 * i], attribs, sizeof (qfm_attrdesc_t[4]));
	}
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		for (int j = 0; j < 4; j++) {
			attribs[i * 4 + j].offset += (byte *) verts - (byte *) &meshes[i];
		}
	}

	auto walk_tris = tris;
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		meshes[i].indices = (byte *) walk_tris - (byte *) &meshes[i];
		meshes[i].index_type = qfm_special; // dtriangle_t
		meshes[i].attributes = (qfm_loc_t) {
			.offset = (byte *) attribs - (byte *) &meshes[i],
			.count = 4,
		};
		//meshes[i].vertices.offset = (byte *) verts - (byte *) &meshes[i];
		meshes[i].vertex_stride = stride * sizeof (float);

		walk_tris +=  meshes[i].triangle_count;
	}

	*rmesh = (sw_alias_mesh_t) {
		.size = 1,//FIXME what's right?
		.blend_palette = (byte *) blend - (byte *) model,
		.palette_size = palette_size,
	};
	model->render_data = (byte *) rmesh - (byte *) model;

	sw_iqm_convert_tris (iqm_ctx, tris);
	sw_iqm_load_textures (iqm_ctx);
}
