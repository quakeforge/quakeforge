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
#include "QF/plist.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/debug.h"
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

static const char quakeforge_renderpass[] =
#include "libs/video/renderer/vulkan/deferred.plc"
;

cvar_t *vulkan_frame_count;
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
vulkan_frame_count_f (cvar_t *var)
{
	if (var->int_val < 1) {
		Sys_Printf ("Invalid frame count: %d. Setting to 1\n", var->int_val);
		Cvar_Set (var, "1");
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
	vulkan_frame_count = Cvar_Get ("vulkan_frame_count", "3", CVAR_NONE,
								   vulkan_frame_count_f,
								   "Number of frames to render in the"
								   " background. More frames can increase"
								   " performance, but at the cost of latency."
								   " The default of 3 is recommended.");
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
	Sys_MaskPrintf (SYS_VULKAN, "Vulkan_Init_Common\n");

	QFV_InitParse (ctx);
	Vulkan_Init_Cvars ();
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0,
										instance_extensions);//FIXME version
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
	PL_Free (ctx->pipelineDef);
	PL_Free (ctx->renderpassDef);
	if (ctx->capture) {
		QFV_DestroyCapture (ctx->capture);
	}
	if (ctx->frames.size) {
		Vulkan_DestroyFrames (ctx);
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

	//FIXME msaa and deferred rendering...
	//also, location
	ctx->msaaSamples = 1;
	/*ctx->msaaSamples = min ((VkSampleCountFlagBits) msaaSamples->int_val,
							QFV_GetMaxSampleCount (device->physDev));
	if (ctx->msaaSamples > 1) {
		name = "renderpass_msaa";
	}*/
}

void
Vulkan_CreateStagingBuffers (vulkan_ctx_t *ctx)
{
	ctx->staging = QFV_CreateStagingBuffer (ctx->device, "vulkan_ctx",
											4*1024*1024, ctx->cmdpool);
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
	if (old_swapchain && ctx->swapchain->swapchain != old_swapchain) {
		ctx->device->funcs->vkDestroySwapchainKHR (ctx->device->dev,
												   old_swapchain, 0);
	}
}

static plitem_t *
qfv_load_pipeline (vulkan_ctx_t *ctx, const char *name)
{
	if (!ctx->pipelineDef) {
		ctx->pipelineDef = PL_GetPropertyList (quakeforge_pipeline,
											   &ctx->hashlinks);
	}

	plitem_t   *item = ctx->pipelineDef;
	if (!item || !(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading %s\n", name);
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found %s def\n", name);
	}
	return item;
}

static plitem_t *
qfv_load_renderpass (vulkan_ctx_t *ctx, const char *name)
{
	if (!ctx->renderpassDef) {
		ctx->renderpassDef = PL_GetPropertyList (quakeforge_renderpass,
												 &ctx->hashlinks);
	}

	plitem_t   *item = ctx->renderpassDef;
	if (!item || !(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading %s\n", name);
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found %s def\n", name);
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

void
Vulkan_CreateRenderPass (vulkan_ctx_t *ctx)
{
	const char *name = "renderpass";//FIXME

	hashtab_t  *tab = ctx->renderpasses;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", name);
	__auto_type renderpass = (VkRenderPass) QFV_GetHandle (tab, path);
	if (renderpass) {
		ctx->renderpass = renderpass;
		return;
	}

	plitem_t   *item = qfv_load_renderpass (ctx, name);

	ctx->renderpass = QFV_ParseRenderPass (ctx, item, ctx->renderpassDef);
	QFV_AddHandle (tab, path, (uint64_t) ctx->renderpass);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_RENDER_PASS,
						 ctx->renderpass, va (ctx->va_ctx, "renderpass:%s",
											  name));
	item = qfv_load_renderpass (ctx, "clearValues");
	QFV_ParseClearValues (ctx, item, ctx->renderpassDef);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	static float quad_vertices[] = {
		-1, -1, 0, 1,
		-1,  1, 0, 1,
		 1, -1, 0, 1,
		 1,  1, 0, 1,
	};
	ctx->quad_buffer = QFV_CreateBuffer (device, sizeof (quad_vertices),
										 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
										 | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	ctx->quad_memory = QFV_AllocBufferMemory (device, ctx->quad_buffer,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										0, 0);
	QFV_BindBufferMemory (device, ctx->quad_buffer, ctx->quad_memory, 0);

	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	float      *verts = QFV_PacketExtend (packet, sizeof (quad_vertices));
	memcpy (verts, quad_vertices, sizeof (quad_vertices));

	VkBufferMemoryBarrier wr_barriers[] = {
		{   VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			ctx->quad_buffer, 0, sizeof (quad_vertices) },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0, 1, wr_barriers, 0, 0);
	VkBufferCopy copy_region[] = {
		{ packet->offset, 0, sizeof (quad_vertices) },
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, ctx->staging->buffer,
							ctx->quad_buffer, 1, &copy_region[0]);
	VkBufferMemoryBarrier rd_barriers[] = {
		{   VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			ctx->quad_buffer, 0, sizeof (quad_vertices) },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
								 0, 0, 0, 1, rd_barriers, 0, 0);
	QFV_PacketSubmit (packet);
}

void
Vulkan_DestroyRenderPass (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyRenderPass (device->dev, ctx->renderpass, 0);

	dfunc->vkFreeMemory (device->dev, ctx->quad_memory, 0);
	dfunc->vkDestroyBuffer (device->dev, ctx->quad_buffer, 0);
}

VkPipeline
Vulkan_CreatePipeline (vulkan_ctx_t *ctx, const char *name)
{
	plitem_t   *item = qfv_load_pipeline (ctx, "pipelines");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found pipeline def %s\n", name);
	}
	VkPipeline pipeline = QFV_ParsePipeline (ctx, item, ctx->pipelineDef);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE, pipeline,
						 va (ctx->va_ctx, "pipeline:%s", name));
	return pipeline;
}

VkDescriptorPool
Vulkan_CreateDescriptorPool (vulkan_ctx_t *ctx, const char *name)
{
	hashtab_t  *tab = ctx->descriptorPools;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".descriptorPools.%s", name);
	__auto_type pool = (VkDescriptorPool) QFV_GetHandle (tab, path);
	if (pool) {
		return pool;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "descriptorPools");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading descriptor pool %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found descriptor pool def %s\n",
						name);
	}
	pool = QFV_ParseDescriptorPool (ctx, item, ctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) pool);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, pool,
						 va (ctx->va_ctx, "descriptor_pool:%s", name));
	return pool;
}

