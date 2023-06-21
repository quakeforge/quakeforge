/*
	vulkan_main.c

	Vulkan output

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/11/21

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_output.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/capture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/swapchain.h"

#include "r_local.h"
#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"//FIXME

static void
acquire_image (qfv_renderframe_t *rFrame)
{
	auto ctx = rFrame->vulkan_ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto frame = &ctx->frames.a[ctx->curFrame];

	uint32_t imageIndex = 0;
	while (!QFV_AcquireNextImage (ctx->swapchain,
								  frame->imageAvailableSemaphore,
								  0, &imageIndex)) {
		QFV_DeviceWaitIdle (device);
		if (ctx->capture) {
			QFV_DestroyCapture (ctx->capture);
		}
		Vulkan_CreateSwapchain (ctx);
		Vulkan_CreateCapture (ctx);

		__auto_type out = ctx->output_renderpass;
		out->output = (qfv_output_t) {
			.extent    = ctx->swapchain->extent,
			.format    = ctx->swapchain->format,
			.frames    = ctx->swapchain->numImages,
			.view_list = ctx->swapchain->imageViews->a,
		};
		out->viewport.width = out->output.extent.width;
		out->viewport.height = out->output.extent.height;
		out->scissor.extent = out->output.extent;
		QFV_RenderPass_CreateFramebuffer (out);

		dfunc->vkDestroySemaphore (device->dev, frame->imageAvailableSemaphore,
								   0);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_SEMAPHORE,
							 frame->imageAvailableSemaphore,
							 va (ctx->va_ctx, "sc image:%d", ctx->curFrame));
	}
	ctx->swapImageIndex = imageIndex;
}

static void
output_update_input (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	outputctx_t *octx = ctx->output_context;
	uint32_t    curFrame = ctx->curFrame;
	outputframe_t *oframe = &octx->frames.a[curFrame];

	if (oframe->input == octx->input) {
		return;
	}
	oframe->input = octx->input;

	VkDescriptorImageInfo imageInfo = {
		octx->sampler, oframe->input,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  oframe->set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 }
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
}

static void
preoutput_draw (qfv_renderframe_t *rFrame)
{
	acquire_image (rFrame);
	output_update_input (rFrame);
}

static void
process_input (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	outputctx_t  *octx = ctx->output_context;
	outputframe_t *oframe = &octx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = oframe->cmd;

	DARRAY_APPEND (&rFrame->subpassCmdSets[0], cmd);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, 0,
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, "output:output");

	__auto_type pipeline = octx->output;
	__auto_type layout = octx->output_layout;
	if (scr_fisheye) {
		pipeline = octx->fisheye;
		layout = octx->fish_layout;
	} else if (r_dowarp) {
		pipeline = octx->waterwarp;
		layout = octx->warp_layout;
	}
	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	VkDescriptorSet set[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		oframe->set,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, set, 0, 0);
	if (scr_fisheye) {
		float       width = r_refdef.vrect.width;
		float       height = r_refdef.vrect.height;

		float       ffov = scr_ffov * M_PI / 360;
		float       aspect = height / width;
		qfv_push_constants_t push_constants[] = {
			{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (float), &ffov },
			{ VK_SHADER_STAGE_FRAGMENT_BIT, 4, sizeof (float), &aspect },
		};
		QFV_PushConstants (device, cmd, layout, 2, push_constants);
	} else if (r_dowarp) {
		float       time = vr_data.realtime;
		qfv_push_constants_t push_constants[] = {
			{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (float), &time },
		};
		QFV_PushConstants (device, cmd, layout, 1, push_constants);
	}

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);


	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);

}

static void
draw_output (qfv_renderframe_t *rFrame)
{
	process_input (rFrame);
	Vulkan_FlushText (rFrame);
}

void
Vulkan_Output_CreateRenderPasses (vulkan_ctx_t *ctx)
{
	outputctx_t *octx = calloc (1, sizeof (outputctx_t));
	ctx->output_context = octx;

	__auto_type out = QFV_RenderPass_New (ctx, "output", draw_output);
	out->output = (qfv_output_t) {
		.extent    = ctx->swapchain->extent,
		.format    = ctx->swapchain->format,
		.frames    = ctx->swapchain->numImages,
		.view_list = ctx->swapchain->imageViews->a,
	};
	QFV_RenderPass_CreateRenderPass (out);
	QFV_RenderPass_CreateFramebuffer (out);
	ctx->output_renderpass = out;


	out->order = QFV_rp_output;
	DARRAY_APPEND (&ctx->renderPasses, out);

	__auto_type pre = QFV_RenderPass_New (ctx, "preoutput", preoutput_draw);
	pre->order = QFV_rp_preoutput;
	DARRAY_APPEND (&ctx->renderPasses, pre);
}

static void
acquire_output (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto frame = &ctx->frames.a[ctx->curFrame];
	auto octx = ctx->output_context;
	auto sc = ctx->swapchain;

	printf ("acquire_output: %d\n", ctx->curFrame);
	uint32_t imageIndex = 0;
	while (!QFV_AcquireNextImage (sc, frame->imageAvailableSemaphore,
								  0, &imageIndex)) {
		QFV_DeviceWaitIdle (device);
		if (ctx->capture) {
			QFV_DestroyCapture (ctx->capture);
		}
		for (uint32_t i = 0; i < sc->imageViews->size; i++) {
			dfunc->vkDestroyFramebuffer (device->dev,
										 octx->framebuffers[i], 0);
		}
		octx->framebuffers = 0;
		Vulkan_CreateSwapchain (ctx);
		sc = ctx->swapchain;
		Vulkan_CreateCapture (ctx);

		__auto_type out = ctx->output_renderpass;
		out->output = (qfv_output_t) {
			.extent    = ctx->swapchain->extent,
			.format    = ctx->swapchain->format,
			.frames    = ctx->swapchain->numImages,
			.view_list = ctx->swapchain->imageViews->a,
		};
		out->viewport.width = out->output.extent.width;
		out->viewport.height = out->output.extent.height;
		out->scissor.extent = out->output.extent;

		dfunc->vkDestroySemaphore (device->dev, frame->imageAvailableSemaphore,
								   0);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_SEMAPHORE,
							 frame->imageAvailableSemaphore,
							 va (ctx->va_ctx, "sc image:%d", ctx->curFrame));
	}

	//FIXME clean this up
	auto step = QFV_GetStep (params[0], ctx->render_context->job);
	auto render = step->render;
	auto rp = &render->renderpasses[0];
	if (!octx->framebuffers) {
		uint32_t    count = ctx->swapchain->imageViews->size;
		octx->framebuffers = malloc (sizeof (VkFramebuffer [count]));
		for (uint32_t i = 0; i < count; i++) {
			rp->beginInfo.framebuffer = 0;
			//FIXME come up with a better mechanism
			ctx->swapImageIndex = i;
			QFV_CreateFramebuffer (ctx, rp);
			octx->framebuffers[i] = rp->beginInfo.framebuffer;
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_FRAMEBUFFER,
								 octx->framebuffers[i],
								 va (ctx->va_ctx, "sc fb:%d", i));
		}
		for (uint32_t i = 0; i < rp->subpass_count; i++) {
			auto sp = &rp->subpasses[i];
			for (uint32_t j = 0; j < sp->pipeline_count; j++) {
				auto pl = &sp->pipelines[j];
				pl->viewport.width = sc->extent.width;
				pl->viewport.height = sc->extent.height;
				pl->scissor.extent = sc->extent;
			}
		}
	}
	ctx->swapImageIndex = imageIndex;
	rp->beginInfo.framebuffer = octx->framebuffers[imageIndex];
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		auto sp = &rp->subpasses[i];
		sp->inherit.framebuffer = rp->beginInfo.framebuffer;
	}
}

static void
update_input (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto octx = ctx->output_context;
	auto oframe = &octx->frames.a[ctx->curFrame];

	if (oframe->input == octx->input) {
		return;
	}
	oframe->input = octx->input;

	VkDescriptorImageInfo imageInfo = {
		octx->sampler, oframe->input,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet write[] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
		  oframe->set, 0, 0, 1,
		  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		  &imageInfo, 0, 0 }
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
}

static void
output_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
}

static exprtype_t *stepref_param[] = {
	&cexpr_string,
};
static exprfunc_t acquire_output_func[] = {
	{ .func = acquire_output, .num_params = 1, stepref_param },
	{}
};
static exprfunc_t update_input_func[] = {
	{ .func = update_input, .num_params = 1, stepref_param },
	{}
};
static exprfunc_t output_draw_func[] = {
	{ .func = output_draw },
	{}
};
static exprsym_t output_task_syms[] = {
	{ "acquire_output", &cexpr_function, acquire_output_func },
	{ "update_input", &cexpr_function, update_input_func },
	{ "output_draw", &cexpr_function, output_draw_func },
	{}
};

void
Vulkan_Output_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "output init");
	QFV_Render_AddTasks (ctx, output_task_syms);

	outputctx_t *octx = ctx->output_context;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&octx->frames, frames);
	DARRAY_RESIZE (&octx->frames, frames);
	octx->frames.grow = 0;

	__auto_type pld = ctx->script_context->pipelineDef;//FIXME
	ctx->script_context->pipelineDef = Vulkan_GetConfig (ctx, "qf_output");

	octx->output = Vulkan_CreateGraphicsPipeline (ctx, "output");
	octx->waterwarp = Vulkan_CreateGraphicsPipeline (ctx, "waterwarp");
	octx->fisheye = Vulkan_CreateGraphicsPipeline (ctx, "fisheye");
	octx->output_layout = Vulkan_CreatePipelineLayout (ctx, "output_layout");
	octx->warp_layout = Vulkan_CreatePipelineLayout (ctx, "waterwarp_layout");
	octx->fish_layout = Vulkan_CreatePipelineLayout (ctx, "fisheye_layout");
	octx->sampler = Vulkan_CreateSampler (ctx, "linear");

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	layouts->a[0] = Vulkan_CreateDescriptorSetLayout (ctx, "output_set");
	for (size_t i = 0; i < frames; i++) {
		layouts->a[i] = layouts->a[0];
	}
	__auto_type pool = Vulkan_CreateDescriptorPool (ctx, "output_pool");
	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	__auto_type cmdSet = QFV_AllocCommandBufferSet (1, alloca);

	for (size_t i = 0; i < frames; i++) {
		__auto_type oframe = &octx->frames.a[i];

		oframe->input = 0;
		oframe->set = sets->a[i];

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdSet);
		oframe->cmd = cmdSet->a[0];
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 oframe->cmd, "cmd:output");
	}

	ctx->script_context->pipelineDef = pld;

	free (sets);
	qfvPopDebug (ctx);
}

void
Vulkan_Output_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	outputctx_t *octx = ctx->output_context;

	for (uint32_t i = 0; i < ctx->swapchain->imageViews->size; i++) {
		dfunc->vkDestroyFramebuffer (device->dev, octx->framebuffers[i], 0);
	}
	free (octx->framebuffers);
	auto step = QFV_FindStep ("output", ctx->render_context->job);
	auto render = step->render;
	auto rp = &render->renderpasses[0];
	rp->beginInfo.framebuffer = 0;

	dfunc->vkDestroyPipeline (device->dev, octx->output, 0);
	dfunc->vkDestroyPipeline (device->dev, octx->waterwarp, 0);
	dfunc->vkDestroyPipeline (device->dev, octx->fisheye, 0);
	free (octx->frames.a);
	free (octx);
}

void
Vulkan_Output_SetInput (vulkan_ctx_t *ctx, VkImageView input)
{
	outputctx_t *octx = ctx->output_context;
	octx->input = input;
}
