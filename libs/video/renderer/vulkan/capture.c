/*
	capture.c

	Vulkan frame capture support

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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

#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/swapchain.h"

#include "QF/plugin/vid_render.h"
#include "vid_vulkan.h"

static void
capture_initiate (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto cap = ctx->capture_context;
	auto frame = &cap->frames.a[ctx->curFrame];

	if (!frame->callback) {
		return;
	}

	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	auto sc = ctx->swapchain;
	auto scImage = sc->images->a[ctx->swapImageIndex];
	auto buffer = frame->buffer->buffer.buffer;

	VkBufferMemoryBarrier start_buffer_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.buffer = buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	VkImageMemoryBarrier start_image_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = scImage,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
	};
	VkBufferMemoryBarrier end_buffer_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.buffer = buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	VkImageMemoryBarrier end_image_barriers[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = scImage,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		},
	};

	dfunc->vkCmdPipelineBarrier (cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0,
								 1, start_buffer_barriers,
								 1, start_image_barriers);

	VkBufferImageCopy copy = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageOffset = { },
		.imageExtent = { cap->extent.width, cap->extent.height, 1 },
	};
	dfunc->vkCmdCopyImageToBuffer (cmd, scImage,
								   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								   buffer, 1, &copy);

	dfunc->vkCmdPipelineBarrier (cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0,
								 1, end_buffer_barriers,
								 1, end_image_barriers);
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);

	frame->initiated = true;
	auto time = Sys_LongTime ();
	printf ("capture_initiate: %zd.%03zd.%0zd\n",
			time / 1000000, (time / 1000) % 1000, time % 1000);
}

static int
is_bgr (VkFormat format)
{
	return (format >= VK_FORMAT_B8G8R8A8_UNORM
			&& format <= VK_FORMAT_B8G8R8A8_SRGB);
}

static void
capture_finalize (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto cap = ctx->capture_context;
	auto frame = &cap->frames.a[ctx->curFrame];

	if (!frame->callback || !frame->initiated) {
		return;
	}
	auto time = Sys_LongTime ();
	printf ("capture_finalize: %zd.%03zd.%0zd\n",
			time / 1000000, (time / 1000) % 1000, time % 1000);

	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = cap->resources->memory,
		.offset = frame->data - cap->data,
		.size = cap->imgsize,
	};
	dfunc->vkInvalidateMappedMemoryRanges (device->dev, 1, &range);

	int         count = cap->extent.width * cap->extent.height;
	tex_t      *tex = malloc (sizeof (tex_t) + count * 3);

	if (tex) {
		tex->data = (byte *) (tex + 1);
		tex->flagbits = 0;
		tex->width = cap->extent.width;
		tex->height = cap->extent.height;
		tex->format = tex_rgb;
		tex->palette = 0;
		tex->flagbits = 0;
		tex->loaded = 1;

		if (is_bgr (ctx->swapchain->format)) {
			tex->bgr = 1;
		}
		const byte *src = frame->data;
		byte       *dst = tex->data;
		while (count-- > 0) {
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			src++;
		}
	}

	frame->callback (tex, frame->callback_data);;
	frame->callback = 0;
	frame->callback_data = 0;
	frame->initiated = false;
}

static exprfunc_t capture_initiate_func[] = {
	{ .func = capture_initiate },
	{}
};
static exprfunc_t capture_finalize_func[] = {
	{ .func = capture_finalize },
	{}
};
static exprsym_t capture_task_syms[] = {
	{ "capture_initiate", &cexpr_function, capture_initiate_func },
	{ "capture_finalize", &cexpr_function, capture_finalize_func },
	{}
};

void
QFV_Capture_Init (vulkan_ctx_t *ctx)
{
	QFV_Render_AddTasks (ctx, capture_task_syms);

	qfvPushDebug (ctx, "capture init");

	auto device = ctx->device;
	auto ifunc = device->physDev->instance->funcs;

	ctx->capture_context = calloc (1, sizeof (qfv_capturectx_t));
	auto cap = ctx->capture_context;

	auto swapchain = ctx->swapchain;
	VkFormatProperties format_props;
	ifunc->vkGetPhysicalDeviceFormatProperties (device->physDev->dev,
												swapchain->format,
												&format_props);
	if (!(swapchain->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
		Sys_Printf ("Swapchain does not support reading. FIXME\n");
		return;
	}

	cap->extent = swapchain->extent;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&cap->frames, frames);
	DARRAY_RESIZE (&cap->frames, frames);
	cap->frames.grow = 0;

	cap->resources = calloc (1, sizeof (qfv_resource_t)
							 + sizeof (qfv_resobj_t[frames]));
	auto buffers = (qfv_resobj_t *) &cap->resources[1];
	cap->resources[0] = (qfv_resource_t) {
		.name = "capture",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = frames,
		.objects = buffers,
	};
	for (size_t i = 0; i < frames; i++) {
		buffers[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "capture:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		auto frame = &cap->frames.a[i];
		*frame = (qfv_capture_frame_t) {
			.buffer = &buffers[i],
		};
	}

	qfvPopDebug (ctx);
}

void
QFV_Capture_Renew (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto swapchain = ctx->swapchain;
	auto cap = ctx->capture_context;

	if (!cap->resources) {
		return;
	}
	if (cap->resources->memory) {
		dfunc->vkUnmapMemory (device->dev, cap->resources->memory);
		QFV_DestroyResource (device, cap->resources);
	}

	cap->extent = swapchain->extent;
	//FIXME assumes the swapchain is 32bpp
	cap->imgsize = swapchain->extent.width * swapchain->extent.height * 4;
	for (uint32_t i = 0; i < cap->resources->num_objects; i++) {
		auto obj = &cap->resources->objects[i];
		obj->buffer.size = cap->imgsize;
	}
	QFV_CreateResource (device, cap->resources);
	dfunc->vkMapMemory (device->dev, cap->resources->memory, 0,
						cap->resources->size, 0, (void **) &cap->data);
	for (size_t i = 0; i < cap->frames.size; i++) {
		auto frame = &cap->frames.a[i];
		frame->data = cap->data + i * cap->imgsize;
	}
}

void
QFV_Capture_Shutdown (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto cap = ctx->capture_context;

	if (cap->resources->memory) {
		dfunc->vkUnmapMemory (device->dev, cap->resources->memory);
		QFV_DestroyResource (device, cap->resources);
	}
	free (cap->resources);
	free (cap->frames.a);
	free (cap);
}

void
QFV_Capture_Screen (vulkan_ctx_t *ctx, capfunc_t callback, void *data)
{
	auto cap = ctx->capture_context;
	if (!cap->resources) {
		Sys_Printf ("Capture not supported\n");
		callback (0, data);
		return;
	}
	if (!cap->resources->memory) {
		QFV_Capture_Renew (ctx);
	}
	auto frame = &cap->frames.a[ctx->curFrame];
	frame->callback = callback;
	frame->callback_data = data;
	auto time = Sys_LongTime ();
	printf ("capture_request: %zd.%03zd.%0zd\n",
			time / 1000000, (time / 1000) % 1000, time % 1000);
}
