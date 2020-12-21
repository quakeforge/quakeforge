/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/swapchain.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"
#include "vkparse.h"

static const char quakeforge_pipeline[] =
#include "libs/video/renderer/vulkan/qfpipeline.plc"
;

cvar_t *vulkan_presentation_mode;
cvar_t *msaaSamples;

static void
vulkan_presentation_mode_f (cvar_t *var)
{
	if (!strcmp (var->string, "immediate")) {
		var->int_val = VK_PRESENT_MODE_IMMEDIATE_KHR;
	} else if (!strcmp (var->string, "fifo")) {
		var->int_val = VK_PRESENT_MODE_FIFO_KHR;
	} else if (!strcmp (var->string, "relaxed")) {
		var->int_val = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	} else if (!strcmp (var->string, "mailbox")) {
		var->int_val = VK_PRESENT_MODE_MAILBOX_KHR;
	} else {
		Sys_Printf ("Invalid presentation mode, using fifo\n");
		var->int_val = VK_PRESENT_MODE_FIFO_KHR;
	}
}

static void
msaaSamples_f (cvar_t *var)
{
	exprctx_t   context = {};
	context.memsuper = new_memsuper();

	if (cexpr_parse_enum (&VkSampleCountFlagBits_enum, var->string, &context,
						  &var->int_val)) {
		Sys_Printf ("Invalid MSAA samples, using 1\n");
		var->int_val = VK_SAMPLE_COUNT_1_BIT;
	}
}

static void
Vulkan_Init_Cvars (void)
{
	vulkan_use_validation = Cvar_Get ("vulkan_use_validation", "1", CVAR_NONE,
									  0,
									  "enable LunarG Standard Validation "
									  "Layer if available (requires instance "
									  "restart).");
	// FIXME implement fallback choices (instead of just fifo)
	vulkan_presentation_mode = Cvar_Get ("vulkan_presentation_mode", "mailbox",
										 CVAR_NONE, vulkan_presentation_mode_f,
										 "desired presentation mode (may fall "
										 "back to fifo).");
	msaaSamples = Cvar_Get ("msaaSamples", "VK_SAMPLE_COUNT_1_BIT",
										 CVAR_NONE, msaaSamples_f,
										 "desired MSAA sample size.");
}

static const char *instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	0,
};

static const char *device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	0,
};

void
Vulkan_Init_Common (vulkan_ctx_t *ctx)
{
	Sys_Printf ("Vulkan_Init_Common\n");
	QFV_InitParse ();
	Vulkan_Init_Cvars ();
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0, instance_extensions);//FIXME version
}

void
Vulkan_Shutdown_Common (vulkan_ctx_t *ctx)
{
	if (ctx->framebuffers.size) {
		Vulkan_DestroyFramebuffers (ctx);
	}
	if (ctx->renderpass.colorImage) {
		Vulkan_DestroyRenderPass (ctx);
	}
	if (ctx->swapchain) {
		QFV_DestroySwapchain (ctx->swapchain);
	}
	ctx->instance->funcs->vkDestroySurfaceKHR (ctx->instance->instance,
											   ctx->surface, 0);
	if (ctx->device) {
		QFV_DestroyDevice (ctx->device);
	}
	if (ctx->instance) {
		QFV_DestroyInstance (ctx->instance);
	}
	ctx->instance = 0;
	ctx->unload_vulkan (ctx);
}

void
Vulkan_CreateDevice (vulkan_ctx_t *ctx)
{
	ctx->device = QFV_CreateDevice (ctx, device_extensions);
}

void
Vulkan_CreateSwapchain (vulkan_ctx_t *ctx)
{
	VkSwapchainKHR old_swapchain = 0;
	if (ctx->swapchain) {
		old_swapchain = ctx->swapchain->swapchain;
		free (ctx->swapchain);
	}
	ctx->swapchain = QFV_CreateSwapchain (ctx, old_swapchain);
	if (ctx->swapchain->swapchain == old_swapchain) {
		ctx->device->funcs->vkDestroySwapchainKHR (ctx->device->dev,
												   old_swapchain, 0);
	}
}

typedef struct {
	VkPipelineStageFlags src;
	VkPipelineStageFlags dst;
} qfv_pipelinestagepair_t;

//XXX Note: imageLayoutTransitionBarriers, imageLayoutTransitionStages and
// the enum be kept in sync
enum {
	qfv_LT_Undefined_to_TransferDst,
	qfv_LT_TransferDst_to_ShaderReadOnly,
	qfv_LT_Undefined_to_DepthStencil,
	qfv_LT_Undefined_to_Color,
};

static VkImageMemoryBarrier imageLayoutTransitionBarriers[] = {
	// undefined -> transfer dst optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	// transfer dst optimal -> shader read only optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	// undefined -> depth stencil attachment optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
	},
	// undefined -> color attachment optimal
	{	VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
			| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	},
	{ /* end of transition barriers */ }
};

