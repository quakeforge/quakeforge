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

typedef struct giz_count_s {
	uint32_t    numObjects;
	uint32_t    maxObjects;
} giz_count_t;

typedef struct giz_queue_s {
	uint32_t    obj_id;
	uint32_t    next;
} giz_queue_t;

typedef struct giz_sphere_s {
	uint32_t    cmd;
	float       c[3];
	float       r;
	byte        col[4];
} giz_sphere_t;

typedef struct gizmoframe_s {
	VkBuffer    counts;
	VkBuffer    queue;
	VkImageView queue_heads_view;
	VkBuffer    objects;
	VkBuffer    obj_ids;
	uint32_t    num_obj_ids;

	VkImage     queue_heads_image;
	VkDescriptorSet set;
} gizmoframe_t;

typedef struct gizmoframeset_s
	DARRAY_TYPE (gizmoframe_t) gizmoframeset_t;

typedef struct giz_data_s
	DARRAY_TYPE (uint32_t) giz_data_t;

typedef struct gizmoctx_s {
	qfv_dsmanager_t *dsmanager;
	qfv_resource_t *resource;
	gizmoframeset_t frames;
	uint32_t    cmd_width;
	uint32_t    cmd_height;
	giz_data_t  obj_ids;
	giz_data_t  objects;
} gizmoctx_t;

static void
gizmo_shutdown (exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	qfvPushDebug (ctx, "gizmo shutdown");

	auto device = ctx->device;
	QFV_DestroyResource (device, gctx->resource);

	qfvPopDebug (ctx);
}

