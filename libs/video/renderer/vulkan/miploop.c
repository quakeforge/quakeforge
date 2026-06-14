/*
	miploop.c

	Looped mipmap rendering

	Copyright (C) 2026       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2026/6/11

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

#include "qfalloca.h"

#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/heapsort.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/script.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/miploop.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/projection.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "compat.h"

#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"

static void
miploop_parse_textures (vulkan_ctx_t *ctx, miploopctx_t *mctx,
						memsuper_t *memsuper, plitem_t *textures)
{
	if (PL_Type (textures) != QFArray) {
		Sys_Error (MAG"%d:miploop textures param not an array"DFL,
				   PL_Line (textures));
	}
	int num_textures = PL_A_NumObjects (textures);
	DARRAY_RESIZE (&mctx->textures, num_textures);
	for (int i = 0; i < num_textures; i++) {
		const char *var_name = nullptr;
		const char *set_name = nullptr;
		auto texset = PL_ObjectAtIndex (textures, i);
		if (PL_A_NumObjects (texset) != 2) {
		}
		var_name = PL_String (PL_ObjectAtIndex (texset, 0));
		set_name = PL_String (PL_ObjectAtIndex (texset, 1));
		if (!var_name || !set_name) {
			Sys_Error (MAG"%d:miploop textures param not an array of 2 "
					   " strings"DFL, PL_Line (texset));
		}
		mctx->textures.a[i] = (miplooptex_t) {
			.var_name = cmemstrdup (memsuper, var_name),
			.set_name = cmemstrdup (memsuper, set_name),
			.set_index = QFV_GetDSIndex (ctx, set_name),
		};
	}
}

static void
miploop_init_loop (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	miploopctx_t *mctx = malloc (sizeof (miploopctx_t));
	ctx->miploop_context = mctx;

	auto textures = *(plitem_t **) params[0]->value;
	int layers = *(int *) params[1]->value;
	auto img_name = *(const char **) params[2]->value;
	auto sinfo = taskctx->stepinfo;
	auto rt = sinfo->render_template;
	Sys_Printf ("miploop_init_loop: %s %s %d %p\n",
				sinfo->name, img_name, layers, textures);
	if (rt->num_renderpasses != 1) {
		Sys_Error ("%d:%s: need exactly one render pass in render template",
				   sinfo->line, sinfo->name);
	}
	auto memsuper = taskctx->memsuper;
	char *fb_name = cmemalloc (memsuper, strlen (img_name) + 2);
	fb_name[0] = '$';
	strcpy (fb_name + 1, img_name);
	sinfo->render = cmemalloc (memsuper, sizeof (qfv_renderinfo_t));
	auto render = sinfo->render;
	*render = (qfv_renderinfo_t) {
		.color = rt->color,
		.name = "miploop_render",
		.num_renderpasses = 1,
		.renderpasses = cmemalloc (memsuper, sizeof (qfv_renderpassinfo_t)),
	};
	auto rpinfo = rt->renderpasses;
	auto rp = render->renderpasses;
	uint32_t num_subpasses = rpinfo->num_subpasses;
	if (num_subpasses > 0 && strcmp (rpinfo->subpasses[num_subpasses - 1].name,
									 "$external") == 0) {
		num_subpasses--;
	}
	VkRenderPassMultiviewCreateInfo *mv = cmemalloc (memsuper,
			sizeof (VkRenderPassMultiviewCreateInfo));
	uint32_t *all_viewmasks = cmemalloc (memsuper,
			sizeof (uint32_t[num_subpasses]));
	{
		auto viewmasks = &all_viewmasks[0 * num_subpasses];
		mv[0] = (VkRenderPassMultiviewCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
			.pNext = rpinfo->pNext,
			.subpassCount = num_subpasses,
			.pViewMasks = viewmasks,
		};
		rp[0] = *rpinfo;
		rp[0].pNext =&mv[0];
		rp[0].name = cmemstrdup (memsuper, vac (ctx->va_ctx, "%s:%d",
												rpinfo->name, 0 + 1));
		for (uint32_t j = 0; j < num_subpasses; j++) {
			viewmasks[j] = ~0u >> (32 - layers);
		}
	}

	auto image_info = QFV_FindImageInfo (ctx, img_name);
	*mctx = (miploopctx_t) {
		//FIXME multiple instances
		.miploop_info = (qfv_attachmentinfo_t) {
			.name = fb_name,
			.format = image_info->format,
			.samples = 1,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,//FIXME plist
		},
		.image_info = image_info,
		.img_name = img_name,
		.layers = layers,
		.textures = DARRAY_STATIC_INIT (4),
	};
	qfv_attachmentinfo_t *attachments[] = {
		&mctx->miploop_info,
	};
	QFV_Render_AddAttachments (ctx, 1, attachments);
	miploop_parse_textures (ctx, mctx, memsuper, textures);
}

static VkImageView
create_view (vulkan_ctx_t *ctx, qfv_imageinfo_t *image_info,
			 uint32_t layer, uint32_t level)
{
	auto mctx = ctx->miploop_context;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkImageView view;
	dfunc->vkCreateImageView (device->dev,
		&(VkImageViewCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = QFV_GetImage (ctx, image_info),
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,	//FIXME
			.format = image_info->format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = level,
				.levelCount = 1,
				.baseArrayLayer = layer,
				.layerCount = mctx->layers,
			},
		}, nullptr, &view);
	return view;
}

static VkFramebuffer
create_framebuffer (vulkan_ctx_t *ctx, vec3u_t size,
					VkImageView view, VkRenderPass renderpass)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev,
		&(VkFramebufferCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderpass,
			.attachmentCount = 1,
			.pAttachments = &view,
			.width = size.v[0],
			.height = size.v[1],
			.layers = 1,
		}, 0, &framebuffer);
	return framebuffer;
}

static void
miploop_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto mctx = ctx->miploop_context;
	auto step = taskctx->step;
	auto render = step->render;
	auto ii = mctx->image_info;
	auto layer = *(int32_t *) params[0]->value;

	for (uint32_t i = 0; i < mctx->textures.size; i++) {
		auto tex = &mctx->textures.a[i];
		if (tex->var_name && !tex->tex_id) {
			tex->tex_id = QFV_GetJobBlackboardVar (taskctx->job, tex->var_name);
		}
		auto texture = QFV_GetTexture (ctx, *tex->tex_id);
		QFV_SetDescriptorSet (ctx, ctx->curFrame, tex->set_index, texture);
	}

	//FIXME support 3d mips
	vec3u_t base = {{ii->extent.width, ii->extent.height, 1}};
	vec3u_t size = base;
	vec2u_t range = {0, QFV_MipLevels (base.v[0], base.v[1])};

	*mctx->miploop_base = base;
	*mctx->miploop_layer = layer;
	*mctx->miploop_range = range;
	(*mctx->miploop_range)[1] -= 1;

	auto renderpass = render->active;

	auto tctx = *taskctx;
	tctx.data = nullptr;
	int count = range[1] - range[0];
	VkFramebuffer fbuffers[count + 1] = {};
	VkImageView views[count + 1] = {};
	for (int i = 0; i < count; i++) {
		uint32_t level = i + range[0];
		*mctx->miploop_size = size;
		*mctx->miploop_level = level;
		views[i] = create_view (ctx, mctx->image_info, layer, level);
		auto bi = &renderpass->beginInfo;
		fbuffers[i] = create_framebuffer (ctx, size, views[i], bi->renderPass);
		bi->framebuffer = fbuffers[i];
		auto name = vac (ctx->va_ctx, "%s:miploop:%d:%d", step->label.name,
						 layer, level);
		renderpass->label.name = name;
		QFV_RunRenderPass (&tctx, renderpass, size.v[0], size.v[1]);

		size.v[0] = max (size.v[0] >> 1, 1);
		size.v[1] = max (size.v[1] >> 1, 1);
	}
	renderpass->beginInfo.framebuffer = 0;
	for (int i = 0; i < count; i++) {
		QFV_QueueFramebufferDelete (ctx, fbuffers[i]);
		QFV_QueueImageViewDelete (ctx, views[i]);
	}
}

static void
miploop_shutdown (exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfZoneScoped (true);
	auto mctx = ctx->miploop_context;

	//QFV_DestroyResource (device, mctx->light_resources);
	//free (mctx->light_resources);

	free (mctx);
}

static void
miploop_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	//auto mctx = ctx->miploop_context;
	qfvPushDebug (ctx, "mipmap init");
	qfvPopDebug (ctx);
}

static void
miploop_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto mctx = ctx->miploop_context;

	QFV_Render_AddShutdown (ctx, miploop_shutdown);
	QFV_Render_AddStartup (ctx, miploop_startup);

	mctx->miploop_base = QFV_GetBlackboardVar (ctx, "miploop_base");
	mctx->miploop_layer = QFV_GetBlackboardVar (ctx, "miploop_layer");
	mctx->miploop_size = QFV_GetBlackboardVar (ctx, "miploop_size");
	mctx->miploop_level = QFV_GetBlackboardVar (ctx, "miploop_level");
	mctx->miploop_range = QFV_GetBlackboardVar (ctx, "miploop_range");
}

static exprtype_t *stepref_param[] = {
	&cexpr_string,
};

static exprtype_t *miploop_init_loop_params[] = {
	&cexpr_plitem,
	&cexpr_uint,
	&cexpr_string,
};

static exprfunc_t miploop_init_loop_func[] = {
	{ .func = miploop_init_loop,
		.num_params = countof (miploop_init_loop_params),
		.param_types = miploop_init_loop_params },
	{}
};

static exprtype_t *miploop_draw_params[] = {
	&cexpr_uint,
};

static exprfunc_t miploop_draw_func[] = {
	{ .func = miploop_draw,
		.num_params = countof (miploop_draw_params),
		.param_types = miploop_draw_params },
	{}
};

static exprfunc_t miploop_init_func[] = {
	{ .func = miploop_init },
	{}
};

static exprsym_t miploop_task_syms[] = {
	{ "miploop_init_loop", &cexpr_function, miploop_init_loop_func },
	{ "miploop_draw", &cexpr_function, miploop_draw_func },

	{ "miploop_init", &cexpr_function, miploop_init_func },
	{}
};

void
QFV_MipLoop_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);

	QFV_Render_AddTasks (ctx, miploop_task_syms);

	stepref_param[0] = &ctx->render_context->step_type;
}
