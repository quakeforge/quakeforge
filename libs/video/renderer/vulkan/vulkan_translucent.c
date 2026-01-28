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

#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/swapchain.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static void
trans_delete_buffers (vulkan_ctx_t *ctx)
{
	auto tctx = ctx->translucent_context;

	auto resources = &tctx->resources.array[tctx->resources.active];
	if (resources->memory) {
		QFV_QueueResourceDelete (ctx, resources);
		if (++tctx->resources.active >= tctx->resources.count) {
			tctx->resources.active = 0;
		}
	}
}

static void
trans_create_buffers (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto tctx = ctx->translucent_context;
	size_t frames = tctx->frames.size;

	auto resources = &tctx->resources.array[tctx->resources.active];

	for (uint32_t i = 0; i < resources->num_objects; i++) {
		auto obj = &resources->objects[i];
		if (obj->type == qfv_res_image) {
			auto img = &obj->image;
			if (img->num_layers == 6) {
				auto e = min (tctx->extent.width, tctx->extent.height);
				img->extent.width = e;
				img->extent.height = e;
			} else {
				img->extent.width = tctx->extent.width;
				img->extent.height = tctx->extent.height;
			}
		}
	}
	QFV_CreateResource (device, resources);

	for (size_t i = 0; i < frames; i++) {
		tctx->frames.a[i].need_update = true;
	}
}

static void
trans_update_descriptors (vulkan_ctx_t *ctx, int i)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto tctx = ctx->translucent_context;

	auto resources = &tctx->resources.array[tctx->resources.active];
	auto obj = resources->objects;

	auto tframe = &tctx->frames.a[i];
	auto heads_view = obj[tframe->heads_view].image_view.view;
	auto cube_heads_view = obj[tframe->cube_heads_view].image_view.view;
	auto state = obj[tframe->state].buffer.buffer;
	auto frags = obj[tframe->frags].buffer.buffer;

	VkDescriptorImageInfo flat_imageInfo[] = {
		{ 0, heads_view, VK_IMAGE_LAYOUT_GENERAL },
	};
	VkDescriptorImageInfo cube_imageInfo[] = {
		{ 0, cube_heads_view, VK_IMAGE_LAYOUT_GENERAL },
	};
	VkDescriptorBufferInfo bufferInfo[] = {
		{ state, 0, VK_WHOLE_SIZE },
		{ frags, 0, VK_WHOLE_SIZE },
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			tframe->flat, 2, 0, 1,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = flat_imageInfo },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			tframe->flat, 0, 0, 2,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = bufferInfo },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			tframe->cube, 2, 0, 1,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = cube_imageInfo },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			tframe->cube, 0, 0, 2,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = bufferInfo },
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 4, write, 0, 0);

	tframe->need_update = false;
}

static void
clear_translucent (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto tctx = ctx->translucent_context;
	auto tframe = &tctx->frames.a[ctx->curFrame];
	auto job = ctx->render_context->job;
	auto step = QFV_GetStep (params[0], job);
	auto render = step->render;

	auto set = scr_fisheye ? tframe->cube : tframe->flat;
	QFV_SetDescriptorSet (ctx, ctx->curFrame, tctx->ds_index, set);

	if (tctx->extent.width != render->output.extent.width
		|| tctx->extent.height != render->output.extent.height) {

		tctx->extent = render->output.extent;
		trans_delete_buffers (ctx);
		trans_create_buffers (ctx);
	}
	if (tframe->need_update) {
		trans_update_descriptors (ctx, ctx->curFrame);
	}

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "trans.clear"));

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	auto resources = &tctx->resources.array[tctx->resources.active];
	auto obj = resources->objects;
	auto img = scr_fisheye ? &obj[tframe->cube_heads] : &obj[tframe->heads];
	auto image = img->image.image;
	qfv_imagebarrier_t ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = image;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);
	VkClearColorValue clear_color[] = {
		{ .int32 = {-1, -1, -1, -1} },
	};
	VkImageSubresourceRange ranges[] = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS },
	};
	dfunc->vkCmdClearColorImage (cmd, image,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 clear_color, 1, ranges);
	ib = imageBarriers[qfv_LT_TransferDst_to_General];
	ib.barrier.image = image;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0,
								 1, &ib.barrier);

	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging, "trans.clear");
	qfv_transtate_t *state = QFV_PacketExtend (packet, 2 * sizeof (*state));
	*state = (qfv_transtate_t) { 0, tctx->maxFragments };
	auto sb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	QFV_PacketCopyBuffer (packet, obj[tframe->state].buffer.buffer, 0, &sb,
		&(qfv_bufferbarrier_t) {
			.srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
			.dstStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.barrier = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
							   | VK_ACCESS_SHADER_WRITE_BIT,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			},
		});
	QFV_PacketSubmit (packet);
}

