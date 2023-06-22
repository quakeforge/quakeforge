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
#include "QF/Vulkan/qf_renderpass.h"

#include "vid_vulkan.h"
#include "vkparse.h"

static plitem_t *
get_rp_item (vulkan_ctx_t *ctx, qfv_orenderpass_t *rp, const char *name)
{
	rp->renderpassDef = Vulkan_GetConfig (ctx, rp->name);

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
destroy_framebuffers (vulkan_ctx_t *ctx, qfv_orenderpass_t *rp)
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
QFV_RenderPass_CreateAttachments (qfv_orenderpass_t *renderpass)
{
	vulkan_ctx_t *ctx = renderpass->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rp = renderpass;

	if (rp->output.image) {
		// if output has an image, then the view is owned by the renderpass
		dfunc->vkDestroyImageView (device->dev, rp->output.view, 0);
		dfunc->vkDestroyImage (device->dev, rp->output.image, 0);
		free (rp->output.view_list);
		rp->output.view_list = 0;
		rp->output.view = 0;
		rp->output.image = 0;
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

	plitem_t   *output_def = get_rp_item (ctx, rp, "output");
	plitem_t   *images_def = get_rp_item (ctx, rp, "images");
	plitem_t   *views_def = get_rp_item (ctx, rp, "imageViews");

	plitem_t   *rp_def = rp->renderpassDef;

	size_t      memSize = 0;
	VkImage     ref_image = 0;
	if (output_def) {
		// QFV_ParseOutput clears the structure, but extent and frames need to
		// be preserved
		qfv_output_t output = rp->output;
		QFV_ParseOutput (ctx, &output, output_def, rp_def);
		rp->output.format = output.format;
		rp->output.finalLayout = output.finalLayout;

		plitem_t   *image = PL_ObjectForKey (output_def, "image");
		Vulkan_Script_SetOutput (ctx, &rp->output);
		rp->output.image = QFV_ParseImage (ctx, image, rp_def);
		memSize += get_image_size (rp->output.image, device);
		ref_image = rp->output.image;
	}
	if (images_def) {
		__auto_type images = QFV_ParseImageSet (ctx, images_def, rp_def);
		rp->attachment_images = images;
		ref_image = images->a[0];
		for (size_t i = 0; i < images->size; i++) {
			memSize += get_image_size (images->a[i], device);
		}
	}
	VkDeviceMemory mem = rp->attachmentMemory;
	if (memSize > rp->attachmentMemory_size) {
		if (rp->attachmentMemory) {
			dfunc->vkFreeMemory (device->dev, rp->attachmentMemory, 0);
		}
		rp->attachmentMemory_size = memSize;
		mem = QFV_AllocImageMemory (device, ref_image,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									memSize, 0);
		rp->attachmentMemory = mem;
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
							 mem, "memory:framebuffers");
	}
	size_t      offset = 0;
	if (rp->output.image) {
		QFV_BindImageMemory (device, rp->output.image, mem, offset);
		offset += get_image_size (rp->output.image, device);

		plitem_t   *view = PL_ObjectForKey (output_def, "view");
		Vulkan_Script_SetOutput (ctx, &rp->output);
		rp->output.view = QFV_ParseImageView (ctx, view, rp_def);
		rp->output.view_list = malloc (rp->output.frames
									   * sizeof (VkImageView));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE,
							 rp->output.image,
							 va (ctx->va_ctx, "image:%s:output", rp->name));
		QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_IMAGE_VIEW,
							 rp->output.view,
							 va (ctx->va_ctx, "iview:%s:output", rp->name));
		for (uint32_t i = 0; i < rp->output.frames; i++) {
			rp->output.view_list[i] = rp->output.view;
		}
	}
	if (rp->attachment_images) {
		__auto_type images = rp->attachment_images;
		for (size_t i = 0; i < images->size; i++) {
			QFV_BindImageMemory (device, images->a[i], mem, offset);
			offset += get_image_size (images->a[i], device);
		}
	}

	if (views_def) {
		__auto_type views = QFV_ParseImageViewSet (ctx, views_def, rp_def);
		rp->attachment_views = views;
	}
}

