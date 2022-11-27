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

#include <string.h>

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/heapsort.h"
#include "QF/plist.h"
#include "QF/va.h"
#include "QF/scene/entity.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/swapchain.h"

#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_main.h"
#include "QF/Vulkan/qf_output.h"
#include "QF/Vulkan/qf_particles.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_vid.h"

#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"

static const char *instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	0,
};

static const char *device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	0,
};

void
Vulkan_Init_Common (vulkan_ctx_t *ctx)
{
	Sys_MaskPrintf (SYS_vulkan, "Vulkan_Init_Common\n");

	Vulkan_Init_Cvars ();
	R_Init_Cvars ();
	Vulkan_Script_Init (ctx);
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0,
										instance_extensions);//FIXME version
	DARRAY_INIT (&ctx->renderPasses, 4);
}

void
Vulkan_Shutdown_Common (vulkan_ctx_t *ctx)
{
	if (ctx->capture) {
		QFV_DestroyCapture (ctx->capture);
	}
	if (ctx->frames.size) {
		Vulkan_DestroyFrames (ctx);
	}
	if (ctx->swapchain) {
		QFV_DestroySwapchain (ctx->swapchain);
	}
	ctx->instance->funcs->vkDestroySurfaceKHR (ctx->instance->instance,
											   ctx->surface, 0);
	Vulkan_Script_Shutdown (ctx);
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

	//FIXME msaa and deferred rendering...
	//also, location
	ctx->msaaSamples = 1;
	/*ctx->msaaSamples = min ((VkSampleCountFlagBits) msaaSamples,
							QFV_GetMaxSampleCount (device->physDev));
	if (ctx->msaaSamples > 1) {
		name = "renderpass_msaa";
	}*/
}

void
Vulkan_CreateStagingBuffers (vulkan_ctx_t *ctx)
{
	// FIXME configurable?
	ctx->staging = QFV_CreateStagingBuffer (ctx->device, "vulkan_ctx",
											32*1024*1024, ctx->cmdpool);
}

void
Vulkan_CreateSwapchain (vulkan_ctx_t *ctx)
{
	VkSwapchainKHR old_swapchain = 0;
	if (ctx->swapchain) {
		//FIXME this shouldn't be here
		qfv_device_t *device = ctx->swapchain->device;
		VkDevice dev = device->dev;
		qfv_devfuncs_t *dfunc = device->funcs;
		old_swapchain = ctx->swapchain->swapchain;
		for (size_t i = 0; i < ctx->swapchain->imageViews->size; i++) {
			dfunc->vkDestroyImageView(dev, ctx->swapchain->imageViews->a[i], 0);
		}
		free (ctx->swapchain->images);
		free (ctx->swapchain->imageViews);
		free (ctx->swapchain);
	}
	ctx->swapchain = QFV_CreateSwapchain (ctx, old_swapchain);
}

static int
renderpass_cmp (const void *_a, const void *_b)
{
	__auto_type a = (const qfv_renderpass_t **) _a;
	__auto_type b = (const qfv_renderpass_t **) _b;
	return (*a)->order - (*b)->order;
}

void
Vulkan_CreateRenderPasses (vulkan_ctx_t *ctx)
{
	Vulkan_Output_CreateRenderPasses (ctx);
	Vulkan_Main_CreateRenderPasses (ctx);
	Vulkan_Particles_CreateRenderPasses (ctx);
	Vulkan_Lighting_CreateRenderPasses (ctx);

	heapsort (ctx->renderPasses.a, ctx->renderPasses.size,
			  sizeof (qfv_renderpass_t *), renderpass_cmp);
}

void
Vulkan_DestroyRenderPasses (vulkan_ctx_t *ctx)
{
	for (size_t i = 0; i < ctx->renderPasses.size; i++) {
		QFV_RenderPass_Delete (ctx->renderPasses.a[i]);
	}
}

void
Vulkan_CreateFrames (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	VkCommandPool cmdpool = ctx->cmdpool;

	if (!ctx->frames.grow) {
		DARRAY_INIT (&ctx->frames, 4);
	}

	DARRAY_RESIZE (&ctx->frames, vulkan_frame_count);

	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (ctx->frames.size,
														alloca);
	QFV_AllocateCommandBuffers (device, cmdpool, 0, cmdBuffers);

	for (size_t i = 0; i < ctx->frames.size; i++) {
		__auto_type frame = &ctx->frames.a[i];
		frame->fence = QFV_CreateFence (device, 1);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		frame->renderDoneSemaphore = QFV_CreateSemaphore (device);
		frame->cmdBuffer = cmdBuffers->a[i];
	}
}

void
Vulkan_CreateCapture (vulkan_ctx_t *ctx)
{
	ctx->capture = QFV_CreateCapture (ctx->device, ctx->frames.size,
									  ctx->swapchain, ctx->cmdpool);
}

void
Vulkan_DestroyFrames (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *df = device->funcs;
	VkDevice    dev = device->dev;

	for (size_t i = 0; i < ctx->frames.size; i++) {
		__auto_type frame = &ctx->frames.a[i];
		df->vkDestroyFence (dev, frame->fence, 0);
		df->vkDestroySemaphore (dev, frame->imageAvailableSemaphore, 0);
		df->vkDestroySemaphore (dev, frame->renderDoneSemaphore, 0);
	}

	DARRAY_CLEAR (&ctx->frames);
}

void
Vulkan_BeginEntityLabel (vulkan_ctx_t *ctx, VkCommandBuffer cmd, entity_t ent)
{
	qfv_device_t *device = ctx->device;
	uint32_t    entgen = Ent_Generation (ent.id);
	uint32_t    entind = Ent_Index (ent.id);
	transform_t transform = Entity_Transform (ent);
	vec4f_t     pos = Transform_GetWorldPosition (transform);
	vec4f_t     dir = normalf (pos - (vec4f_t) { 0, 0, 0, 1 });
	vec4f_t     color = 0.5 * dir + (vec4f_t) {0.5, 0.5, 0.5, 1 };

	QFV_CmdBeginLabel (device, cmd,
					   va (ctx->va_ctx, "ent %03x.%05x [%g, %g, %g]",
						   entgen, entind, VectorExpand (pos)), color);
}
