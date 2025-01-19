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

#include "r_internal.h"
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
	Vulkan_UpdateTex (ctx, pctx->palette, &tex, 0, 0, 0, 0, true);
}

static void
palette_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "palette shutdown");

	auto pctx = ctx->palette_context;

	Vulkan_UnloadTex (ctx, pctx->palette);
	free (pctx);

	qfvPopDebug (ctx);
}

static void
palette_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "palette init");
	auto pctx = ctx->palette_context;

	pctx->sampler = QFV_Render_Sampler (ctx, "palette_sampler");

	tex_t       tex = {
		.width = 16,
		.height = 16,
		.format = tex_rgb,
		.loaded = 1,
		.data = (byte *) vid.palette,
	};
	pctx->palette = Vulkan_LoadTex (ctx, &tex, 0, "palette");
	pctx->descriptor = Vulkan_CreateCombinedImageSampler (ctx,
														  pctx->palette->view,
														  pctx->sampler);

	qfvPopDebug (ctx);
}

static void
palette_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, palette_shutdown);
	QFV_Render_AddStartup (ctx, palette_startup);

	palettectx_t *pctx = calloc (1, sizeof (palettectx_t));
	ctx->palette_context = pctx;
}

static exprfunc_t palette_init_func[] = {
	{ .func = palette_init },
	{}
};

static exprsym_t palette_task_syms[] = {
	{ "palette_init", &cexpr_function, palette_init_func },
	{}
};

void
Vulkan_Palette_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, palette_task_syms);
}

VkDescriptorSet
Vulkan_Palette_Descriptor (struct vulkan_ctx_s *ctx)
{
	__auto_type pctx = ctx->palette_context;
	return pctx->descriptor;
}
