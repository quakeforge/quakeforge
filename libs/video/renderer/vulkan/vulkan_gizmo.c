/*
	vulkan_gizmo.c

	3D drawing loosely based on Guerilla's UIPainter

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "compat.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/qf_gizmo.h"
#include "QF/Vulkan/qf_matrices.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#define MAX_QUEUE_BUFFER (8*1024*1024)
#define MAX_OBJECT_DATA (8*1024*1024)

static void
gizmo_delete_buffers (vulkan_ctx_t *ctx)
{
	auto gctx = ctx->gizmo_context;

	auto resources = &gctx->resources.array[gctx->resources.active];
	if (resources->memory) {
		QFV_QueueResourceDelete (ctx, resources);
		qfv_resourcearray_next (&gctx->resources);
	}
}

static void
gizmo_create_buffers (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto gctx = ctx->gizmo_context;
	size_t frames = gctx->frames.size;

	auto resources = &gctx->resources.array[gctx->resources.active];

	for (uint32_t i = 0; i < resources->num_objects; i++) {
		auto obj = &resources->objects[i];
		if (obj->type == qfv_res_image) {
			auto img = &obj->image;
			img->extent.width = gctx->extent.width;
			img->extent.height = gctx->extent.height;
		}
	}
	QFV_CreateResource (device, resources);

	for (size_t i = 0; i < frames; i++) {
		gctx->frames.a[i].need_update = true;
	}
}

static void
gizmo_update_descriptors (vulkan_ctx_t *ctx, int i)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto gctx = ctx->gizmo_context;

	auto resources = &gctx->resources.array[gctx->resources.active];
	auto obj = resources->objects;

	auto frame = &gctx->frames.a[i];
	auto counts = obj[frame->counts].buffer.buffer;
	auto queue = obj[frame->queue].buffer.buffer;
	auto queue_heads_view = obj[frame->queue_heads_view].image_view.view;
	auto objects = obj[frame->objects].buffer.buffer;

	VkDescriptorImageInfo imageInfo[] = {
		{ .imageView = queue_heads_view,
		  .imageLayout = VK_IMAGE_LAYOUT_GENERAL, },
	};
	VkDescriptorBufferInfo bufferInfo[] = {
		{ .buffer = counts, .offset = 0, .range = VK_WHOLE_SIZE },
		{ .buffer = queue, .offset = 0, .range = VK_WHOLE_SIZE },
		{ .buffer = objects, .offset = 0, .range = VK_WHOLE_SIZE },
	};
	VkWriteDescriptorSet write[] = {
		{   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame->set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &bufferInfo[0], },
		{   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame->set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &bufferInfo[1], },
		{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame->set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &imageInfo[0], },
		{   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame->set,
			.dstBinding = 3,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &bufferInfo[2], },
	};
	dfunc->vkUpdateDescriptorSets (device->dev,
								   countof (write), write,
								   0, nullptr);

	frame->need_update = false;
}

static void
gizmo_shutdown (exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	qfvPushDebug (ctx, "gizmo shutdown");

	auto device = ctx->device;
	if (gctx->resources.array) {
		for (uint32_t j = 0; j < gctx->resources.count; j++) {
			auto resources = &gctx->resources.array[j];
			QFV_DestroyResource (device, resources);
			for (uint32_t i = 0; i < resources->num_objects; i++) {
				auto obj = &resources->objects[i];
				free ((char *) obj->name);
			}
		}

	}
	free (gctx->resources.array);

	qfvPopDebug (ctx);
}

static void
gizmo_setup_buffers (vulkan_ctx_t *ctx, qfv_resource_t *resources,
					 uint32_t set,
					 qfv_resobj_t *counts,
					 qfv_resobj_t *queue,
					 qfv_resobj_t *queue_heads_image,
					 qfv_resobj_t *queue_heads_view,
					 qfv_resobj_t *objects,
					 qfv_resobj_t *obj_ids)
{
	auto  gctx = ctx->gizmo_context;
	size_t frames = gctx->frames.size;

	resources[0] = (qfv_resource_t) {
		.name = nva ("gizmo[%d]", set),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames + 1 + 1 + 1 + frames + frames,
		.objects = counts,
	};
	queue_heads_image[0] = (qfv_resobj_t) {
		.name = nva ("queue_heads_image[%d]", set),
		.type = qfv_res_image,
		.image = {
			.type = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R32_UINT,
			.extent = { 0, 0, 1 },
			.num_mipmaps = 1,
			.num_layers = 6,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
					| VK_IMAGE_USAGE_STORAGE_BIT,
		},
	};
	queue_heads_view[0] = (qfv_resobj_t) {
		.name = nva ("queue_heads_view[%d]", set),
		.type = qfv_res_image_view,
		.image_view = {
			.image = queue_heads_image - resources[0].objects,
			.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = queue_heads_image[0].image.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
		},
	};
	queue[0] = (qfv_resobj_t) {
		.name = nva ("queue[%d]", set),
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (giz_queue_t[MAX_QUEUE_BUFFER]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};

	for (size_t i = 0; i < frames; i++) {
		counts[i] = (qfv_resobj_t) {
			.name = nva ("counts[%zd]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (giz_count_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		objects[i] = (qfv_resobj_t) {
			.name = nva ("objects[%zd]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t[MAX_OBJECT_DATA]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		obj_ids[i] = (qfv_resobj_t) {
			.name = nva ("obj_ids[%zd]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t[MAX_OBJECT_DATA]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
	}
}

static void
gizmo_create_resources (vulkan_ctx_t *ctx)
{
	auto gctx = ctx->gizmo_context;
	auto device = ctx->device;
	size_t frames = gctx->frames.size;

	size_t size = sizeof (qfv_resource_t)
				// queue counts buffer
				+ sizeof (qfv_resobj_t[frames])
				// queue buffer
				+ sizeof (qfv_resobj_t)
				// queue_heads image
				+ sizeof (qfv_resobj_t)
				// queue_heads view
				+ sizeof (qfv_resobj_t)
				// object buffer
				+ sizeof (qfv_resobj_t[frames])
				// obj_id buffer
				+ sizeof (qfv_resobj_t[frames]);
	gctx->resources.array = malloc (size * frames);
	gctx->resources.active = 0;
	gctx->resources.count = frames;
	// all qfv_resource_t elements are at the beginning of the block, followed
	// by all the qfv_resobj_t elements
	void *res = &gctx->resources.array[frames]; // walks through block
	for (uint32_t j = 0; j < frames; j++) {
		auto counts = (qfv_resobj_t *) res;
		auto queue = &counts[frames];
		auto queue_heads_image = &queue[1];
		auto queue_heads_view = &queue_heads_image[1];
		auto objects = &queue_heads_view[1];
		auto obj_ids = &objects[frames];
		res = &obj_ids[frames];

		gizmo_setup_buffers (ctx, &gctx->resources.array[j], j,
							 counts, queue,
							 queue_heads_image, queue_heads_view,
							 objects, obj_ids);

		for (size_t i = 0; i < frames; i++) {
			auto obj = gctx->resources.array[j].objects;
			auto frame = &gctx->frames.a[i];
			*frame = (gizmoframe_t) {
				.counts = &counts[i] - obj,
				.queue_heads_image = &queue_heads_image[0] - obj,
				.queue_heads_view = &queue_heads_view[0] - obj,
				.queue = &queue[0] - obj,
				.objects = &objects[i] - obj,
				.obj_ids = &obj_ids[i] - obj,
				.set = QFV_DSManager_AllocSet (gctx->dsmanager),
			};
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
								 frame->set,
								 vac (ctx->va_ctx, "gizmo[%zd]", i));
		}
	}
}

static void
gizmo_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	qfvPushDebug (ctx, "gizmo startup");

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&gctx->frames, frames);
	DARRAY_RESIZE (&gctx->frames, frames);
	gctx->frames.grow = 0;
	memset (gctx->frames.a, 0, gctx->frames.size * sizeof (gizmoframe_t));

	gctx->dsmanager = QFV_Render_DSManager (ctx, "gizmo_set");
	gizmo_create_resources (ctx);

	qfvPopDebug (ctx);
}

static void
gizmo_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, gizmo_shutdown);
	QFV_Render_AddStartup (ctx, gizmo_startup);

	gizmoctx_t *gctx = malloc (sizeof (gizmoctx_t));
	ctx->gizmo_context = gctx;

	*gctx = (gizmoctx_t) {
		.fs_obj_ids = DARRAY_STATIC_INIT (1024),
		.obj_ids = DARRAY_STATIC_INIT (1024),
		.objects = DARRAY_STATIC_INIT (1024),

		.frag_size = QFV_GetBlackboardVar (ctx, "frag_size"),
		.full_screen = QFV_GetBlackboardVar (ctx, "full_screen"),
	};
}

static void
gizmo_flush (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	auto packet = QFV_PacketAcquire (ctx->staging, "gizmo.flush");
	auto frame = &gctx->frames.a[ctx->curFrame];

	if (frame->need_update) {
		gizmo_update_descriptors (ctx, ctx->curFrame);
	}

	auto resources = &gctx->resources.array[gctx->resources.active];
	auto obj = resources->objects;

	auto counts_buffer = obj[frame->counts].buffer.buffer;
	auto queue_buffer = obj[frame->queue].buffer.buffer;
	auto queue_heads_image = obj[frame->queue_heads_image].image.image;
	auto objects_buffer = obj[frame->objects].buffer.buffer;
	auto obj_ids_buffer = obj[frame->obj_ids].buffer.buffer;

	size_t size = sizeof (giz_count_t)
				+ sizeof (uint32_t[gctx->objects.size])
				+ sizeof (uint32_t[gctx->obj_ids.size])
				+ sizeof (uint32_t[gctx->fs_obj_ids.size]);
	byte *packet_start = QFV_PacketExtend (packet, size);
	byte *packet_data = packet_start;

	qfv_scatter_t counts_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = sizeof (giz_count_t),
	};
	auto counts = (giz_count_t *) packet_data;
	packet_data += sizeof (giz_count_t);
	*counts = (giz_count_t) { .maxObjects = MAX_QUEUE_BUFFER };
	QFV_PacketScatterBuffer (packet, counts_buffer, 1, &counts_scatter,
			&bufferBarriers[qfv_BB_UniformRead_to_TransferWrite],
			&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRW]);

	if (gctx->objects.size) {
		qfv_scatter_t objects_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = 0,
			.length = sizeof (uint32_t[gctx->objects.size]),
		};
		auto objects = (uint32_t *) packet_data;
		packet_data += sizeof (uint32_t[gctx->objects.size]);
		memcpy (objects, gctx->objects.a,
				sizeof (uint32_t[gctx->objects.size]));
		QFV_PacketScatterBuffer (packet, objects_buffer, 1, &objects_scatter,
				&bufferBarriers[qfv_BB_UniformRead_to_TransferWrite],
				&bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	}

	frame->num_obj_ids = gctx->obj_ids.size;
	if (gctx->obj_ids.size) {
		qfv_scatter_t obj_ids_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = 0,
			.length = sizeof (uint32_t[gctx->obj_ids.size]),
		};
		auto obj_ids = (uint32_t *) packet_data;
		packet_data += sizeof (uint32_t[gctx->obj_ids.size]);
		memcpy (obj_ids, gctx->obj_ids.a,
				sizeof (uint32_t[gctx->obj_ids.size]));
		QFV_PacketScatterBuffer (packet, obj_ids_buffer, 1, &obj_ids_scatter,
				&bufferBarriers[qfv_BB_UniformRead_to_TransferWrite],
				&bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	}
	frame->num_fs_obj_ids = gctx->fs_obj_ids.size;
	if (gctx->fs_obj_ids.size) {
		qfv_scatter_t fs_obj_ids_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = sizeof (uint32_t[gctx->obj_ids.size]),
			.length = sizeof (uint32_t[gctx->fs_obj_ids.size]),
		};
		auto fs_obj_ids = (uint32_t *) packet_data;
		packet_data += sizeof (uint32_t[gctx->fs_obj_ids.size]);
		memcpy (fs_obj_ids, gctx->fs_obj_ids.a,
				sizeof (uint32_t[gctx->fs_obj_ids.size]));
		// append fs_obj_ids to obj_ids
		QFV_PacketScatterBuffer (packet, obj_ids_buffer, 1, &fs_obj_ids_scatter,
				&bufferBarriers[qfv_BB_UniformRead_to_TransferWrite],
				&bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	}

	QFV_PacketSubmit (packet);

	auto device = ctx->device;
	auto dfunc = device->funcs;
	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "gizmo.flush"));
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	auto bb = bufferBarriers[qfv_BB_Unknown_to_ShaderWrite];
	auto ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &bb,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &ib,
	};

	bb.buffer = queue_buffer;
	bb.size = VK_WHOLE_SIZE;
	auto image = queue_heads_image;
	ib.image = image;
	ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);

	VkClearColorValue clear_color[] = {
		{ .int32 = {-1, -1, -1, -1} },
	};
	VkImageSubresourceRange ranges[] = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS },
	};
	dfunc->vkCmdClearColorImage (cmd, image,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 clear_color, 1, ranges);
	ib = imageBarriers[qfv_LT_TransferDst_to_StorageAtomic];
	ib.image = image;
	ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dep.bufferMemoryBarrierCount = 0;
	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);

	gctx->objects.size = 0;
	gctx->obj_ids.size = 0;
	gctx->fs_obj_ids.size = 0;
}

static void
gizmo_draw_cmd (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto gctx = ctx->gizmo_context;
	auto frame = &gctx->frames.a[ctx->curFrame];
	auto pipeline = taskctx->pipeline;
	auto layout = pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		frame->set,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, countof(sets), sets, 0, nullptr);

	*gctx->frag_size = (vec2f_t) {
		1.0 / gctx->extent.width,
		1.0 / gctx->extent.height,
	};
	*gctx->full_screen = 0;

	auto resources = &gctx->resources.array[gctx->resources.active];
	auto obj = resources->objects;
	auto obj_ids_buffer = obj[frame->obj_ids].buffer.buffer;

	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &obj_ids_buffer, offsets);
	if (frame->num_obj_ids) {
		*gctx->full_screen = 0;
		QFV_PushBlackboard (ctx, cmd, pipeline);
		dfunc->vkCmdDraw(cmd, 72, frame->num_obj_ids, 0, 0);
	}
	if (frame->num_fs_obj_ids) {
		*gctx->full_screen = 1;
		QFV_PushBlackboard (ctx, cmd, pipeline);
		dfunc->vkCmdDraw(cmd, 3, frame->num_fs_obj_ids, 0, frame->num_obj_ids);
	}
}

static void
gizmo_sync (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto gctx = ctx->gizmo_context;
	auto frame = &gctx->frames.a[ctx->curFrame];

	auto resources = &gctx->resources.array[gctx->resources.active];
	auto obj = resources->objects;
	auto queue_buffer = obj[frame->queue].buffer.buffer;
	auto queue_heads_image = obj[frame->queue_heads_image].image.image;

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "gizmo.sync"));

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	auto bb = bufferBarriers[qfv_BB_ShaderWrite_to_ShaderRO];
	auto ib = imageBarriers[qfv_LT_StorageAtomic_to_StorageReadOnly];
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &bb,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &ib,
	};
	bb.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	bb.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	bb.buffer = queue_buffer;
	bb.size = VK_WHOLE_SIZE;

	ib.image = queue_heads_image;

	dfunc->vkCmdPipelineBarrier2 (cmd, &dep);

	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);
}

static void
gizmo_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	auto frame = &gctx->frames.a[ctx->curFrame];
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		frame->set,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, countof (sets), sets, 0, 0);
	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
}

static void
gizmo_update_framebuffer (const exprval_t **params, exprval_t *result,
		                  exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto job = ctx->render_context->job;
	auto main_step = QFV_GetStep (params[0], job);
	auto gizmo_step = QFV_GetStep (params[1], job);
	auto main_render = main_step->render;
	auto gizmo_render = gizmo_step->render;
	auto rp = gizmo_render->active;
	auto gctx = ctx->gizmo_context;

	auto size = main_render->output.extent;
	qfv_output_t output = (qfv_output_t) {
		.extent = {
			.width = (size.width + 7) / 8,
			.height = (size.height + 7) / 8,
		},
	};
	if ((output.extent.width != gizmo_render->output.extent.width
		|| output.extent.height != gizmo_render->output.extent.height)) {
		QFV_DestroyFramebuffer (ctx, rp);
		gizmo_delete_buffers (ctx);

		gizmo_render->output.extent = output.extent;
		gctx->extent = output.extent;

		gizmo_create_buffers (ctx);
		QFV_UpdateViewportScissor (gizmo_render, &output);
	}

	if (!rp->beginInfo.framebuffer) {
		QFV_CreateFramebuffer (ctx, rp, gizmo_render->output.extent);
	}
}

static exprfunc_t gizmo_flush_func[] = {
	{ .func = gizmo_flush },
	{}
};
static exprfunc_t gizmo_draw_cmd_func[] = {
	{ .func = gizmo_draw_cmd },
	{}
};
static exprfunc_t gizmo_sync_func[] = {
	{ .func = gizmo_sync },
	{}
};
static exprfunc_t gizmo_draw_func[] = {
	{ .func = gizmo_draw },
	{}
};
static exprfunc_t gizmo_init_func[] = {
	{ .func = gizmo_init },
	{}
};

static exprtype_t *gizmo_update_framebuffer_params[] = {
	&cexpr_string,
	&cexpr_string,
};
static exprfunc_t gizmo_update_framebuffer_func[] = {
	{ .func = gizmo_update_framebuffer, .num_params = 2,
		gizmo_update_framebuffer_params },
	{}
};

static exprsym_t gizmo_task_syms[] = {
	{ "gizmo_flush", &cexpr_function, gizmo_flush_func },
	{ "gizmo_draw_cmd", &cexpr_function, gizmo_draw_cmd_func },
	{ "gizmo_sync", &cexpr_function, gizmo_sync_func },
	{ "gizmo_draw", &cexpr_function, gizmo_draw_func },
	{ "gizmo_init", &cexpr_function, gizmo_init_func },
	{ "gizmo_update_framebuffer", &cexpr_function,
		gizmo_update_framebuffer_func },
	{}
};

void
Vulkan_Gizmo_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, gizmo_task_syms);
}

static uint32_t *
gizmo_add_object (void *obj, uint32_t size, uint32_t extra, bool fs,
				  vulkan_ctx_t *ctx)
{
	auto gctx = ctx->gizmo_context;

	uint32_t count = (size) / sizeof (uint32_t) + extra;
	uint32_t obj_id = gctx->objects.size;
	if (obj_id + count > MAX_OBJECT_DATA) {
		return nullptr;
	}
	auto data = DARRAY_OPEN_AT (&gctx->objects, obj_id, count);
	memcpy (data, obj, size);
	if (fs) {
		DARRAY_APPEND (&gctx->fs_obj_ids, obj_id);
	} else {
		DARRAY_APPEND (&gctx->obj_ids, obj_id);
	}
	return data + size / sizeof (uint32_t);
}

void
Vulkan_Gizmo_AddSphere (vec4f_t c, float r, const quat_t color,
						vulkan_ctx_t *ctx)
{
	giz_sphere_t sphere = {
		.cmd = 0,
		.c = { VectorExpand (c) },
		.r = r,
	};
	QuatScale (color, 255, sphere.col);
	gizmo_add_object (&sphere, sizeof (sphere), 0, false, ctx);
}

void
Vulkan_Gizmo_AddCapsule (vec4f_t p1, vec4f_t p2, float r, const quat_t color,
						 vulkan_ctx_t *ctx)
{
	giz_capsule_t capsule = {
		.cmd = 1,
		.p1 = { VectorExpand (p1) },
		.p2 = { VectorExpand (p2) },
		.r = r,
	};
	QuatScale (color, 255, capsule.col);
	gizmo_add_object (&capsule, sizeof (capsule), 0, false, ctx);
}

void
Vulkan_Gizmo_AddBrush (vec4f_t orig, const vec4f_t bounds[2],
					   int num_nodes, const gizmo_node_t *nodes,
					   quat_t color, vulkan_ctx_t *ctx)
{
	giz_brush_t brush = {
		.cmd = 2,
		.orig = { VectorExpand (orig) },
		.mins = { VectorExpand (bounds[0]) },
		.maxs = { VectorExpand (bounds[1]) },
	};
	QuatScale (color, 255, brush.col);
	auto p = gizmo_add_object (&brush, sizeof (brush), 5 * num_nodes,
							   false, ctx);
	if (!p) {
		return;
	}
	for (int i = 0; i < num_nodes; i++) {
		for (int j = 0; j < 4; j++) {
			*(float *)(p++) = nodes[i].plane[j];
		}
		uint32_t front = nodes[i].children[0] & 0xff;
		uint32_t back = nodes[i].children[1] & 0xff;
		uint32_t children = front | (back << 8);
		*p++ = children;
	}
}

void
Vulkan_Gizmo_AddPlane (vec4f_t p, vec4f_t s, vec4f_t t,
					   quat_t gcol, quat_t scol, quat_t tcol,
					   vulkan_ctx_t *ctx)
{
	vec4f_t n = crossf (s, t);
	vec4f_t d = p - loadxyzf (r_refdef.camera[3]);
	vec4f_t S = dotf (t, t) * s - dotf (s, t) * t;
	vec4f_t T = dotf (s, s) * t - dotf (s, t) * s;
	vec4f_t dn = dotf (d, n);
	vec4f_t Sd = dotf (S, d);
	vec4f_t Td = dotf (T, d);
	vec4f_t SS = dn * S - Sd * n;
	vec4f_t TT = dn * T - Td * n;
	vec4f_t nn = dotf (n, n) * n;

	giz_plane_t plane = {
		.cmd = 3,
		.p = {
			{SS[0], TT[0], nn[0], dn[0]},
			{SS[1], TT[1], nn[1], 0},
			{SS[2], TT[2], nn[2], 0},
		},
	};
	QuatScale (gcol, 255, plane.gcol);
	QuatScale (scol, 255, plane.scol);
	QuatScale (tcol, 255, plane.tcol);
	gizmo_add_object (&plane, sizeof (plane), 0, true, ctx);
}
