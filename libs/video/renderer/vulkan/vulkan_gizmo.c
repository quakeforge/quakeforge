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

typedef struct giz_cmd_s {
	uint32_t    cmd;
	uint32_t    next;
} giz_cmd_t;

typedef struct giz_sphere_s {
	giz_cmd_t   cmd;
	float       c[3];
	float       r;
	byte        col[4];
} giz_sphere_t;

typedef struct gizmoframe_s {
	VkImage     cmd_heads_image;
	VkImageView cmd_heads_view;
	VkBuffer    cmd_queue;
	VkDescriptorSet set;
} gizmoframe_t;

typedef struct gizmoframeset_s
	DARRAY_TYPE (gizmoframe_t) gizmoframeset_t;

typedef struct giz_queue_s
	DARRAY_TYPE (uint32_t) giz_queue_t;

typedef struct gizmoctx_s {
	qfv_dsmanager_t *dsmanager;
	qfv_resource_t *resource;
	gizmoframeset_t frames;
	uint32_t    cmd_width;
	uint32_t    cmd_height;
	uint32_t    max_queues;
	uint32_t   *cmd_heads;
	uint32_t   *cmd_tails;
	giz_queue_t cmd_queue;
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
							 // cmd_heads image
							 + frames * sizeof (qfv_resobj_t)
							 // cmd_heads view
							 + frames * sizeof (qfv_resobj_t)
							 // cmd_queue buffer
							 + frames * sizeof (qfv_resobj_t));
	auto cmd_heads_image = (qfv_resobj_t *) &gctx->resource[1];
	auto cmd_heads_view = &cmd_heads_image[frames];
	auto cmd_queue = &cmd_heads_view[frames];
	*gctx->resource = (qfv_resource_t) {
		.name = "gizmo",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames + frames + frames,
		.objects = cmd_heads_image,
	};
	for (uint32_t i = 0; i < frames; i++) {
		cmd_heads_image[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "cmd_heads_image[%d]", i),
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
				.num_layers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
						| VK_IMAGE_USAGE_STORAGE_BIT,
			},
		};
		cmd_heads_view[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "cmd_heads_view[%d]", i),
			.type = qfv_res_image_view,
			.image_view = {
				.image = i,
				.type = VK_IMAGE_VIEW_TYPE_2D,
				.format = cmd_heads_image[i].image.format,
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
		cmd_queue[i] = (qfv_resobj_t) {
			.name = vac (ctx->va_ctx, "cmd_queue[%d]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t) * MAX_QUEUE_BUFFER,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, gctx->resource);
	for (uint32_t i = 0; i < frames; i++) {
		auto frame = &gctx->frames.a[i];
		*frame = (gizmoframe_t) {
			.cmd_heads_image = cmd_heads_image[i].image.image,
			.cmd_heads_view = cmd_heads_view[i].image_view.view,
			.cmd_queue = cmd_queue[i].buffer.buffer,
			.set = QFV_DSManager_AllocSet (gctx->dsmanager),
		};

		VkDescriptorImageInfo imageInfo[] = {
			{ .imageView = frame->cmd_heads_view,
			  .imageLayout = VK_IMAGE_LAYOUT_GENERAL, },
		};
		VkDescriptorBufferInfo bufferInfo[] = {
			{ .buffer = frame->cmd_queue, .offset = 0, .range = VK_WHOLE_SIZE },
		};
		VkWriteDescriptorSet write[] = {
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frame->set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &imageInfo[0], },
			{   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frame->set,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[0], },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);
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
		.max_queues = 32400,//FIXME
		.cmd_queue = DARRAY_STATIC_INIT (1024),
	};

	gctx->cmd_heads = malloc (gctx->max_queues * 2 * sizeof (uint32_t));
	gctx->cmd_tails = &gctx->cmd_heads[gctx->max_queues];
	memset (gctx->cmd_heads, 0xff, gctx->max_queues * 2 * sizeof (uint32_t));
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

	uint32_t size = gctx->cmd_width * gctx->cmd_height * sizeof (uint32_t);
	auto data = QFV_PacketExtend (packet, size);
	memcpy (data, gctx->cmd_heads, size);
	QFV_PacketCopyImage (packet, frame->cmd_heads_image,
						 gctx->cmd_width, gctx->cmd_height,
						 &imageBarriers[qfv_LT_Undefined_to_TransferDst],
						 &imageBarriers[qfv_LT_TransferDst_to_General]);
	QFV_PacketSubmit (packet);

	if (gctx->cmd_queue.size) {
		packet = QFV_PacketAcquire (ctx->staging);
		size = gctx->cmd_queue.size * sizeof (uint32_t);
		data = QFV_PacketExtend (packet, size);
		memcpy (data, gctx->cmd_queue.a, size);
		auto sb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
		auto db = bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
		QFV_PacketCopyBuffer (packet, frame->cmd_queue, 0, &sb, &db);
		QFV_PacketSubmit (packet);
	}
	memset (gctx->cmd_heads, 0xff, gctx->max_queues * 2 * sizeof (uint32_t));
	gctx->cmd_queue.size = 0;
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

static exprfunc_t gizmo_flush_func[] = {
	{ .func = gizmo_flush },
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

static exprsym_t gizmo_task_syms[] = {
	{ "gizmo_flush", &cexpr_function, gizmo_flush_func },
	{ "gizmo_draw", &cexpr_function, gizmo_draw_func },
	{ "gizmo_init", &cexpr_function, gizmo_init_func },
	{}
};

void
Vulkan_Gizmo_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, gizmo_task_syms);
}

static void
gizmo_queue_cmd (int x, int y, giz_cmd_t *cmd, uint32_t size,
				   gizmoctx_t *gctx)
{
	uint32_t    queue_ind = y * gctx->cmd_width + x;
	auto head_ptr = &gctx->cmd_heads[queue_ind];
	auto tail_ptr = &gctx->cmd_tails[queue_ind];
	uint32_t    tail = *tail_ptr;
	uint32_t    ind = gctx->cmd_queue.size;
	uint32_t    count = size / sizeof (uint32_t);
	auto queued_cmd = (giz_cmd_t *) DARRAY_OPEN_AT (&gctx->cmd_queue, ind,
													count);
	if (*head_ptr == ~0u) {
		*head_ptr = ind;
	}
	memcpy (queued_cmd, cmd, size);
	if (tail != ~0u) {
		gctx->cmd_queue.a[tail] = ind;
	}
	*tail_ptr = &queued_cmd->next - gctx->cmd_queue.a;
}

static void
gizmo_add_command (vec4f_t tl, vec4f_t br, giz_cmd_t *cmd, uint32_t size,
				   vulkan_ctx_t *ctx)
{
	auto gctx = ctx->gizmo_context;
	auto mctx = ctx->matrix_context;
	auto proj = mctx->matrices.Projection3d;

	tl[0] = (1 + tl[0] * proj[0][0] / tl[2]) * 0.5 * r_refdef.vrect.width;
	tl[1] = (1 + tl[1] * proj[1][1] / tl[2]) * 0.5 * r_refdef.vrect.height;
	br[0] = (1 + br[0] * proj[0][0] / tl[2]) * 0.5 * r_refdef.vrect.width;
	br[1] = (1 + br[1] * proj[1][1] / tl[2]) * 0.5 * r_refdef.vrect.height;

	int x1 = (tl[0] - 1) / 8;
	int y1 = (tl[1] - 1) / 8;
	int x2 = (br[0] + 1) / 8 + 1;
	int y2 = (br[1] + 1) / 8 + 1;

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 > (int) gctx->cmd_width) x2 = gctx->cmd_width;
	if (y2 > (int) gctx->cmd_height) y2 = gctx->cmd_height;
	for (int y = y1; y < y2; y++) {
		for (int x = x1; x < x2; x++) {
			gizmo_queue_cmd (x, y, cmd, size, gctx);
		}
	}
}

void
Vulkan_Gizmo_AddSphere (vec4f_t c, float r, const quat_t color,
						vulkan_ctx_t *ctx)
{
	giz_sphere_t sphere = {
		.cmd = {
			.cmd = 0,
			.next = ~0u,
		},
		.c = { VectorExpand (c) },
		.r = r,
	};
	QuatScale (color, 255, sphere.col);
	gizmo_add_command (c - 2*r, c + 2*r, &sphere.cmd, sizeof (sphere), ctx);
}