void
QFV_RenderPass_CreateRenderPass (qfv_orenderpass_t *renderpass)
{
	vulkan_ctx_t *ctx = renderpass->vulkan_ctx;
	__auto_type rp = renderpass;

	plitem_t   *rp_cfg = get_rp_item (ctx, rp, "renderpass");
	if (rp_cfg) {
		hashtab_t  *tab = ctx->script_context->renderpasses;
		const char *path;
		path = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", rp->name);
		__auto_type renderpass = (VkRenderPass) QFV_GetHandle (tab, path);
		if (renderpass) {
			rp->renderpass = renderpass;
		} else {
			Vulkan_Script_SetOutput (ctx, &rp->output);
			rp->renderpass =  QFV_ParseRenderPass (ctx, rp_cfg,
												   rp->renderpassDef);
			QFV_AddHandle (tab, path, (uint64_t) rp->renderpass);
			QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_RENDER_PASS,
								 rp->renderpass, va (ctx->va_ctx,
													 "renderpass:%s",
													 rp->name));
		}
		rp->subpassCount = PL_A_NumObjects (PL_ObjectForKey (rp_cfg,
															 "subpasses"));
	}


	plitem_t   *item = get_rp_item (ctx, rp, "clearValues");
	rp->clearValues = QFV_ParseClearValues (ctx, item, rp->renderpassDef);
}

void
QFV_RenderPass_CreateFramebuffer (qfv_orenderpass_t *renderpass)
{
	vulkan_ctx_t *ctx = renderpass->vulkan_ctx;
	__auto_type rp = renderpass;

	if (renderpass->framebuffers) {
		destroy_framebuffers (ctx, renderpass);
	}

	plitem_t   *fb_def = get_rp_item (ctx, rp, "framebuffer");
	plitem_t   *rp_def = rp->renderpassDef;
	if (fb_def) {
		rp->framebuffers = QFV_AllocFrameBuffers (rp->output.frames, malloc);
		for (size_t i = 0; i < rp->framebuffers->size; i++) {
			rp->output.view = rp->output.view_list[i];
			Vulkan_Script_SetOutput (ctx, &rp->output);
			rp->framebuffers->a[i] = QFV_ParseFramebuffer (ctx, fb_def, rp_def);
		}
	}

	int         width = rp->output.extent.width;
	int         height = rp->output.extent.height;
	rp->viewport = (VkViewport) { 0, 0, width, height, 0, 1 };
	rp->scissor = (VkRect2D) { {0, 0}, {width, height} };
	rp->renderArea = (VkRect2D) { {0, 0}, {width, height} };
}

static void
init_renderframe (vulkan_ctx_t *ctx, qfv_orenderpass_t *rp,
				  qfv_orenderframe_t *rFrame)
{
	rFrame->vulkan_ctx = ctx;
	rFrame->renderpass = rp;
	rFrame->subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
	rFrame->framebuffer = 0;
	rFrame->subpassCount = rp->subpassCount;
	rFrame->subpassInfo = 0;
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
destroy_attachments (vulkan_ctx_t *ctx, qfv_orenderpass_t *rp)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (rp->output.image) {
		// if output has an image, then the view is owned by the renderpass
		dfunc->vkDestroyImageView (device->dev, rp->output.view, 0);
		dfunc->vkDestroyImage (device->dev, rp->output.image, 0);
		free (rp->output.view_list);
		rp->output.view_list = 0;
		rp->output.view = 0;
		rp->output.image = 0;
	}
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
destroy_renderframes (vulkan_ctx_t *ctx, qfv_orenderpass_t *rp)
{
	for (size_t i = 0; i < rp->frames.size; i++) {
		__auto_type rFrame = &rp->frames.a[i];
		for (int j = 0; j < rFrame->subpassCount; j++) {
			DARRAY_CLEAR (&rFrame->subpassCmdSets[j]);
		}
		free (rFrame->subpassCmdSets);
	}
}

qfv_orenderpass_t *
QFV_RenderPass_New (vulkan_ctx_t *ctx, const char *name, qfv_draw_t function)
{
	qfv_orenderpass_t *rp = calloc (1, sizeof (qfv_orenderpass_t));
	rp->vulkan_ctx = ctx;
	rp->name = name;
	rp->draw = function;
	rp->renderpassDef = Vulkan_GetConfig (ctx, rp->name);

	plitem_t   *rp_info = get_rp_item (ctx, rp, "info");
	if (rp_info) {
		plitem_t   *subpass_info = PL_ObjectForKey (rp_info, "subpass_info");
		if (subpass_info) {
			rp->subpass_info = QFV_ParseSubpasses (ctx, subpass_info,
												   rp->renderpassDef);
			if (rp->subpass_info->size < rp->subpassCount) {
				Sys_Printf ("warning:%s:%d: insufficient entries in "
							"subpass_info\n", rp->name, PL_Line (subpass_info));
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
	if (!rp->subpassCount) {
		rp->subpassCount = 1;
	}

	DARRAY_INIT (&rp->frames, 4);
	//DARRAY_RESIZE (&rp->frames, ctx->frames.size);
	for (size_t i = 0; i < rp->frames.size; i++) {
		init_renderframe (ctx, rp, &rp->frames.a[i]);
	}
	return rp;
}

void
QFV_RenderPass_Delete (qfv_orenderpass_t *renderpass)
{
	vulkan_ctx_t *ctx = renderpass->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

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