static void
gizmo_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	qfvPushDebug (ctx, "gizmo startup");

	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&gctx->frames, frames);
	DARRAY_RESIZE (&gctx->frames, frames);
	gctx->frames.grow = 0;
	memset (gctx->frames.a, 0, gctx->frames.size * sizeof (gizmoframe_t));

	gctx->dsmanager = QFV_Render_DSManager (ctx, "gizmo_set");

	gctx->resource = malloc (sizeof (qfv_resource_t)
							 // queue counts buffer
							 + frames * sizeof (qfv_resobj_t)
							 // queue buffer
							 + sizeof (qfv_resobj_t)
							 // queue_heads image
							 + sizeof (qfv_resobj_t)
							 // queue_heads view
							 + sizeof (qfv_resobj_t)
							 // object buffer
							 + frames * sizeof (qfv_resobj_t)
							 // obj_id buffer
							 + frames * sizeof (qfv_resobj_t));
	auto counts = (qfv_resobj_t *) &gctx->resource[1];
	auto queue = &counts[frames];
	auto queue_heads_image = &queue[1];
	auto queue_heads_view = &queue_heads_image[1];
	auto objects = &queue_heads_view[1];
	auto obj_ids = &objects[frames];
	*gctx->resource = (qfv_resource_t) {
		.name = "gizmo",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames + 1 + 1 + 1 + frames + frames,
		.objects = counts,
	};
	queue_heads_image[0] = (qfv_resobj_t) {
		.name = "queue_heads_image",
		.type = qfv_res_image,
		.image = {
			.type = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R32_UINT,
			.extent = {
				.width = gctx->cmd_width,
				.height = gctx->cmd_height,
				.depth = 1,
			},
			.num_mipmaps = 1,
			.num_layers = 6,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
					| VK_IMAGE_USAGE_STORAGE_BIT,
		},
	};
	queue_heads_view[0] = (qfv_resobj_t) {
		.name = "queue_heads_view",
		.type = qfv_res_image_view,
		.image_view = {
			.image = queue_heads_image - gctx->resource[0].objects,
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
		.name = "queue",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (giz_queue_t[MAX_QUEUE_BUFFER]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};
	for (uint32_t i = 0; i < frames; i++) {
		counts[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "counts[%d]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (giz_count_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		objects[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "objects[%d]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t[MAX_OBJECT_DATA]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
		obj_ids[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "obj_ids[%d]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t[MAX_OBJECT_DATA]),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, gctx->resource);
	for (uint32_t i = 0; i < frames; i++) {
		auto frame = &gctx->frames.a[i];
		*frame = (gizmoframe_t) {
			.counts = counts[i].buffer.buffer,
			.queue_heads_image = queue_heads_image[0].image.image,
			.queue_heads_view = queue_heads_view[0].image_view.view,
			.queue = queue[0].buffer.buffer,
			.objects = objects[i].buffer.buffer,
			.obj_ids = obj_ids[i].buffer.buffer,
			.set = QFV_DSManager_AllocSet (gctx->dsmanager),
		};
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 frame->set, vac (ctx->va_ctx, "gizmo[%d]", i));

		VkDescriptorImageInfo imageInfo[] = {
			{ .imageView = frame->queue_heads_view,
			  .imageLayout = VK_IMAGE_LAYOUT_GENERAL, },
		};
		VkDescriptorBufferInfo bufferInfo[] = {
			{ .buffer = frame->counts, .offset = 0, .range = VK_WHOLE_SIZE },
			{ .buffer = frame->queue, .offset = 0, .range = VK_WHOLE_SIZE },
			{ .buffer = frame->objects, .offset = 0, .range = VK_WHOLE_SIZE },
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
	}

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
		.cmd_width = 240,//FIXME
		.cmd_height = 135,//FIXME
		.obj_ids = DARRAY_STATIC_INIT (1024),
		.objects = DARRAY_STATIC_INIT (1024),
	};
}

static void
gizmo_flush (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto gctx = ctx->gizmo_context;
	auto packet = QFV_PacketAcquire (ctx->staging);
	auto frame = &gctx->frames.a[ctx->curFrame];

	auto sb = &bufferBarriers[qfv_BB_UniformRead_to_TransferWrite];
	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];

	size_t size = sizeof (giz_count_t)
				+ sizeof (uint32_t[gctx->objects.size])
				+ sizeof (uint32_t[gctx->obj_ids.size]);
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
	QFV_PacketScatterBuffer (packet, frame->counts, 1, &counts_scatter, sb, bb);

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
		QFV_PacketScatterBuffer (packet, frame->objects, 1, &objects_scatter,
								 sb, bb);
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
		QFV_PacketScatterBuffer (packet, frame->obj_ids, 1, &obj_ids_scatter,
								 sb, bb);
	}

	QFV_PacketSubmit (packet);

	auto device = ctx->device;
	auto dfunc = device->funcs;
	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	auto image = frame->queue_heads_image;
	auto ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
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

	gctx->objects.size = 0;
	gctx->obj_ids.size = 0;
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
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		frame->set,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, countof(sets), sets, 0, nullptr);

	vec2f_t frag_size = { 1.0 / gctx->cmd_width, 1.0 / gctx->cmd_height };
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (vec2f_t), &frag_size },
	};
	QFV_PushConstants (device, cmd, layout,
					   countof (push_constants), push_constants);

	VkDeviceSize offsets[] = {0};
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &frame->obj_ids, offsets);
	dfunc->vkCmdDraw(cmd, 72, frame->num_obj_ids, 0, 0);
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
		QFV_UpdateViewportScissor (gizmo_render, &output);
		gizmo_render->output.extent = output.extent;
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

static void
gizmo_add_object (void *obj, uint32_t size, vulkan_ctx_t *ctx)
{
	auto gctx = ctx->gizmo_context;

	uint32_t count = size / sizeof (uint32_t);
	uint32_t obj_id = gctx->objects.size;
	if (obj_id + count > MAX_OBJECT_DATA) {
		return;
	}
	auto data = DARRAY_OPEN_AT (&gctx->objects, obj_id, count);
	memcpy (data, obj, size);
	DARRAY_APPEND (&gctx->obj_ids, obj_id);
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
	gizmo_add_object (&sphere, sizeof (sphere), ctx);
}