static qfv_pipelinestagepair_t imageLayoutTransitionStages[] = {
	// undefined -> transfer dst optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT },
	// transfer dst optimal -> shader read only optimal
	{	VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT },
	// undefined -> depth stencil attachment optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT },
	// undefined -> color attachment optimal
	{	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
};

static plitem_t *
qfv_load_pipeline (void)
{
	return PL_GetPropertyList (quakeforge_pipeline);
}

void
Vulkan_CreateRenderPass (vulkan_ctx_t *ctx)
{
	plitem_t   *item = qfv_load_pipeline ();

	if (!item || !(item = PL_ObjectForKey (item, "renderpass"))) {
		Sys_Printf ("error loading pipeline\n");
	} else {
		Sys_Printf ("Found renderer def\n");
	}
	qfv_device_t *device = ctx->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *df = device->funcs;
	VkCommandBuffer cmd = ctx->cmdbuffer;
	qfv_swapchain_t *sc = ctx->swapchain;

	qfv_imageresource_t *colorImage = malloc (sizeof (*colorImage));
	qfv_imageresource_t *depthImage = malloc (sizeof (*depthImage));

	VkExtent3D extent = {sc->extent.width, sc->extent.height, 1};

	VkSampleCountFlagBits msaaSamples
		= QFV_GetMaxSampleCount (device->physDev);

	Sys_MaskPrintf (SYS_VULKAN, "color resource\n");
	colorImage->image
		= QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
						   sc->format, extent, 1, 1, msaaSamples,
						   VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
							   | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	colorImage->object
		= QFV_AllocImageMemory (device, colorImage->image,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0);
	QFV_BindImageMemory (device, colorImage->image, colorImage->object, 0);
	colorImage->view
		= QFV_CreateImageView (device, colorImage->image,
							   VK_IMAGE_VIEW_TYPE_2D,
							   sc->format, VK_IMAGE_ASPECT_COLOR_BIT);
	Sys_MaskPrintf (SYS_VULKAN, "	image: %p object: %p view:%p\n",
					colorImage->image, colorImage->object, colorImage->view);

	Sys_MaskPrintf (SYS_VULKAN, "depth resource\n");
	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
	depthImage->image
		= QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
						   depthFormat, extent, 1, 1, msaaSamples,
						   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	depthImage->object
		= QFV_AllocImageMemory (device, depthImage->image,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0);
	QFV_BindImageMemory (device, depthImage->image, depthImage->object, 0);
	depthImage->view
		= QFV_CreateImageView (device, depthImage->image,
							   VK_IMAGE_VIEW_TYPE_2D,
							   depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	Sys_MaskPrintf (SYS_VULKAN, "	image: %p object: %p view:%p\n",
					depthImage->image, depthImage->object, depthImage->view);

	VkImageMemoryBarrier barrier;
	qfv_pipelinestagepair_t stages;

	df->vkWaitForFences (dev, 1, &ctx->fence, VK_TRUE, ~0ull);
	df->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, 0,
	};
	df->vkBeginCommandBuffer (cmd, &beginInfo);

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_Color];
	barrier = imageLayoutTransitionBarriers[qfv_LT_Undefined_to_Color];
	barrier.image = colorImage->image;

	df->vkCmdPipelineBarrier (cmd, stages.src, stages.dst, 0,
							  0, 0,
							  0, 0,
							  1, &barrier);

	stages = imageLayoutTransitionStages[qfv_LT_Undefined_to_DepthStencil];
	barrier = imageLayoutTransitionBarriers[qfv_LT_Undefined_to_DepthStencil];
	barrier.image = depthImage->image;
	if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
		|| depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	df->vkCmdPipelineBarrier (cmd, stages.src, stages.dst, 0,
							  0, 0,
							  0, 0,
							  1, &barrier);
	df->vkEndCommandBuffer (cmd);

	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		0, 0, 0,
		1, &cmd,
		0, 0
	};
	df->vkResetFences (dev, 1, &ctx->fence);
	df->vkQueueSubmit (device->queue.queue, 1, &submitInfo, ctx->fence);

	ctx->renderpass.colorImage = colorImage;
	ctx->renderpass.depthImage = depthImage;

	__auto_type attachments = QFV_AllocAttachmentDescription (3, alloca);
	__auto_type attachmentRefs = QFV_AllocAttachmentReference (3, alloca);

	// color attachment
	attachments->a[0].flags = 0;
	attachments->a[0].format = sc->format;
	attachments->a[0].samples = msaaSamples;
	attachments->a[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments->a[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments->a[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments->a[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments->a[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments->a[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentRefs->a[0].attachment = 0;
	attachmentRefs->a[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// depth attachment
	attachments->a[1].flags = 0;
	attachments->a[1].format = depthFormat;
	attachments->a[1].samples = msaaSamples;
	attachments->a[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments->a[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments->a[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments->a[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments->a[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments->a[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentRefs->a[1].attachment = 1;
	attachmentRefs->a[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// resolve attachment
	attachments->a[2].flags = 0;
	attachments->a[2].format = sc->format;
	attachments->a[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments->a[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments->a[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments->a[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments->a[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments->a[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments->a[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachmentRefs->a[2].attachment = 2;
	attachmentRefs->a[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	__auto_type subpasses = QFV_AllocSubpassParametersSet (1, alloca);
	subpasses->a[0].flags = 0;
	subpasses->a[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses->a[0].inputAttachmentCount = 0;
	subpasses->a[0].pInputAttachments = 0;
	subpasses->a[0].colorAttachmentCount = 1;
	subpasses->a[0].pColorAttachments = &attachmentRefs->a[0];
	subpasses->a[0].pResolveAttachments = &attachmentRefs->a[2];
	subpasses->a[0].pDepthStencilAttachment = &attachmentRefs->a[1];
	subpasses->a[0].preserveAttachmentCount = 0;
	subpasses->a[0].pPreserveAttachments = 0;

	__auto_type depenencies = QFV_AllocSubpassDependencies (1, alloca);
	depenencies->a[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	depenencies->a[0].dstSubpass = 0;
	depenencies->a[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depenencies->a[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depenencies->a[0].srcAccessMask = 0;
	depenencies->a[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	depenencies->a[0].dependencyFlags = 0;

	ctx->renderpass.renderpass = QFV_CreateRenderPass (device, attachments,
													   subpasses, depenencies);
}

void
Vulkan_DestroyRenderPass (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *df = device->funcs;

	df->vkDestroyRenderPass (dev, ctx->renderpass.renderpass, 0);

	df->vkDestroyImageView (dev, ctx->renderpass.colorImage->view, 0);
	df->vkDestroyImage (dev, ctx->renderpass.colorImage->image, 0);
	df->vkFreeMemory (dev, ctx->renderpass.colorImage->object, 0);
	free (ctx->renderpass.colorImage);
	ctx->renderpass.colorImage = 0;

	df->vkDestroyImageView (dev, ctx->renderpass.depthImage->view, 0);
	df->vkDestroyImage (dev, ctx->renderpass.depthImage->image, 0);
	df->vkFreeMemory (dev, ctx->renderpass.depthImage->object, 0);
	free (ctx->renderpass.depthImage);
	ctx->renderpass.depthImage = 0;
}

void
Vulkan_CreateFramebuffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	VkCommandPool cmdpool = ctx->cmdpool;
	qfv_swapchain_t *sc = ctx->swapchain;
	VkRenderPass renderpass = ctx->renderpass.renderpass;

	if (!ctx->framebuffers.grow) {
		DARRAY_INIT (&ctx->framebuffers, 4);
	}

	DARRAY_RESIZE (&ctx->framebuffers, sc->numImages);

	__auto_type attachments = DARRAY_ALLOCFIXED (qfv_imageviewset_t, 3,
												 alloca);
	attachments->a[0] = ctx->renderpass.colorImage->view;
	attachments->a[1] = ctx->renderpass.depthImage->view;

	__auto_type cmdBuffers
		= QFV_AllocateCommandBuffers (device, cmdpool, 0,
									  ctx->framebuffers.size);

	for (size_t i = 0; i < ctx->framebuffers.size; i++) {
		attachments->a[2] = sc->imageViews->a[i];
		__auto_type frame = &ctx->framebuffers.a[i];
		frame->framebuffer = QFV_CreateFramebuffer (device, renderpass,
												    attachments,
												    sc->extent, 1);
		frame->fence = QFV_CreateFence (device, 1);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		frame->renderDoneSemaphore = QFV_CreateSemaphore (device);
		frame->cmdBuffer = cmdBuffers->a[i];
	}
}

void
Vulkan_DestroyFramebuffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *df = device->funcs;
	VkDevice    dev = device->dev;

	for (size_t i = 0; i < ctx->framebuffers.size; i++) {
		__auto_type frame = &ctx->framebuffers.a[i];
		df->vkDestroyFence (dev, frame->fence, 0);
		df->vkDestroySemaphore (dev, frame->imageAvailableSemaphore, 0);
		df->vkDestroySemaphore (dev, frame->renderDoneSemaphore, 0);
		df->vkDestroyFramebuffer (dev, frame->framebuffer, 0);
	}

	DARRAY_CLEAR (&ctx->framebuffers);
}
