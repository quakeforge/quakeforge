/*
	vulkan_translucent.c

	Vulkan translucent pass pipeline

	Copyright (C) 2022       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/11/30

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

#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#define MAX_FRAGMENTS (1<<24)

static const char * __attribute__((used)) translucent_pass_names[] = {
	"clear",
	"blend",
};

void
Vulkan_Translucent_Draw (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_renderpass_t *renderpass = rFrame->renderpass;

	translucentctx_t *tctx = ctx->translucent_context;
	translucentframe_t *tframe = &tctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = tframe->cmdSet.a[QFV_translucentBlend];

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passTranslucentFinal], cmd);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		renderpass->renderpass, QFV_passTranslucentFinal,
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, "translucent", { 0, 0.2, 0.6, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  tctx->pipeline);
	VkDescriptorSet set[] = {
		tframe->descriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									tctx->layout, 0, 1, set, 0, 0);


	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_Translucent_Init (vulkan_ctx_t *ctx)
{
	if (ctx->translucent_context) {//FIXME
		return;
	}
	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "translucent init");

	translucentctx_t *tctx = calloc (1, sizeof (translucentctx_t));
	ctx->translucent_context = tctx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&tctx->frames, frames);
	DARRAY_RESIZE (&tctx->frames, frames);
	tctx->frames.grow = 0;

	tctx->pipeline = Vulkan_CreateGraphicsPipeline (ctx, "oit");
	tctx->layout = Vulkan_CreatePipelineLayout (ctx, "oit_layout");

	__auto_type setLayout = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		setLayout->a[i] = Vulkan_CreateDescriptorSetLayout (ctx, "oit_set");
	}
	__auto_type pool = Vulkan_CreateDescriptorPool (ctx, "oit_pool");

	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, setLayout);
	for (size_t i = 0; i < frames; i++) {
		__auto_type tframe = &tctx->frames.a[i];

		DARRAY_INIT (&tframe->cmdSet, QFV_translucentNumPasses);
		DARRAY_RESIZE (&tframe->cmdSet, QFV_translucentNumPasses);
		tframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &tframe->cmdSet);

		tframe->descriptors = sets->a[i];

		for (int j = 0; j < QFV_translucentNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 tframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:translucent:%zd:%s", i,
									 translucent_pass_names[j]));
		}
	}
	free (sets);
	qfvPopDebug (ctx);
}

void
Vulkan_Translucent_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	translucentctx_t *tctx = ctx->translucent_context;

	if (tctx->resources) {
		QFV_DestroyResource (device, tctx->resources);
		free (tctx->resources);
		tctx->resources = 0;
	}

	dfunc->vkDestroyPipeline (device->dev, tctx->pipeline, 0);
	free (tctx->frames.a);
	free (tctx);
}

VkDescriptorSet
Vulkan_Translucent_Descriptors (vulkan_ctx_t *ctx, int frame)
{
	__auto_type tctx = ctx->translucent_context;
	return tctx->frames.a[frame].descriptors;
}

void
Vulkan_Translucent_CreateBuffers (vulkan_ctx_t *ctx, VkExtent2D extent)
{
	if (!ctx->translucent_context) {//FIXME
		Vulkan_Translucent_Init (ctx);
	}
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type tctx = ctx->translucent_context;
	size_t      frames = ctx->frames.size;

	if (tctx->resources) {
		QFV_DestroyResource (device, tctx->resources);
		free (tctx->resources);
		tctx->resources = 0;
	}
	tctx->resources = malloc (sizeof (qfv_resource_t)
							  // heads image
							  + frames * sizeof (qfv_resobj_t)
							  // heads image view
							  + frames * sizeof (qfv_resobj_t)
							  // fragment buffer
							  + frames * sizeof (qfv_resobj_t)
							  // fragment count
							  + frames * sizeof (qfv_resobj_t));
	__auto_type heads_objs = (qfv_resobj_t *) &tctx->resources[1];
	__auto_type head_views_objs = &heads_objs[frames];
	__auto_type buffer_objs = &head_views_objs[frames];
	__auto_type count_objs = &buffer_objs[frames];
	tctx->resources[0] = (qfv_resource_t) {
		.name = "oit",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 4 * frames,
		.objects = heads_objs,
	};
	for (size_t i = 0; i < frames; i++) {
		heads_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "heads:%zd", i),
			.type = qfv_res_image,
			.image = {
				.type = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32_SINT,
				.extent = { extent.width, extent.height, 1 },
				.num_mipmaps = 1,
				.num_layers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_IMAGE_USAGE_STORAGE_BIT
						| VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			},
		};
		head_views_objs[i] = (qfv_resobj_t) {
			.name = "head view",
			.type = qfv_res_image_view,
			.image_view = {
				.image = i,
				.type = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_R32_SINT,
				.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
			},
		};
		buffer_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "frags:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_transfrag_t) * MAX_FRAGMENTS,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		count_objs[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "count:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = 2 * sizeof (qfv_transtate_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
	}
	QFV_CreateResource (device, tctx->resources);

	for (size_t i = 0; i < frames; i++) {
		__auto_type tframe = &tctx->frames.a[i];
		tframe->heads = heads_objs[i].image.image;
		tframe->state = count_objs[i].buffer.buffer;

		VkDescriptorImageInfo imageInfo[] = {
			{ 0, head_views_objs[i].image_view.view, VK_IMAGE_LAYOUT_GENERAL },
		};
		VkDescriptorBufferInfo bufferInfo[] = {
			{ count_objs[i].buffer.buffer, 0, VK_WHOLE_SIZE },
			{ buffer_objs[i].buffer.buffer, 0, VK_WHOLE_SIZE },
		};
		VkWriteDescriptorSet write[] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
				tframe->descriptors, 2, 0, 1,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = imageInfo },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
				tframe->descriptors, 0, 0, 2,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = bufferInfo },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);
	}
}

static void
translucent_clear (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	translucentctx_t *tctx = ctx->translucent_context;
	__auto_type tframe = &tctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = tframe->cmdSet.a[QFV_translucentClear];

	DARRAY_APPEND (&rFrame->subpassCmdSets[0], cmd);
	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		0, 0, 0,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = tframe->heads;
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
	VkClearColorValue clear_color[] = {
		{ .int32 = {-1, -1, -1, -1} },
	};
	VkImageSubresourceRange ranges[] = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	};
	dfunc->vkCmdClearColorImage (cmd, tframe->heads,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 clear_color, 1, ranges);
	ib = imageBarriers[qfv_LT_TransferDst_to_General];
	ib.barrier.image = tframe->heads;
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	dfunc->vkEndCommandBuffer (cmd);

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	qfv_transtate_t *state = QFV_PacketExtend (packet, 2 * sizeof (*state));
	*state = (qfv_transtate_t) { 0, MAX_FRAGMENTS };
	__auto_type bb = &bufferBarriers[qfv_BB_TransferWrite_to_ShaderRW];
	QFV_PacketCopyBuffer (packet, tframe->state, bb);
	QFV_PacketSubmit (packet);
}

void
Vulkan_Translucent_CreateRenderPasses (vulkan_ctx_t *ctx)
{
	__auto_type rp = QFV_RenderPass_New (ctx, "translucent", translucent_clear);
	rp->order = QFV_rp_translucent;
	DARRAY_APPEND (&ctx->renderPasses, rp);
}
