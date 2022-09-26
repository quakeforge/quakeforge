/*
	renderpass.c

	Vulkan render pass and frame buffer functions

	Copyright (C) 2020      Bill Currie <bill@taniwha.org>

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

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/plist.h"
#include "QF/va.h"

#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/swapchain.h"
#include "QF/Vulkan/qf_renderpass.h"

#include "vid_vulkan.h"
#include "vkparse.h"

static plitem_t *
get_rp_item (vulkan_ctx_t *ctx, qfv_renderpass_t *rp, const char *name)
{
	if (!rp->renderpassDef) {
		rp->renderpassDef = Vulkan_GetConfig (ctx, rp->name);
	}

	plitem_t   *item = rp->renderpassDef;
	if (!item) {
		Sys_Printf ("error loading %s\n", rp->name);
	} else if ((item = PL_ObjectForKey (item, name))) {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found %s def\n", name);
	}
	return item;
}

static size_t
get_image_size (VkImage image, qfv_device_t *device)
{
	qfv_devfuncs_t *dfunc = device->funcs;
	size_t      size;
	size_t      align;

	VkMemoryRequirements requirements;
	dfunc->vkGetImageMemoryRequirements (device->dev, image, &requirements);
	size = requirements.size;
	align = requirements.alignment - 1;
	size = (size + align) & ~(align);
	return size;
}

static void
destroy_framebuffers (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < rp->framebuffers->size; i++) {
		dfunc->vkDestroyFramebuffer (device->dev, rp->framebuffers->a[i], 0);
	}
	free (rp->framebuffers);
	rp->framebuffers = 0;
}

void
Vulkan_CreateAttachments (vulkan_ctx_t *ctx, qfv_renderpass_t *renderpass)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rp = renderpass;

	plitem_t   *item = get_rp_item (ctx, rp, "images");
	if (!item) {
		return;
	}

	if (renderpass->framebuffers) {
		destroy_framebuffers (ctx, renderpass);
	}
	if (rp->attachment_views) {
		for (size_t i = 0; i < rp->attachment_views->size; i++) {
			dfunc->vkDestroyImageView (device->dev,
									   rp->attachment_views->a[i], 0);
		}
		free (rp->attachment_views);
		rp->attachment_views = 0;
	}
	if (rp->attachment_images) {
		for (size_t i = 0; i < rp->attachment_images->size; i++) {
			dfunc->vkDestroyImage (device->dev, rp->attachment_images->a[i], 0);
		}
		free (rp->attachment_images);
		rp->attachment_images = 0;
	}

	__auto_type images = QFV_ParseImageSet (ctx, item, rp->renderpassDef);
	rp->attachment_images = images;
	size_t      memSize = 0;
	for (size_t i = 0; i < images->size; i++) {
		memSize += get_image_size (images->a[i], device);
	}
	VkDeviceMemory mem = rp->attachmentMemory;
	if (memSize > rp->attachmentMemory_size) {
		if (rp->attachmentMemory) {
			dfunc->vkFreeMemory (device->dev, rp->attachmentMemory, 0);
		}
		rp->attachmentMemory_size = memSize;
		mem = QFV_AllocImageMemory (device, images->a[0],
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									memSize, 0);
		rp->attachmentMemory = mem;
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
							 mem, "memory:framebuffers");
	}
	size_t      offset = 0;
	for (size_t i = 0; i < images->size; i++) {
		QFV_BindImageMemory (device, images->a[i], mem, offset);
		offset += get_image_size (images->a[i], device);
	}

	item = get_rp_item (ctx, rp, "imageViews");
	if (!item) {
		return;
	}

	__auto_type views = QFV_ParseImageViewSet (ctx, item, rp->renderpassDef);
	rp->attachment_views = views;

	item = get_rp_item (ctx, rp, "framebuffer");
	if (!item) {
		return;
	}

	rp->framebuffers = QFV_AllocFrameBuffers (ctx->swapchain->numImages,
											  malloc);
	for (size_t i = 0; i < rp->framebuffers->size; i++) {
		ctx->output.view = ctx->output.view_list[i];
		rp->framebuffers->a[i] = QFV_ParseFramebuffer (ctx, item,
													   rp->renderpassDef);
	}
}

static void
init_renderframe (vulkan_ctx_t *ctx, qfv_renderpass_t *rp,
				  qfv_renderframe_t *rFrame)
{
	rFrame->subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
	rFrame->vulkan_ctx = ctx;
	rFrame->renderpass = rp;
	rFrame->subpassCount = rp->subpassCount;
	if (rp->subpass_info) {
		rFrame->subpassInfo = rp->subpass_info->a;
	}
	rFrame->subpassCmdSets = malloc (rp->subpassCount
									 * sizeof (qfv_cmdbufferset_t));
	for (size_t j = 0; j < rp->subpassCount; j++) {
		DARRAY_INIT (&rFrame->subpassCmdSets[j], 4);
	}
}

static void
destroy_attachments (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (rp->attachment_views) {
		for (size_t i = 0; i < rp->attachment_views->size; i++) {
			dfunc->vkDestroyImageView (device->dev,
									   rp->attachment_views->a[i], 0);
		}
	}
	if (rp->attachment_images) {
		for (size_t i = 0; i < rp->attachment_images->size; i++) {
			dfunc->vkDestroyImage (device->dev, rp->attachment_images->a[i], 0);
		}
	}
	dfunc->vkFreeMemory (device->dev, rp->attachmentMemory, 0);

	free (rp->attachment_images);
	free (rp->attachment_views);
}

static void
destroy_renderframes (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	for (size_t i = 0; i < rp->frames.size; i++) {
		__auto_type rFrame = &rp->frames.a[i];
		for (int j = 0; j < rFrame->subpassCount; j++) {
			DARRAY_CLEAR (&rFrame->subpassCmdSets[j]);
		}
		free (rFrame->subpassCmdSets);
	}
}

qfv_renderpass_t *
Vulkan_CreateRenderPass (vulkan_ctx_t *ctx, const char *name,
						 qfv_output_t *output, qfv_draw_t draw)
{
	plitem_t   *item;

	qfv_renderpass_t *rp = calloc (1, sizeof (qfv_renderpass_t));

	rp->name = name;

	plitem_t   *rp_cfg = get_rp_item (ctx, rp, "renderpass");
	if (rp_cfg) {
		hashtab_t  *tab = ctx->renderpasses;
		const char *path;
		path = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", name);
		__auto_type renderpass = (VkRenderPass) QFV_GetHandle (tab, path);
		if (renderpass) {
			rp->renderpass = renderpass;
		} else {
			ctx->output = *output;
			rp->renderpass =  QFV_ParseRenderPass (ctx, rp_cfg,
												   rp->renderpassDef);
			QFV_AddHandle (tab, path, (uint64_t) rp->renderpass);
			QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_RENDER_PASS,
								 rp->renderpass, va (ctx->va_ctx,
													 "renderpass:%s", name));
		}
		rp->subpassCount = PL_A_NumObjects (PL_ObjectForKey (rp_cfg,
															 "subpasses"));
	}
	plitem_t   *rp_info = get_rp_item (ctx, rp, "info");
	if (rp_info) {
		plitem_t   *subpass_info = PL_ObjectForKey (rp_info, "subpass_info");
		if (subpass_info) {
			rp->subpass_info = QFV_ParseSubpasses (ctx, subpass_info,
												   rp->renderpassDef);
			if (rp->subpass_info->size < rp->subpassCount) {
				Sys_Printf ("warning:%s:%d: insufficient entries in "
							"subpass_info\n", name, PL_Line (subpass_info));
			}
			if (!rp->subpassCount) {
				rp->subpassCount = rp->subpass_info->size;
			}
		}

		plitem_t   *color = PL_ObjectForKey (rp_info, "color");
		if (color) {
			QFV_ParseRGBA (ctx, (float *)&rp->color, color, rp->renderpassDef);
		}
	}

	int         width = output->extent.width;
	int         height = output->extent.height;
	rp->viewport = (VkViewport) { 0, 0, width, height, 0, 1 };
	rp->scissor = (VkRect2D) { {0, 0}, {width, height} };

	DARRAY_INIT (&rp->frames, 4);
	DARRAY_RESIZE (&rp->frames, ctx->frames.size);
	for (size_t i = 0; i < rp->frames.size; i++) {
		init_renderframe (ctx, rp, &rp->frames.a[i]);
	}

	item = get_rp_item (ctx, rp, "clearValues");
	rp->clearValues = QFV_ParseClearValues (ctx, item, rp->renderpassDef);

	rp->draw = draw;

	Vulkan_CreateAttachments (ctx, rp);

	return rp;
}

void
Vulkan_DestroyRenderPass (vulkan_ctx_t *ctx, qfv_renderpass_t *renderpass)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	PL_Free (renderpass->renderpassDef);

	destroy_attachments (ctx, renderpass);
	dfunc->vkDestroyRenderPass (device->dev, renderpass->renderpass, 0);
	destroy_renderframes (ctx, renderpass);
	if (renderpass->framebuffers) {
		destroy_framebuffers (ctx, renderpass);
	}

	DARRAY_CLEAR (&renderpass->frames);
	free (renderpass->clearValues);
	free (renderpass);
}
