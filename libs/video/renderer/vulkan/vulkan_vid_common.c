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
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_matrices.h"
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

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_main.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "vid_vulkan.h"

#include "util.h"
#include "vkparse.h"

#include "libs/video/renderer/vulkan/vkparse.hinc"

static const char quakeforge_pipeline[] =
#include "libs/video/renderer/vulkan/qfpipeline.plc"
;

static const char quakeforge_renderpass[] =
#include "libs/video/renderer/vulkan/deferred.plc"
;

int vulkan_frame_count;
static cvar_t vulkan_frame_count_cvar = {
	.name = "vulkan_frame_count",
	.description =
		"Number of frames to render in the background. More frames can "
		"increase performance, but at the cost of latency. The default of 3 is"
		" recommended.",
	.default_value = "3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &vulkan_frame_count },
};
int vulkan_presentation_mode;
static cvar_t vulkan_presentation_mode_cvar = {
	.name = "vulkan_presentation_mode",
	.description =
		"desired presentation mode (may fall back to fifo).",
	.default_value = "mailbox",
	.flags = CVAR_NONE,
	.value = {
		.type = &VkPresentModeKHR_type,
		.value = &vulkan_presentation_mode,
	},
};
int msaaSamples;
static cvar_t msaaSamples_cvar = {
	.name = "msaaSamples",
	.description =
		"desired MSAA sample size.",
	.default_value = "VK_SAMPLE_COUNT_1_BIT",
	.flags = CVAR_NONE,
	.value = { .type = &VkSampleCountFlagBits_type, .value = &msaaSamples },
};
static exprenum_t validation_enum;
static exprtype_t validation_type = {
	.name = "vulkan_use_validation",
	.size = sizeof (int),
	.binops = cexpr_flag_binops,
	.unops = cexpr_flag_unops,
	.data = &validation_enum,
	.get_string = cexpr_flags_get_string,
};

static int validation_values[] = {
	0,
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT,
};
static exprsym_t validation_symbols[] = {
	{"none", &validation_type, validation_values + 0},
	{"all", &validation_type, validation_values + 1},
	{}
};
static exprtab_t validation_symtab = {
	validation_symbols,
};
static exprenum_t validation_enum = {
	&validation_type,
	&validation_symtab,
};
static cvar_t vulkan_use_validation_cvar = {
	.name = "vulkan_use_validation",
	.description =
		"enable KRONOS Validation Layer if available (requires instance "
		"restart).",
	.default_value = "error|warning",
	.flags = CVAR_NONE,
	.value = { .type = &validation_type, .value = &vulkan_use_validation },
};

static void
vulkan_frame_count_f (void *data, const cvar_t *cvar)
{
	if (vulkan_frame_count < 1) {
		Sys_Printf ("Invalid frame count: %d. Setting to 1\n",
					vulkan_frame_count);
		vulkan_frame_count = 1;
	}
}

static void
Vulkan_Init_Cvars (void)
{
	int         num_syms = 0;
	for (exprsym_t *sym = VkDebugUtilsMessageSeverityFlagBitsEXT_symbols;
		 sym->name; sym++, num_syms++) {
	}
	for (exprsym_t *sym = validation_symbols; sym->name; sym++, num_syms++) {
	}
	validation_symtab.symbols = calloc (num_syms + 1, sizeof (exprsym_t));
	num_syms = 0;
	for (exprsym_t *sym = VkDebugUtilsMessageSeverityFlagBitsEXT_symbols;
		 sym->name; sym++, num_syms++) {
		validation_symtab.symbols[num_syms] = *sym;
		validation_symtab.symbols[num_syms].type = &validation_type;
	}
	for (exprsym_t *sym = validation_symbols; sym->name; sym++, num_syms++) {
		validation_symtab.symbols[num_syms] = *sym;
	}
	Cvar_Register (&vulkan_use_validation_cvar, 0, 0);
	// FIXME implement fallback choices (instead of just fifo)
	Cvar_Register (&vulkan_presentation_mode_cvar, 0, 0);
	Cvar_Register (&vulkan_frame_count_cvar, vulkan_frame_count_f, 0);
	Cvar_Register (&msaaSamples_cvar, 0, 0);
	R_Init_Cvars ();
}

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

	QFV_InitParse (ctx);
	Vulkan_Init_Cvars ();
	ctx->instance = QFV_CreateInstance (ctx, PACKAGE_STRING, 0x000702ff, 0,
										instance_extensions);//FIXME version
	DARRAY_INIT (&ctx->renderPasses, 4);
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
		Sys_MaskPrintf (SYS_vulkan_parse, "Found %s def\n", name);
	}
	return item;
}

