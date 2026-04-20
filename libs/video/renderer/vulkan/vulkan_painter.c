/*
	vulkan_painter.c

	2D drawing based on Guerilla's UIPainter

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
#include "QF/Vulkan/qf_painter.h"

#include "r_internal.h"
#include "vid_vulkan.h"

#define MAX_QUEUE_BUFFER (8*1024*1024)
#define MAX_OBJECT_DATA (8*1024*1024)

static void
painter_delete_buffers (vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;

	auto resources = &pctx->resources.array[pctx->resources.active];
	if (resources->memory) {
		QFV_QueueResourceDelete (ctx, resources);
		qfv_resourcearray_next (&pctx->resources);
	}
	free (pctx->cmd_heads);
	pctx->cmd_heads = nullptr;
	pctx->cmd_tails = nullptr;
}

static void
painter_create_buffers (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto pctx = ctx->painter_context;
	size_t frames = pctx->frames.size;

	auto resources = &pctx->resources.array[pctx->resources.active];

	if (resources->memory) {
		Sys_Error ("resources already allocated!");
	}

	for (uint32_t i = 0; i < resources->num_objects; i++) {
		auto obj = &resources->objects[i];
		if (obj->type == qfv_res_image) {
			auto img = &obj->image;
			img->extent.width = pctx->cmd_extent.width;
			img->extent.height = pctx->cmd_extent.height;
		}
	}
	QFV_CreateResource (device, resources);

	for (size_t i = 0; i < frames; i++) {
		pctx->frames.a[i].need_update = true;
	}

	pctx->max_queues = pctx->cmd_extent.width * pctx->cmd_extent.height;
	pctx->cmd_heads = malloc (sizeof(uint32_t[pctx->max_queues * 2]));
	pctx->cmd_tails = &pctx->cmd_heads[pctx->max_queues];
	// clear both heads and tails
	memset (pctx->cmd_heads, 0xff, sizeof(uint32_t[pctx->max_queues * 2]));
}

static void
painter_update_descriptors (vulkan_ctx_t *ctx, int i)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->painter_context;

	auto resources = &pctx->resources.array[pctx->resources.active];
	auto obj = resources->objects;

	auto frame = &pctx->frames.a[i];
	auto cmd_heads_view = obj[frame->cmd_heads_view].image_view.view;
	auto cmd_queue = obj[frame->cmd_queue].buffer.buffer;

	VkDescriptorImageInfo imageInfo[] = {
		{ .imageView = cmd_heads_view,
		  .imageLayout = VK_IMAGE_LAYOUT_GENERAL, },
	};
	VkDescriptorBufferInfo bufferInfo[] = {
		{ .buffer = cmd_queue, .offset = 0, .range = VK_WHOLE_SIZE },
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

static void
painter_shutdown (exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto pctx = ctx->painter_context;
	qfvPushDebug (ctx, "painter shutdown");

	auto device = ctx->device;
	if (pctx->resources.array) {
		for (uint32_t j = 0; j < pctx->resources.count; j++) {
			auto resources = &pctx->resources.array[j];
			QFV_DestroyResource (device, resources);
			for (uint32_t i = 0; i < resources->num_objects; i++) {
				auto obj = &resources->objects[i];
				free ((char *) obj->name);
			}
		}
	}
	free (pctx->resources.array);

	qfvPopDebug (ctx);
}

static void
painter_setup_buffers (vulkan_ctx_t *ctx, qfv_resource_t *resources,
					   uint32_t set,
					   qfv_resobj_t *cmd_heads_image,
					   qfv_resobj_t *cmd_heads_view,
					   qfv_resobj_t *cmd_queue)
{
	auto  pctx = ctx->painter_context;
	size_t frames = pctx->frames.size;

	resources[0] = (qfv_resource_t) {
		.name = nva ("painter[%d]", set),
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames + frames + frames,
		.objects = cmd_heads_image,
	};
	for (uint32_t i = 0; i < frames; i++) {
		cmd_heads_image[i] = (qfv_resobj_t) {
			.name = nva ("cmd_heads_image[%d]", i),
			.type = qfv_res_image,
			.image = {
				.type = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32_UINT,
				.extent = {
					.width = pctx->cmd_extent.width,
					.height = pctx->cmd_extent.height,
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
			.name = nva ("cmd_heads_view[%d]", i),
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
			.name = nva ("cmd_queue[%d]", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (uint32_t) * MAX_QUEUE_BUFFER,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
	}
}

static void
painter_create_resources (vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	auto device = ctx->device;
	size_t frames = pctx->frames.size;

	size_t size = sizeof (qfv_resource_t)
				// cmd_heads image
				+ frames * sizeof (qfv_resobj_t)
				// cmd_heads view
				+ frames * sizeof (qfv_resobj_t)
				// cmd_queue buffer
				+ frames * sizeof (qfv_resobj_t);
	pctx->resources = (qfv_resourcearray_t) {
		//FIXME I'm not sure why 2x the frame count is needed
		.array = malloc (size * frames * 2),
		.count = frames * 2,
	};
	// all qfv_resource_t elements are at the beginning of the block, followed
	// by all the qfv_resobj_t elements
	// walks through block
	void *res = &pctx->resources.array[pctx->resources.count];
	for (uint32_t j = 0; j < pctx->resources.count; j++) {
		auto cmd_heads_image = (qfv_resobj_t *) res;
		auto cmd_heads_view = &cmd_heads_image[frames];
		auto cmd_queue = &cmd_heads_view[frames];
		res = &cmd_queue[frames];

		painter_setup_buffers (ctx, &pctx->resources.array[j], j,
							   cmd_heads_image, cmd_heads_view, cmd_queue);

		for (uint32_t i = 0; i < frames; i++) {
			auto obj = pctx->resources.array[j].objects;
			auto frame = &pctx->frames.a[i];
			*frame = (painterframe_t) {
				.cmd_heads_image = &cmd_heads_image[i] - obj,
				.cmd_heads_view = &cmd_heads_view[i] - obj,
				.cmd_queue = &cmd_queue[i] - obj,
				.set = QFV_DSManager_AllocSet (pctx->dsmanager),
			};
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
								 frame->set,
								 vac (ctx->va_ctx, "painter[%d]", i));
		}
	}
}

static void
painter_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto pctx = ctx->painter_context;
	qfvPushDebug (ctx, "painter startup");

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&pctx->frames, frames);
	DARRAY_RESIZE (&pctx->frames, frames);
	pctx->frames.grow = 0;
	memset (pctx->frames.a, 0, pctx->frames.size * sizeof (painterframe_t));

	pctx->dsmanager = QFV_Render_DSManager (ctx, "painter_set");
	painter_create_resources (ctx);

	qfvPopDebug (ctx);
}

static void
painter_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, painter_shutdown);
	QFV_Render_AddStartup (ctx, painter_startup);

	painterctx_t *pctx = malloc (sizeof (painterctx_t));
	ctx->painter_context = pctx;

	*pctx = (painterctx_t) {
		.cmd_extent = {
			.depth = 1,
			.layers =1,
		},
		.cmd_queue = DARRAY_STATIC_INIT (1024),
	};
}

static void
painter_flush (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto pctx = ctx->painter_context;
	auto packet = QFV_PacketAcquire (ctx->staging, "painter.flush1");
	auto frame = &pctx->frames.a[ctx->curFrame];

	if (frame->need_update) {
		painter_update_descriptors (ctx, ctx->curFrame);
	}

	auto resources = &pctx->resources.array[pctx->resources.active];
	auto obj = resources->objects;

	auto cmd_heads_image = obj[frame->cmd_heads_image].image.image;
	auto cmd_queue = obj[frame->cmd_queue].buffer.buffer;

	size_t heads_size = sizeof(uint32_t[pctx->max_queues]);
	auto data = QFV_PacketExtend (packet, heads_size);
	memcpy (data, pctx->cmd_heads, heads_size);
	QFV_PacketCopyImage (packet, cmd_heads_image,
						 (qfv_offset_t){}, pctx->cmd_extent, 0,
						 &imageBarriers[qfv_LT_Undefined_to_TransferDst],
						 &imageBarriers[qfv_LT_TransferDst_to_StorageReadOnly]);
	QFV_PacketSubmit (packet);

	if (pctx->cmd_queue.size) {
		packet = QFV_PacketAcquire (ctx->staging, "painter.flush2");
		size_t size = pctx->cmd_queue.size * sizeof (uint32_t);
		data = QFV_PacketExtend (packet, size);
		memcpy (data, pctx->cmd_queue.a, size);
		auto sb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
		auto db = bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
		QFV_PacketCopyBuffer (packet, cmd_queue, 0, &sb, &db);
		QFV_PacketSubmit (packet);
	}
	// clear both heads and tails
	memset (pctx->cmd_heads, 0xff, heads_size * 2);
	pctx->cmd_queue.size = 0;
}

static void
painter_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto pctx = ctx->painter_context;
	auto frame = &pctx->frames.a[ctx->curFrame];
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		frame->set,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 1, sets, 0, 0);
	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
}

static void
painter_update_framebuffer (const exprval_t **params, exprval_t *result,
							exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto job = ctx->render_context->job;
	auto output_step = QFV_GetStep (params[0], job);
	auto output_render = output_step->render;
	auto pctx = ctx->painter_context;

	auto size = output_render->output.extent;
	qfv_output_t output = (qfv_output_t) {
		.extent = {
			.width = (size.width + 7) / 8,
			.height = (size.height + 7) / 8,
		},
	};
	if ((output.extent.width != pctx->cmd_extent.width
		|| output.extent.height != pctx->cmd_extent.height)) {
		painter_delete_buffers (ctx);

		pctx->cmd_extent.width = output.extent.width;
		pctx->cmd_extent.height = output.extent.height;

		painter_create_buffers (ctx);
	}
}


static exprfunc_t painter_flush_func[] = {
	{ .func = painter_flush },
	{}
};
static exprfunc_t painter_draw_func[] = {
	{ .func = painter_draw },
	{}
};
static exprfunc_t painter_init_func[] = {
	{ .func = painter_init },
	{}
};

static exprtype_t *painter_update_framebuffer_params[] = {
	&cexpr_string,
};

static exprfunc_t painter_update_framebuffer_func[] = {
	{ .func = painter_update_framebuffer, .num_params = 1,
		painter_update_framebuffer_params },
	{}
};

static exprsym_t painter_task_syms[] = {
	{ "painter_flush", &cexpr_function, painter_flush_func },
	{ "painter_draw", &cexpr_function, painter_draw_func },
	{ "painter_init", &cexpr_function, painter_init_func },
	{ "painter_update_framebuffer", &cexpr_function,
		painter_update_framebuffer_func },
	{}
};

void
Vulkan_Painter_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, painter_task_syms);
}

static void
painter_queue_cmd (int x, int y, uip_cmd_t *cmd, uint32_t size,
				   painterctx_t *pctx)
{
	uint32_t    queue_ind = y * pctx->cmd_extent.width + x;
	auto head_ptr = &pctx->cmd_heads[queue_ind];
	auto tail_ptr = &pctx->cmd_tails[queue_ind];
	uint32_t    tail = *tail_ptr;
	uint32_t    ind = pctx->cmd_queue.size;
	uint32_t    count = size / sizeof (uint32_t);
	auto queued_cmd = (uip_cmd_t *) DARRAY_OPEN_AT (&pctx->cmd_queue, ind,
													count);
	if (*head_ptr == ~0u) {
		*head_ptr = ind;
	}
	memcpy (queued_cmd, cmd, size);
	if (tail != ~0u) {
		pctx->cmd_queue.a[tail] = ind;
	}
	*tail_ptr = &queued_cmd->next - pctx->cmd_queue.a;
}

static void
painter_add_command (vec2f_t tl, vec2f_t br, uip_cmd_t *cmd, uint32_t size,
					 painterctx_t *pctx)
{
	int x1 = (tl[0] - 1) / 8;
	int y1 = (tl[1] - 1) / 8;
	int x2 = (br[0] + 1) / 8 + 1;
	int y2 = (br[1] + 1) / 8 + 1;

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 > (int) pctx->cmd_extent.width) x2 = pctx->cmd_extent.width;
	if (y2 > (int) pctx->cmd_extent.height) y2 = pctx->cmd_extent.height;
	for (int y = y1; y < y2; y++) {
		for (int x = x1; x < x2; x++) {
			painter_queue_cmd (x, y, cmd, size, pctx);
		}
	}
}

void
Vulkan_Painter_AddLine (vec2f_t p1, vec2f_t p2, float r, const quat_t color,
						vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	uip_line_t line = {
		.cmd = {
			.cmd = 0,
			.next = ~0u,
		},
		.a = { VEC2_EXP (p1) },
		.b = { VEC2_EXP (p2) },
		.r = r,
	};
	QuatScale (color, 255, line.col);
	if (p1[0] > p2[0]) {
		float t = p1[0];
		p1[0] = p2[0];
		p2[0] = t;
	}
	if (p1[1] > p2[1]) {
		float t = p1[1];
		p1[1] = p2[1];
		p2[1] = t;
	}
	//FIXME use modified xiaolin wu's line algorithm
	painter_add_command (p1 - r, p2 + r, &line.cmd, sizeof (line), pctx);
}

void
Vulkan_Painter_AddCircle (vec2f_t c, float r, const quat_t color,
						  vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	uip_circle_t circle = {
		.cmd = {
			.cmd = 1,
			.next = ~0u,
		},
		.c = { VEC2_EXP (c) },
		.r = r,
	};
	QuatScale (color, 255, circle.col);
	//FIXME use filled circle?
	painter_add_command (c - r, c + r, &circle.cmd, sizeof (circle), pctx);
}

void
Vulkan_Painter_AddBox (vec2f_t c, vec2f_t e, float r, const quat_t color,
					   vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	uip_box_t box = {
		.cmd = {
			.cmd = 2,
			.next = ~0u,
		},
		.c = { VEC2_EXP (c) },
		.e = { VEC2_EXP (e) },
		.r = r,
	};
	QuatScale (color, 255, box.col);
	painter_add_command (c - e - r, c + e + r, &box.cmd, sizeof (box), pctx);
}
