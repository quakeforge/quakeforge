/*
	vulkan_compose.c

	Vulkan compose pass pipeline

	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/2/23

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

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static VkDescriptorImageInfo base_image_info = {
	0, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	0, 0, 0
};

static void
compose_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	int  color_only = *(int *) params[0]->value;

	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto cctx = ctx->compose_context;
	auto cframe = &cctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	auto fb = &taskctx->renderpass->framebuffer;
	cframe->imageInfo[0].imageView = fb->views[QFV_attachColor];
	cframe->imageInfo[1].imageView = fb->views[QFV_attachLight];
	cframe->imageInfo[2].imageView = fb->views[QFV_attachEmission];
	dfunc->vkUpdateDescriptorSets (device->dev, 1,
								   cframe->descriptors, 0, 0);
	if (!color_only) {
		dfunc->vkUpdateDescriptorSets (device->dev, COMPOSE_IMAGE_INFOS,
									   cframe->descriptors, 0, 0);
	}

	VkDescriptorSet sets[] = {
		cframe->descriptors[0].dstSet,
		Vulkan_Translucent_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, sets, 0, 0);

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
}

static exprtype_t *compose_draw_params[] = {
	&cexpr_int,
};
static exprfunc_t compose_draw_func[] = {
	{ .func = compose_draw, .num_params = 1,
		.param_types = compose_draw_params },
	{}
};
static exprsym_t compose_task_syms[] = {
	{ "compose_draw", &cexpr_function, compose_draw_func },
	{}
};

void
Vulkan_Compose_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, compose_task_syms);

	composectx_t *cctx = calloc (1, sizeof (composectx_t));
	ctx->compose_context = cctx;
}

void
Vulkan_Compose_Setup (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	qfvPushDebug (ctx, "compose init");

	auto device = ctx->device;
	auto cctx = ctx->compose_context;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&cctx->frames, frames);
	DARRAY_RESIZE (&cctx->frames, frames);
	cctx->frames.grow = 0;

	auto dsmanager = QFV_Render_DSManager (ctx, "compose_attach");
	for (size_t i = 0; i < frames; i++) {
		auto cframe = &cctx->frames.a[i];
		auto set = QFV_DSManager_AllocSet (dsmanager);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, set,
							 va (ctx->va_ctx, "compose:attach_set:%zd", i));

		for (int j = 0; j < COMPOSE_IMAGE_INFOS; j++) {
			cframe->imageInfo[j] = base_image_info;
			cframe->imageInfo[j].sampler = 0;
			cframe->descriptors[j] = base_image_write;
			cframe->descriptors[j].dstSet = set;
			cframe->descriptors[j].dstBinding = j;
			cframe->descriptors[j].pImageInfo = &cframe->imageInfo[j];
		}
	}
	qfvPopDebug (ctx);
}

void
Vulkan_Compose_Shutdown (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	composectx_t *cctx = ctx->compose_context;

	free (cctx->frames.a);
	free (cctx);
}