static plitem_t *
qfv_load_renderpass (vulkan_ctx_t *ctx, qfv_renderpass_t *rp, const char *name)
{
	if (!rp->renderpassDef) {
		rp->renderpassDef = PL_GetPropertyList (quakeforge_renderpass,
												&ctx->hashlinks);
	}

	plitem_t   *item = rp->renderpassDef;
	if (!item || !(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading %s\n", name);
	} else {
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
renderpass_draw (qfv_renderframe_t *rFrame)
{
	Vulkan_Matrix_Draw (rFrame);
	Vulkan_RenderView (rFrame);
	Vulkan_FlushText (rFrame);//FIXME delayed by a frame?
	Vulkan_Lighting_Draw (rFrame);
	Vulkan_Compose_Draw (rFrame);
}

static void
create_attachements (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	qfv_device_t *device = ctx->device;

	plitem_t   *item = qfv_load_renderpass (ctx, rp, "images");
	if (!item) {
		return;
	}

	__auto_type images = QFV_ParseImageSet (ctx, item, rp->renderpassDef);
	rp->attachment_images = images;
	size_t      memSize = 0;
	for (size_t i = 0; i < images->size; i++) {
		memSize += get_image_size (images->a[i], device);
	}
	VkDeviceMemory mem;
	mem = QFV_AllocImageMemory (device, images->a[0],
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								memSize, 0);
	rp->attachmentMemory = mem;
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 mem, "memory:framebuffers");
	size_t      offset = 0;
	for (size_t i = 0; i < images->size; i++) {
		QFV_BindImageMemory (device, images->a[i], mem, offset);
		offset += get_image_size (images->a[i], device);
	}

	item = qfv_load_renderpass (ctx, rp, "imageViews");
	if (!item) {
		return;
	}

	__auto_type views = QFV_ParseImageViewSet (ctx, item, rp->renderpassDef);
	rp->attachment_views = views;

	item = qfv_load_renderpass (ctx, rp, "framebuffer");
	if (!item) {
		return;
	}

	rp->framebuffers = QFV_AllocFrameBuffers (ctx->swapchain->numImages,
											  malloc);
	for (size_t i = 0; i < rp->framebuffers->size; i++) {
		ctx->output = (qfv_output_t) {
			.extent = ctx->swapchain->extent,
			.view   = ctx->swapchain->imageViews->a[i],
			.format = ctx->swapchain->format,
		};
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
	rFrame->subpassCount = QFV_NumPasses;
	rFrame->subpassCmdSets = malloc (QFV_NumPasses
									 * sizeof (qfv_cmdbufferset_t));
	for (int j = 0; j < QFV_NumPasses; j++) {
		DARRAY_INIT (&rFrame->subpassCmdSets[j], 4);
	}
}

void
Vulkan_CreateRenderPass (vulkan_ctx_t *ctx)
{
	const char *name = "renderpass";//FIXME
	plitem_t   *item;

	qfv_renderpass_t *rp = calloc (1, sizeof (qfv_renderpass_t));

	hashtab_t  *tab = ctx->renderpasses;
	const char *path;
	path = va (ctx->va_ctx, "$"QFV_PROPERTIES".%s", name);
	__auto_type renderpass = (VkRenderPass) QFV_GetHandle (tab, path);
	if (renderpass) {
		rp->renderpass = renderpass;
	} else {
		ctx->output = (qfv_output_t) {
			.extent = ctx->swapchain->extent,
			.view   = ctx->swapchain->imageViews->a[0],
			.format = ctx->swapchain->format,
		};
		item = qfv_load_renderpass (ctx, rp, name);
		rp->renderpass = QFV_ParseRenderPass (ctx, item, rp->renderpassDef);
		QFV_AddHandle (tab, path, (uint64_t) rp->renderpass);
		QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_RENDER_PASS,
							 rp->renderpass, va (ctx->va_ctx, "renderpass:%s",
												name));
	}

	int         width = ctx->window_width;
	int         height = ctx->window_height;
	rp->viewport = (VkViewport) { 0, 0, width, height, 0, 1 };
	rp->scissor = (VkRect2D) { {0, 0}, {width, height} };

	DARRAY_INIT (&rp->frames, 4);
	DARRAY_RESIZE (&rp->frames, ctx->frames.size);
	for (size_t i = 0; i < rp->frames.size; i++) {
		init_renderframe (ctx, rp, &rp->frames.a[i]);
	}

	create_attachements (ctx, rp);

	item = qfv_load_renderpass (ctx, rp, "clearValues");
	rp->clearValues = QFV_ParseClearValues (ctx, item, rp->renderpassDef);

	rp->draw = renderpass_draw;

	DARRAY_APPEND (&ctx->renderPasses, rp);
}

static void
destroy_attachments (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < rp->attachment_views->size; i++) {
		dfunc->vkDestroyImageView (device->dev, rp->attachment_views->a[i], 0);
	}
	for (size_t i = 0; i < rp->attachment_images->size; i++) {
		dfunc->vkDestroyImage (device->dev, rp->attachment_images->a[i], 0);
	}
	dfunc->vkFreeMemory (device->dev, rp->attachmentMemory, 0);
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

static void
destroy_framebuffers (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < rp->framebuffers->size; i++) {
		dfunc->vkDestroyFramebuffer (device->dev, rp->framebuffers->a[i], 0);
	}
	free (rp->framebuffers);
}

void
Vulkan_DestroyRenderPasses (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	for (size_t i = 0; i < ctx->renderPasses.size; i++) {
		__auto_type rp = ctx->renderPasses.a[i];

		PL_Free (rp->renderpassDef);

		destroy_attachments (ctx, rp);
		dfunc->vkDestroyRenderPass (device->dev, rp->renderpass, 0);
		destroy_renderframes (ctx, rp);
		destroy_framebuffers (ctx, rp);

		free (rp);
	}
}

VkPipeline
Vulkan_CreateComputePipeline (vulkan_ctx_t *ctx, const char *name)
{
	plitem_t   *item = qfv_load_pipeline (ctx, "pipelines");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline def %s\n", name);
	}
	VkPipeline pipeline = QFV_ParseComputePipeline (ctx, item,
													 ctx->pipelineDef);
	QFV_duSetObjectName (ctx->device, VK_OBJECT_TYPE_PIPELINE, pipeline,
						 va (ctx->va_ctx, "pipeline:%s", name));
	return pipeline;
}

VkPipeline
Vulkan_CreateGraphicsPipeline (vulkan_ctx_t *ctx, const char *name)
{
	plitem_t   *item = qfv_load_pipeline (ctx, "pipelines");
	if (!(item = PL_ObjectForKey (item, name))) {
		Sys_Printf ("error loading pipeline %s\n", name);
		return 0;
	} else {
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline def %s\n", name);
	}
	VkPipeline pipeline = QFV_ParseGraphicsPipeline (ctx, item,
													 ctx->pipelineDef);
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
		Sys_MaskPrintf (SYS_vulkan_parse, "Found descriptor pool def %s\n",
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
		Sys_MaskPrintf (SYS_vulkan_parse, "Found pipeline layout def %s\n",
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
		Sys_MaskPrintf (SYS_vulkan_parse, "Found sampler def %s\n", name);
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
		Sys_MaskPrintf (SYS_vulkan_parse, "Found descriptor set def %s\n",
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

	DARRAY_RESIZE (&ctx->frames, vulkan_frame_count);

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
		frame->framebuffer = 0;
	}

	DARRAY_CLEAR (&ctx->frames);
}