static void
trans_setup_resources (vulkan_ctx_t *ctx, qfv_resource_t *resources,
					   qfv_resobj_t *heads_objs,
					   qfv_resobj_t *cube_heads_objs,
					   qfv_resobj_t *head_views_objs,
					   qfv_resobj_t *cube_head_views_objs,
					   qfv_resobj_t *buffer_objs,
					   qfv_resobj_t *count_objs)
{
	auto  tctx = ctx->translucent_context;
	size_t frames = tctx->frames.size;

	resources[0] = (qfv_resource_t) {
		.name = "oit",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 6 * frames,
		.objects = heads_objs,
	};
	for (size_t i = 0; i < frames; i++) {
		heads_objs[i] = (qfv_resobj_t) {
			.name = nva ("heads:%zd", i),
			.type = qfv_res_image,
			.image = {
				.type = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32_SINT,
				.extent = { 0, 0, 1 },
				.num_mipmaps = 1,
				.num_layers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_IMAGE_USAGE_STORAGE_BIT
						| VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			},
		};
		cube_heads_objs[i] = heads_objs[i];
		cube_heads_objs[i].name = nva ("cube_heads:%zd", i);
		cube_heads_objs[i].image.extent = (VkExtent3D) { 0, 0, 1 };
		cube_heads_objs[i].image.num_layers = 6;
		head_views_objs[i] = (qfv_resobj_t) {
			.name = strdup (heads_objs[i].name),
			.type = qfv_res_image_view,
			.image_view = {
				.image = i,
				.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.format = VK_FORMAT_R32_SINT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				},
			},
		};
		cube_head_views_objs[i] = head_views_objs[i];
		cube_head_views_objs[i].name = strdup (cube_heads_objs[i].name);
		cube_head_views_objs[i].image_view.image = i + frames;
		buffer_objs[i] = (qfv_resobj_t) {
			.name = nva ("frags:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_transfrag_t) * tctx->maxFragments,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		count_objs[i] = (qfv_resobj_t) {
			.name = nva ("count:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = 2 * sizeof (qfv_transtate_t),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
	}
}

static void
trans_create_resources (vulkan_ctx_t *ctx)
{
	auto  tctx = ctx->translucent_context;
	size_t frames = tctx->frames.size;

	size_t size = sizeof (qfv_resource_t)
				// heads images (flat + cube)
				+ sizeof (qfv_resobj_t[frames]) * 2
				// heads image views (flat + cube)
				+ sizeof (qfv_resobj_t[frames]) * 2
				// fragment buffer
				+ sizeof (qfv_resobj_t[frames])
				// fragment count
				+ sizeof (qfv_resobj_t[frames]);

	tctx->resources.array = malloc (frames * size);
	tctx->resources.active = 0;
	tctx->resources.count = frames;
	void *res = &tctx->resources.array[frames];
	for (uint32_t j = 0; j < frames; j++) {
		auto heads_objs = (qfv_resobj_t *) res;
		auto cube_heads_objs = &heads_objs[frames];
		auto head_views_objs = &cube_heads_objs[frames];
		auto cube_head_views_objs = &head_views_objs[frames];
		auto buffer_objs = &cube_head_views_objs[frames];
		auto count_objs = &buffer_objs[frames];
		res = &count_objs[frames];

		trans_setup_resources (ctx, &tctx->resources.array[j],
							   heads_objs, cube_heads_objs,
							   head_views_objs, cube_head_views_objs,
							   buffer_objs, count_objs);

		for (size_t i = 0; i < frames; i++) {
			auto objects = tctx->resources.array[j].objects;
			auto tframe = &tctx->frames.a[i];
			tframe->heads = &heads_objs[i] - objects;
			tframe->cube_heads = &cube_heads_objs[i] - objects;
			tframe->heads_view = &head_views_objs[i] - objects;
			tframe->cube_heads_view = &cube_head_views_objs[i] - objects;
			tframe->state = &count_objs[i] - objects;
			tframe->frags = &buffer_objs[i] - objects;
		}
	}
}

static void
translucent_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfv_device_t *device = ctx->device;
	translucentctx_t *tctx = ctx->translucent_context;

	if (tctx->resources.array) {
		for (uint32_t j = 0; j < tctx->resources.count; j++) {
			auto resources = &tctx->resources.array[j];
			QFV_DestroyResource (device, resources);
			for (uint32_t i = 0; i < resources->num_objects; i++) {
				auto obj = &resources->objects[i];
				free ((char *) obj->name);
			}
		}
	}
	free (tctx->resources.array);
	free (tctx->frames.a);
	free (tctx);
}

static void
translucent_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "translucent init");
	auto tctx = ctx->translucent_context;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&tctx->frames, frames);
	DARRAY_RESIZE (&tctx->frames, frames);
	tctx->frames.grow = 0;

	tctx->maxFragments = vulkan_oit_fragments * 1024 * 1024;

	auto dsmanager = QFV_Render_DSManager (ctx, "oit_set");
	tctx->ds_index = QFV_GetDSIndex (ctx, "oit_set");
	for (size_t i = 0; i < frames; i++) {
		tctx->frames.a[i] = (translucentframe_t) {
			.flat = QFV_DSManager_AllocSet (dsmanager),
			.cube = QFV_DSManager_AllocSet (dsmanager),
		};
	}
	trans_create_resources (ctx);
	qfvPopDebug (ctx);
}

static void
translucent_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, translucent_shutdown);
	QFV_Render_AddStartup (ctx, translucent_startup);

	translucentctx_t *tctx = calloc (1, sizeof (translucentctx_t));
	ctx->translucent_context = tctx;
}

static exprtype_t *clear_translucent_params[] = {
	&cexpr_string,
};
static exprfunc_t clear_translucent_func[] = {
	{ .func = clear_translucent, .num_params = 1, clear_translucent_params },
	{}
};

static exprfunc_t translucent_init_func[] = {
	{ .func = translucent_init },
	{}
};

static exprsym_t translucent_task_syms[] = {
	{ "clear_translucent", &cexpr_function, clear_translucent_func },
	{ "translucent_init", &cexpr_function, translucent_init_func },
	{}
};

void
Vulkan_Translucent_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, translucent_task_syms);
}

VkDescriptorSet
Vulkan_Translucent_Descriptors (vulkan_ctx_t *ctx, int frame)
{
	auto tctx = ctx->translucent_context;
	auto tframe = &tctx->frames.a[frame];
	return scr_fisheye ? tframe->cube : tframe->flat;
}
