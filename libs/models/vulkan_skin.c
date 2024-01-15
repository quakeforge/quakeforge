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

#include "QF/Vulkan/device.h"
#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_model.h"

#include "mod_internal.h"
#include "vid_vulkan.h"

void
Vulkan_Skin_Clear (qfv_alias_skin_t *skin, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	Vulkan_AliasRemoveSkin (ctx, skin);
	dfunc->vkDestroyImageView (device->dev, skin->view, 0);
	dfunc->vkDestroyImage (device->dev, skin->image, 0);
	dfunc->vkFreeMemory (device->dev, skin->memory, 0);
}

void
Vulkan_Skin_SetupSkin (skin_t *skin, struct vulkan_ctx_s *ctx)
{
	tex_t *tex = skin->tex;
	// FIXME this is gross, but the vulkan skin (and even model) handling
	// needs a complete overhaul.
	aliashdr_t dummy_header = {
		.mdl.skinwidth = tex->width,
		.mdl.skinheight = tex->height,
	};
	model_t dummy_model = { };
	mod_alias_ctx_t alias_ctx = {
		.header = &dummy_header,
		.mod = &dummy_model,
	};
	maliasskindesc_t skindesc = {};
	int    skinsize = tex->width * tex->height;
	size_t hunk_mark = Hunk_LowMark (0);
	auto vkskin =  Vulkan_Mod_LoadSkin (&alias_ctx, tex->data, skinsize,
										0, 0, false, &skindesc, ctx);
	skin->tex = malloc (sizeof (*vkskin));
	*(qfv_alias_skin_t *) skin->tex = *vkskin;
	Hunk_FreeToLowMark (0, hunk_mark);
}

void
Vulkan_Skin_Destroy (skin_t *skin, struct vulkan_ctx_s *ctx)
{
	auto alias_skin = (qfv_alias_skin_t *) skin->tex;
	Vulkan_Skin_Clear (alias_skin, ctx);
}
