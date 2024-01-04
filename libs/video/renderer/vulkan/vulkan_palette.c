/*
	vulkan_palette.c

	Vulkan palette image

	Copyright (C) 2022      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/qf_palette.h"
#include "QF/Vulkan/qf_texture.h"

#include "vid_vulkan.h"

void
Vulkan_Palette_Update (vulkan_ctx_t *ctx, const byte *palette)
{
	palettectx_t *pctx = ctx->palette_context;
	tex_t       tex = {
		.width = 16,
		.height = 16,
		.format = tex_rgb,
		.loaded = 1,
		.data = (byte *) palette,
	};
	Vulkan_UpdateTex (ctx, pctx->palette, &tex, 0, 0, 0, 0);
}

void
Vulkan_Palette_Init (vulkan_ctx_t *ctx, const byte *palette)
{
	qfZoneScoped (true);
	qfvPushDebug (ctx, "palette init");

	palettectx_t *pctx = calloc (1, sizeof (palettectx_t));
	ctx->palette_context = pctx;

	pctx->sampler = QFV_Render_Sampler (ctx, "palette_sampler");

	tex_t       tex = {
		.width = 16,
		.height = 16,
		.format = tex_rgb,
		.loaded = 1,
		.data = (byte *) palette,
	};
	pctx->palette = Vulkan_LoadTex (ctx, &tex, 0, "palette");
	pctx->descriptor = Vulkan_CreateCombinedImageSampler (ctx,
														  pctx->palette->view,
														  pctx->sampler);

	qfvPopDebug (ctx);
}

void
Vulkan_Palette_Shutdown (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "palette shutdown");

	__auto_type pctx = ctx->palette_context;

	Vulkan_UnloadTex (ctx, pctx->palette);
	free (pctx);

	qfvPopDebug (ctx);
}

VkDescriptorSet
Vulkan_Palette_Descriptor (struct vulkan_ctx_s *ctx)
{
	__auto_type pctx = ctx->palette_context;
	return pctx->descriptor;
}
