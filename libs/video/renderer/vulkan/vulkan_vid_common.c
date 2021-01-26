/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

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
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/qfplist.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/shader.h"
#include "QF/Vulkan/staging.h"
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

	if (cexpr_parse_enum (QFV_GetEnum ("VkSampleCountFlagBits"), var->string,
						  &context, &var->int_val)) {
		Sys_Printf ("Invalid MSAA samples, using 1\n");
		var->int_val = VK_SAMPLE_COUNT_1_BIT;
	}
	delete_memsuper (context.memsuper);
}

static void
Vulkan_Init_Cvars (void)
{
	vulkan_use_validation = Cvar_Get ("vulkan_use_validation", "1", CVAR_NONE,
									  0,
									  "enable KRONOS Validation Layer if "
									  "available (requires instance "
									  "restart).");
	// FIXME implement fallback choices (instead of just fifo)
	vulkan_presentation_mode = Cvar_Get ("vulkan_presentation_mode", "mailbox",
										 CVAR_NONE, vulkan_presentation_mode_f,
										 "desired presentation mode (may fall "
										 "back to fifo).");
	msaaSamples = Cvar_Get ("msaaSamples", "VK_SAMPLE_COUNT_1_BIT",
										 CVAR_NONE, msaaSamples_f,
										 "desired MSAA sample size.");
	R_Init_Cvars ();
}

static const char *instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	0,
};

static const char *device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	0,
};

void
Vulkan_Init_Common (vulkan_ctx_t *ctx)
{
	Sys_Printf ("Vulkan_Init_Common\n");
	QFV_InitParse (ctx);
	Vulkan_Init_Cvars ();
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0, instance_extensions);//FIXME version
}

static void
clear_table (hashtab_t **table)
{
	if (*table) {
		hashtab_t  *tab = *table;
		*table = 0;
		Hash_DelTable (tab);
	}
}

void
Vulkan_Shutdown_Common (vulkan_ctx_t *ctx)
{
	if (ctx->pipeline) {
		QFV_DestroyPipeline (ctx->device, ctx->pipeline);
	}
	if (ctx->framebuffers.size) {
		Vulkan_DestroyFramebuffers (ctx);
	}
	if (ctx->renderpass.colorImage) {
		Vulkan_DestroyRenderPass (ctx);
	}
	if (ctx->swapchain) {
		QFV_DestroySwapchain (ctx->swapchain);
	}
	QFV_DestroyStagingBuffer (ctx->staging);
	Vulkan_DestroyMatrices (ctx);
	ctx->instance->funcs->vkDestroySurfaceKHR (ctx->instance->instance,
											   ctx->surface, 0);
	clear_table (&ctx->pipelineLayouts);
	clear_table (&ctx->setLayouts);
	clear_table (&ctx->shaderModules);
	clear_table (&ctx->descriptorPools);
	clear_table (&ctx->samplers);
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
Vulkan_CreateStagingBuffers (vulkan_ctx_t *ctx)
{
	ctx->staging = QFV_CreateStagingBuffer (ctx->device, 4*1024*1024,
											ctx->cmdpool);
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

static void
qfv_load_pipeline (vulkan_ctx_t *ctx)
{
	if (!ctx->pipelineDef) {
		ctx->pipelineDef = PL_GetPropertyList (quakeforge_pipeline);
		if (ctx->pipelineDef) {
			QFV_ParseResources (ctx, ctx->pipelineDef);
		}
	}
}

void
Vulkan_CreateRenderPass (vulkan_ctx_t *ctx)
{
	qfv_load_pipeline (ctx);

	plitem_t   *item = ctx->pipelineDef;
	if (!item || !(item = PL_ObjectForKey (item, "renderpass"))) {
		Sys_Printf ("error loading renderpass\n");
		return;
	} else {
		Sys_Printf ("Found renderpass def\n");
	}
	qfv_device_t *device = ctx->device;
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *df = device->funcs;
	VkCommandBuffer cmd = ctx->cmdbuffer;
	qfv_swapchain_t *sc = ctx->swapchain;

	qfv_imageresource_t *colorImage = malloc (sizeof (*colorImage));
	qfv_imageresource_t *depthImage = malloc (sizeof (*depthImage));

	VkExtent3D extent = {sc->extent.width, sc->extent.height, 1};

	//FIXME incorporate cvar setting
	ctx->msaaSamples = QFV_GetMaxSampleCount (device->physDev);

	Sys_MaskPrintf (SYS_VULKAN, "color resource\n");
	colorImage->image
		= QFV_CreateImage (device, 0, VK_IMAGE_TYPE_2D,
						   sc->format, extent, 1, 1, ctx->msaaSamples,
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
						   depthFormat, extent, 1, 1, ctx->msaaSamples,
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
	ctx->renderpass.renderpass = QFV_ParseRenderPass (ctx, item);
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

VkPipeline
Vulkan_CreatePipeline (vulkan_ctx_t *ctx, const char *name)
{
	qfv_load_pipeline (ctx);

	plitem_t   *item = ctx->pipelineDef;
	if (!item || !(item = PL_ObjectForKey (item, "pipelines"))) {
		Sys_Printf ("error loading pipelines\n");
		return 0;
	} else {
		Sys_Printf ("Found pipelines def\n");
	}
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_Printf ("Found pipeline def %s\n", name);
	}
	return QFV_ParsePipeline (ctx, item);
}

void
Vulkan_CreateFramebuffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	VkCommandPool cmdpool = ctx->cmdpool;

	if (!ctx->framebuffers.grow) {
		DARRAY_INIT (&ctx->framebuffers, 4);
	}

	DARRAY_RESIZE (&ctx->framebuffers, 3);//FIXME cvar

	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (ctx->framebuffers.size,
														alloca);
	QFV_AllocateCommandBuffers (device, cmdpool, 0, cmdBuffers);

	for (size_t i = 0; i < ctx->framebuffers.size; i++) {
		__auto_type frame = &ctx->framebuffers.a[i];
		frame->framebuffer = 0;
		frame->fence = QFV_CreateFence (device, 1);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		frame->renderDoneSemaphore = QFV_CreateSemaphore (device);
		frame->cmdBuffer = cmdBuffers->a[i];

		frame->subCommand = malloc (sizeof (qfv_cmdbufferset_t));
		DARRAY_INIT (frame->subCommand, 4);
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
