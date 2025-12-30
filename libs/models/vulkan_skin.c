/*
	vulkan_skin.c

	Vulkan Skin support

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2024/1/15

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/device.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/qf_mesh.h"
#include "QF/Vulkan/qf_model.h"
#include "QF/Vulkan/qf_texture.h"

#include "mod_internal.h"
#include "vid_vulkan.h"

void
Vulkan_Skin_Clear (qfv_skin_t *skin, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	Vulkan_MeshRemoveSkin (ctx, skin);
	if (skin->resource) {
		QFV_DestroyResource (device, skin->resource);
	} else {
		Vulkan_UnloadTex (ctx, (qfv_tex_t *) skin->tex);
	}
}

void
Vulkan_Skin_SetupSkin (skin_t *skin, struct vulkan_ctx_s *ctx)
{
	tex_t *tex = skin->tex;
	// FIXME this is gross, but the vulkan skin (and even model) handling
	// needs a complete overhaul.
	model_t dummy_model = { };
	mod_alias_ctx_t alias_ctx = {
		.skinwidth = tex->width,
		.skinheight = tex->height,
		.mod = &dummy_model,
	};
	keyframe_t skindesc = {};
	int    skinsize = tex->width * tex->height;
	mod_alias_skin_t askin = {
		.texels = tex->data,
		.skindesc = &skindesc,
	};
	if (tex->format == tex_palette) {
		qfv_skin_t *vskin = Vulkan_Mod_AllocSkins (1, false);
		skin->tex = (tex_t *) vskin;
		Vulkan_Mod_LoadSkin (&alias_ctx, &askin, skinsize, vskin, ctx);
	} else {
		tex_t array_tex[3] = { *tex, *tex, *tex };
		size_t size = ImageSize (tex, false);
		array_tex[1].data = array_tex[2].data = calloc (size, 1);
		auto vtex = Vulkan_LoadTexArray (ctx, array_tex, 3, 1,
										 vac (ctx->va_ctx, "skin:%d",
											  skin->id));
		free (array_tex[2].data);
		qfv_skin_t *vskin = malloc (sizeof (qfv_skin_t));
		*vskin = (qfv_skin_t) {
			.tex = vtex,
			.view = vtex->view,
		};
		Vulkan_MeshAddSkin (ctx, vskin);
		skin->tex = (tex_t *) vskin;
	}
}

void
Vulkan_Skin_Destroy (skin_t *skin, struct vulkan_ctx_s *ctx)
{
	auto alias_skin = (qfv_skin_t *) skin->tex;
	Vulkan_Skin_Clear (alias_skin, ctx);
	free (alias_skin);
}
