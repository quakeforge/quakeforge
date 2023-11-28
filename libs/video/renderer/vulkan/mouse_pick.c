/*
	mouse_pick.c

	Vulkan frame mouse picking support

	Copyright (C) 2023      Bill Currie <bill@taniwha.org>

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

#include "QF/cexpr.h"
#include "QF/va.h"

#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/mouse_pick.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"

#include "QF/plugin/vid_render.h"
#include "vid_vulkan.h"

static void
mousepick_initiate (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto mpctx = ctx->mousepick_context;
	auto frame = &mpctx->frames.a[ctx->curFrame];

	if (!frame->callback) {
		return;
	}

	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	dfunc->vkBeginCommandBuffer (cmd, &(VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	dfunc->vkCmdCopyImageToBuffer (cmd, frame->entid_image,
								   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								   frame->entid_buffer, 1,
								   &(VkBufferImageCopy) {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageOffset = frame->offset,
		.imageExtent = frame->extent,
	});
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);

	frame->initiated = true;
}

static void
mousepick_finalize (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto mpctx = ctx->mousepick_context;
	auto frame = &mpctx->frames.a[ctx->curFrame];

	if (!frame->callback || !frame->initiated) {
			return;
	}

	auto device = ctx->device;
	auto dfunc = device->funcs;

	dfunc->vkInvalidateMappedMemoryRanges (device->dev, 1,
		&(VkMappedMemoryRange) {
			.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			.memory = mpctx->resources->memory,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		});

	uint32_t entids[mousepick_size * mousepick_size];
	memset (entids, 0xff, sizeof (entids));
	if (frame->extent.width > mousepick_size
		|| frame->extent.height > mousepick_size) {
		Sys_Error ("mousepick_finalize: invalid extent: %d %d\n",
				   frame->extent.width, frame->extent.height);
	}
	for (uint32_t j = 0; j < frame->extent.height; j++) {
		uint32_t y = j + 2 - (frame->y - frame->offset.y);
		uint32_t srcInd = j * frame->extent.width;
		uint32_t dstInd = y * mousepick_size;
		for (uint32_t i = 0; i < frame->extent.width; i++) {
			uint32_t x = i + 2 - (frame->x - frame->offset.x);
			entids[dstInd + x] = frame->entid_data[srcInd + i];
		}
	}
#if 0
	for (uint32_t j = 0; j < mousepick_size; j++) {
		for (uint32_t i = 0; i < mousepick_size; i++) {
			printf ("%03x ", entids[j * mousepick_size + i] & 0xfff);
		}
		puts ("");
	}
	puts ("");
#endif
	frame->callback (entids, frame->callback_data);
	frame->callback = 0;
	frame->callback_data = 0;
	frame->initiated = false;
}

static exprfunc_t mousepick_initiate_func[] = {
	{ .func = mousepick_initiate },
	{}
};
static exprfunc_t mousepick_finalize_func[] = {
	{ .func = mousepick_finalize },
	{}
};
static exprsym_t mousepick_task_syms[] = {
	{ "mousepick_initiate", &cexpr_function, mousepick_initiate_func },
	{ "mousepick_finalize", &cexpr_function, mousepick_finalize_func },
	{}
};

void
QFV_MousePick_Init (vulkan_ctx_t *ctx)
{
	QFV_Render_AddTasks (ctx, mousepick_task_syms);

	qfvPushDebug (ctx, "mouse pick init");

	auto device = ctx->device;
	auto dfunc = device->funcs;

	ctx->mousepick_context = calloc (1, sizeof (qfv_mousepickctx_t));
	auto mpctx = ctx->mousepick_context;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&mpctx->frames, frames);
	DARRAY_RESIZE (&mpctx->frames, frames);
	mpctx->frames.grow = 0;

	mpctx->resources = calloc (1, sizeof (qfv_resource_t)
							 + sizeof (qfv_resobj_t[frames]));
	auto buffers = (qfv_resobj_t *) &mpctx->resources[1];
	mpctx->resources[0] = (qfv_resource_t) {
		.name = "mousepick",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = frames,
		.objects = buffers,
	};
	for (size_t i = 0; i < frames; i++) {
		buffers[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "entids:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.size = 16 * 16 * sizeof (uint32_t),
			},
		};
	}
	QFV_CreateResource (device, mpctx->resources);

	byte *entid_data;
	dfunc->vkMapMemory (device->dev, mpctx->resources->memory, 0,
						mpctx->resources->size, 0, (void **) &entid_data);

	for (size_t i = 0; i < frames; i++) {
		auto frame = &mpctx->frames.a[i];
		*frame = (qfv_mousepick_frame_t) {
			.entid_buffer = buffers[i].buffer.buffer,
			.entid_data = (uint32_t *) (entid_data + buffers[i].buffer.offset),
		};
	}

	qfvPopDebug (ctx);
}

void
QFV_MousePick_Shutdown (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto mpctx = ctx->mousepick_context;

	dfunc->vkUnmapMemory (device->dev, mpctx->resources->memory);
	QFV_DestroyResource (device, mpctx->resources);
	free (mpctx->resources);
	free (mpctx->frames.a);
	free (mpctx);
}

void
QFV_MousePick_Read (vulkan_ctx_t *ctx, uint32_t x, uint32_t y,
					mousepickfunc_t callback, void *data)
{
	auto mpctx = ctx->mousepick_context;
	auto frame = &mpctx->frames.a[ctx->curFrame];

	if (!mpctx->entid_res) {
		auto job = ctx->render_context->job;
		auto step = QFV_FindStep ("main", job);
		auto rp = &step->render->renderpasses[0];
		mpctx->entid_res = QFV_FindResource ("entid", rp);
	}
	frame->entid_image = mpctx->entid_res->image.image;
	VkExtent3D extent = mpctx->entid_res->image.extent;

	if (x >= extent.width || y >= extent.height) {
		return;
	}

	frame->callback = callback;
	frame->callback_data = data;

	frame->x = x;
	frame->y = y;
	frame->offset = (VkOffset3D) {
		.x = x >= (mousepick_size / 2) ? x - (mousepick_size / 2) : 0,
		.y = y >= (mousepick_size / 2) ? y - (mousepick_size / 2) : 0,
	};
	frame->extent = (VkExtent3D) {
		.width = extent.width > x + (mousepick_size / 2)
			? (mousepick_size / 2) + 1 : extent.width - x,
		.height = extent.height > y + (mousepick_size / 2)
			? (mousepick_size / 2) + 1 : extent.height - y,
		.depth = 1,
	};
	frame->extent.width += x - frame->offset.x;
	frame->extent.height += y - frame->offset.y;
}
