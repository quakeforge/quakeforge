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

#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"

#include "r_internal.h"
#include "vid_vulkan.h"

void
Vulkan_Compose_Draw (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_renderpass_t *renderpass = rFrame->renderpass;

	composectx_t *cctx = ctx->compose_context;
	composeframe_t *cframe = &cctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = cframe->cmd;

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passCompose], cmd);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		renderpass->renderpass, QFV_passCompose,
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, "compose", { 0, 0.2, 0.6, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  cctx->pipeline);

	cframe->imageInfo[0].imageView
		= renderpass->attachment_views->a[QFV_attachOpaque];
	dfunc->vkUpdateDescriptorSets (device->dev, COMPOSE_IMAGE_INFOS,
								   cframe->descriptors, 0, 0);

	VkDescriptorSet sets[] = {
		cframe->descriptors[0].dstSet,
		Vulkan_Translucent_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									cctx->layout, 0, 2, sets, 0, 0);

	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

static VkDescriptorImageInfo base_image_info = {
	0, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	0, 0, 0
};

void
Vulkan_Compose_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "compose init");

	composectx_t *cctx = calloc (1, sizeof (composectx_t));
	ctx->compose_context = cctx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&cctx->frames, frames);
	DARRAY_RESIZE (&cctx->frames, frames);
	cctx->frames.grow = 0;

	cctx->pipeline = Vulkan_CreateGraphicsPipeline (ctx, "compose");
	cctx->layout = Vulkan_CreatePipelineLayout (ctx, "compose_layout");

	__auto_type cmdSet = QFV_AllocCommandBufferSet (1, alloca);

	__auto_type attach = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		attach->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "compose_attach");
	}
	__auto_type attach_pool = Vulkan_CreateDescriptorPool (ctx,
														"compose_attach_pool");

	__auto_type attach_set = QFV_AllocateDescriptorSet (device, attach_pool,
														attach);
	for (size_t i = 0; i < frames; i++) {
		__auto_type cframe = &cctx->frames.a[i];

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdSet);
		cframe->cmd = cmdSet->a[0];

		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 cframe->cmd, "cmd:compose");
		for (int j = 0; j < COMPOSE_IMAGE_INFOS; j++) {
			cframe->imageInfo[j] = base_image_info;
			cframe->imageInfo[j].sampler = 0;
			cframe->descriptors[j] = base_image_write;
			cframe->descriptors[j].dstSet = attach_set->a[i];
			cframe->descriptors[j].dstBinding = j;
			cframe->descriptors[j].pImageInfo = &cframe->imageInfo[j];
		}
	}
	free (attach_set);
	qfvPopDebug (ctx);
}

void
Vulkan_Compose_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	composectx_t *cctx = ctx->compose_context;

	dfunc->vkDestroyPipeline (device->dev, cctx->pipeline, 0);
	free (cctx->frames.a);
	free (cctx);
}