VkPipelineLayout
Vulkan_CreatePipelineLayout (vulkan_ctx_t *ctx, const char *name)
{
	hashtab_t  *tab = ctx->pipelineLayouts;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".pipelineLayouts.%s", name);
	__auto_type layout = (VkPipelineLayout) QFV_GetHandle (tab, path);
	if (layout) {
		return layout;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "pipelineLayouts");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline layout %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found pipeline layout def %s\n",
						name);
	}
	layout = QFV_ParsePipelineLayout (ctx, item, ctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) layout);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout,
						 va (ctx->va_ctx, "pipeline_layout:%s", name));
	return layout;
}

VkSampler
Vulkan_CreateSampler (vulkan_ctx_t *ctx, const char *name)
{
	hashtab_t  *tab = ctx->samplers;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".samplers.%s", name);
	__auto_type sampler = (VkSampler) QFV_GetHandle (tab, path);
	if (sampler) {
		return sampler;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "samplers");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading sampler %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found sampler def %s\n", name);
	}
	sampler = QFV_ParseSampler (ctx, item, ctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) sampler);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_SAMPLER, sampler,
						 va (ctx->va_ctx, "sampler:%s", name));
	return sampler;
}

VkDescriptorSetLayout
Vulkan_CreateDescriptorSetLayout(vulkan_ctx_t *ctx, const char *name)
{
	hashtab_t  *tab = ctx->setLayouts;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".setLayouts.%s", name);
	__auto_type set = (VkDescriptorSetLayout) QFV_GetHandle (tab, path);
	if (set) {
		return set;
	}

	plitem_t   *item = qfv_load_pipeline (ctx, "setLayouts");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading descriptor set %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_VULKAN_PARSE, "Found descriptor set def %s\n",
						name);
	}
	set = QFV_ParseDescriptorSetLayout (ctx, item, ctx->pipelineDef);
	QFV_AddHandle (tab, path, (uint64_t) set);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						 set, va (ctx->va_ctx, "descriptor_set:%s", name));
	return set;
}

void
Vulkan_CreateFrames (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	VkCommandPool cmdpool = ctx->cmdpool;

	if (!ctx->frames.grow) {
		DARRAY_INIT (&ctx->frames, 4);
	}

	DARRAY_RESIZE (&ctx->frames, vulkan_frame_count->int_val);

	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (ctx->frames.size,
														alloca);
	QFV_AllocateCommandBuffers (device, cmdpool, 0, cmdBuffers);

	for (size_t i = 0; i < ctx->frames.size; i++) {
		__auto_type frame = &ctx->frames.a[i];
		frame->framebuffer = 0;
		frame->fence = QFV_CreateFence (device, 1);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		frame->renderDoneSemaphore = QFV_CreateSemaphore (device);
		frame->cmdBuffer = cmdBuffers->a[i];

		frame->cmdSetCount = QFV_NumPasses;
		frame->cmdSets = malloc (QFV_NumPasses * sizeof (qfv_cmdbufferset_t));
		for (int j = 0; j < QFV_NumPasses; j++) {
			DARRAY_INIT (&frame->cmdSets[j], 4);
		}
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
		df->vkDestroyFramebuffer (dev, frame->framebuffer, 0);
		for (int j = 0; j < frame->cmdSetCount; j++) {
			DARRAY_CLEAR (&frame->cmdSets[j]);
		}
		free (frame->cmdSets);
	}

	DARRAY_CLEAR (&ctx->frames);
}

void
Vulkan_CreateFramebuffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	plitem_t   *item = qfv_load_renderpass (ctx, "images");
	if (!item) {
		return;
	}

	__auto_type images = QFV_ParseImageSet (ctx, item, ctx->renderpassDef);
	ctx->attachment_images = images;
	size_t      memSize = 0;
	for (size_t i = 0; i < images->size; i++) {
		memSize += get_image_size (images->a[i], device);
	}
	VkDeviceMemory mem;
	mem = QFV_AllocImageMemory (device, images->a[0],
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								memSize, 0);
	ctx->attachmentMemory = mem;
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 mem, "memory:framebuffers");
	size_t      offset = 0;
	for (size_t i = 0; i < images->size; i++) {
		QFV_BindImageMemory (device, images->a[i], mem, offset);
		offset += get_image_size (images->a[i], device);
	}

	item = qfv_load_renderpass (ctx, "imageViews");
	if (!item) {
		return;
	}

	__auto_type views = QFV_ParseImageViewSet (ctx, item, ctx->renderpassDef);
	ctx->attachment_views = views;

	item = qfv_load_renderpass (ctx, "framebuffer");
	if (!item) {
		return;
	}

	ctx->framebuffers = QFV_AllocFrameBuffers (ctx->swapchain->numImages,
											   malloc);
	for (size_t i = 0; i < ctx->framebuffers->size; i++) {
		ctx->swapImageIndex = i;
		ctx->framebuffers->a[i] = QFV_ParseFramebuffer (ctx, item,
														ctx->renderpassDef);
	}
}

void
Vulkan_DestroyFramebuffers (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < ctx->attachment_views->size; i++) {
		dfunc->vkDestroyImageView (device->dev, ctx->attachment_views->a[i], 0);
	}
	for (size_t i = 0; i < ctx->attachment_images->size; i++) {
		dfunc->vkDestroyImage (device->dev, ctx->attachment_images->a[i], 0);
	}
	dfunc->vkFreeMemory (device->dev, ctx->attachmentMemory, 0);
}
