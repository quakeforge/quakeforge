/*
	render.c

	Vulkan render manager

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/swapchain.h"
#include "vid_vulkan.h"

#include "vkparse.h"

VkCommandBuffer
QFV_GetCmdBuffer (vulkan_ctx_t *ctx, bool secondary)
{
	auto rctx = ctx->render_context;
	auto rframe = &rctx->frames.a[ctx->curFrame];
	return QFV_CmdPoolManager_CmdBuffer (&rframe->cmdpool, secondary);
}

void
QFV_AppendCmdBuffer (vulkan_ctx_t *ctx, VkCommandBuffer cmd)
{
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;
	DARRAY_APPEND (&job->commands, cmd);
}

static void
update_time (qfv_time_t *time, int64_t start, int64_t end)
{
	int64_t delta = end - start;
	time->cur_time = delta;
	time->min_time = min (time->min_time, delta);
	time->max_time = max (time->max_time, delta);
}

static void
renderpass_update_viewport_sissor (qfv_renderpass_t *rp,
								   const qfv_output_t *output)
{
	rp->beginInfo.renderArea.extent = output->extent;
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		auto sp = &rp->subpasses[i];
		for (uint32_t j = 0; j < sp->pipeline_count; j++) {
			auto pl = &sp->pipelines[j];
			pl->viewport = (VkViewport) {
				.width = output->extent.width,
				.height = output->extent.height,
				.minDepth = 0,
				.maxDepth = 1,
			};
			pl->scissor.extent = output->extent;
		}
	}
}

static void
update_viewport_scissor (qfv_render_t *render, const qfv_output_t *output)
{
	for (uint32_t i = 0; i < render->num_renderpasses; i++) {
		renderpass_update_viewport_sissor (&render->renderpasses[i], output);
	}
}

static void
run_tasks (uint32_t task_count, qfv_taskinfo_t *tasks, qfv_taskctx_t *ctx)
{
	for (uint32_t i = 0; i < task_count; i++) {
		tasks[i].func->func (tasks[i].params, 0, (exprctx_t *) ctx);
	}
}

static void
run_pipeline (qfv_pipeline_t *pipeline, qfv_taskctx_t *taskctx)
{
	if (pipeline->disabled) {
		return;
	}
	qfv_device_t *device = taskctx->ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto cmd = taskctx->cmd;
	dfunc->vkCmdBindPipeline (cmd, pipeline->bindPoint, pipeline->pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &pipeline->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &pipeline->scissor);

	taskctx->pipeline = pipeline;
	run_tasks (pipeline->task_count, pipeline->tasks, taskctx);
}

// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
static void
run_subpass (qfv_subpass_t *sp, qfv_taskctx_t *taskctx)
{
	qfv_device_t *device = taskctx->ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkBeginCommandBuffer (taskctx->cmd, &sp->beginInfo);

	{
		qftVkScopedZone (taskctx->frame->qftVkCtx, taskctx->cmd, "subpass");

		for (uint32_t i = 0; i < sp->pipeline_count; i++) {
			__auto_type pipeline = &sp->pipelines[i];
			run_pipeline (pipeline, taskctx);
		}
	}

	dfunc->vkEndCommandBuffer (taskctx->cmd);
}

void
QFV_RunRenderPassCmd (VkCommandBuffer cmd, vulkan_ctx_t *ctx,
					  qfv_renderpass_t *rp, void *data)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];

	qfZoneNamed (zone, true);
	qftVkScopedZone (frame->qftVkCtx, cmd, "renderpass");

	QFV_duCmdBeginLabel (device, cmd, rp->label.name,
						 {VEC4_EXP (rp->label.color)});
	dfunc->vkCmdBeginRenderPass (cmd, &rp->beginInfo, rp->subpassContents);
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		QFV_duCmdBeginLabel (device, cmd, sp->label.name,
							 {VEC4_EXP (sp->label.color)});
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
			.frame = frame,
			.renderpass = rp,
			.cmd = QFV_GetCmdBuffer (ctx, true),
			.data = data,
		};
		run_subpass (sp, &taskctx);
		dfunc->vkCmdExecuteCommands (cmd, 1, &taskctx.cmd);
		QFV_duCmdEndLabel (device, cmd);
		//FIXME comment is a bit off as exactly one buffer is always
		//submitted
		//
		//Regardless of whether any commands were submitted for this
		//subpass, must step through each and every subpass, otherwise
		//the attachments won't be transitioned correctly.
		//However, only if not the last (or only) subpass.
		if (i < rp->subpass_count - 1) {
			dfunc->vkCmdNextSubpass (cmd, rp->subpassContents);
		}
	}

	dfunc->vkCmdEndRenderPass (cmd);
	QFV_CmdEndLabel (device, cmd);
}

static void
run_renderpass (qfv_renderpass_t *rp, vulkan_ctx_t *ctx, void *data)
{
	qfZoneNamed (zone, true);
	qfZoneName (zone, rp->label.name, rp->label.name_len);
	qfZoneColor (zone, rp->label.color32);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto job = rctx->job;

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 va (ctx->va_ctx, "cmd:render:%s", rp->label.name));
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_RunRenderPassCmd (cmd, ctx, rp, data);

	dfunc->vkEndCommandBuffer (cmd);
	DARRAY_APPEND (&job->commands, cmd);
}

static void
run_compute_pipeline (qfv_pipeline_t *pipeline, VkCommandBuffer cmd,
					  vulkan_ctx_t *ctx)
{
	if (pipeline->disabled) {
		return;
	}
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];
	qftVkScopedZone (frame->qftVkCtx, cmd, "compute");
	dfunc->vkCmdBindPipeline (cmd, pipeline->bindPoint, pipeline->pipeline);

	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.frame = frame,
		.pipeline = pipeline,
		.cmd = cmd,
	};
	run_tasks (pipeline->task_count, pipeline->tasks, &taskctx);

	vec4u_t     d = pipeline->dispatch;
	if (d[0] && d[1] && d[2]) {
		dfunc->vkCmdDispatch (cmd, d[0], d[1], d[2]);
	}
}

static void
run_compute (qfv_compute_t *comp, vulkan_ctx_t *ctx, qfv_step_t *step)
{
	qfZoneNamed (zone, true);
	qfZoneName (zone, step->label.name, step->label.name_len);
	qfZoneColor (zone, step->label.color32);
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 va (ctx->va_ctx, "cmd:compute:%s", step->label.name));
	QFV_duCmdBeginLabel (device, cmd, step->label.name,
						 {VEC4_EXP (step->label.color)});

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	for (uint32_t i = 0; i < comp->pipeline_count; i++) {
		__auto_type pipeline = &comp->pipelines[i];
		run_compute_pipeline (pipeline, cmd, ctx);
	}
	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
	DARRAY_APPEND (&job->commands, cmd);
}

static void
run_process (qfv_process_t *proc, vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];
	qfZoneNamed (zone, true);
	qfZoneName (zone, proc->label.name, proc->label.name_len);
	qfZoneColor (zone, proc->label.color32);
	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.frame = frame,
	};
	run_tasks (proc->task_count, proc->tasks, &taskctx);
}

static void
run_collect (vulkan_ctx_t *ctx)
{
#ifdef TRACY_ENABLE
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 va (ctx->va_ctx, "cmd:render:%s", "tracy"));
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	qftCVkCollect (frame->qftVkCtx, cmd);
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);
#endif
}

void
QFV_RunRenderPass (vulkan_ctx_t *ctx, qfv_renderpass_t *renderpass,
				   uint32_t width, uint32_t height, void *data)
{
	qfZoneNamed (zone, true);
	qfv_output_t output = {
		.extent = {
			.width = width,
			.height = height,
		},
	};
	renderpass_update_viewport_sissor (renderpass, &output);
	run_renderpass (renderpass, ctx, data);
}

void
QFV_RunRenderJob (vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	auto rctx = ctx->render_context;
	auto job = rctx->job;
	int64_t start = Sys_LongTime ();

	for (uint32_t i = 0; i < job->num_steps; i++) {
		int64_t step_start = Sys_LongTime ();
		__auto_type step = &job->steps[i];
		if (!step->process) {
			// run render and compute steps automatically only if there's no
			// process for the step (the idea is the process uses the compute
			// and renderpass objects for its own purposes).
			if (step->render) {
				run_renderpass (step->render->active, ctx, 0);
			}
			if (step->compute) {
				run_compute (step->compute, ctx, step);
			}
		}
		if (step->process) {
			run_process (step->process, ctx);
		}
		update_time (&step->time, step_start, Sys_LongTime ());
	}

	qfMessageL ("submit");
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto queue = &device->queue;
	auto frame = &rctx->frames.a[ctx->curFrame];
	VkPipelineStageFlags waitStage
		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, 0,
		1, &frame->imageAvailableSemaphore, &waitStage,
		job->commands.size, job->commands.a,
		1, &frame->renderDoneSemaphore,
	};
	dfunc->vkResetFences (device->dev, 1, &frame->fence);
	dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo, frame->fence);

	qfMessageL ("present");
	VkPresentInfoKHR presentInfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, 0,
		1, &frame->renderDoneSemaphore,
		1, &ctx->swapchain->swapchain, &ctx->swapImageIndex,
		0
	};
	dfunc->vkQueuePresentKHR (queue->queue, &presentInfo);

	qfMessageL ("update_time");
	if (++ctx->curFrame >= rctx->frames.size) {
		ctx->curFrame = 0;
	}
	update_time (&job->time, start, Sys_LongTime ());
}

static qfv_imageviewinfo_t * __attribute__((pure))
find_imageview (qfv_reference_t *ref, qfv_renderpass_t *rp,
				qfv_renderctx_t *rctx)
{
	auto jinfo = rctx->jobinfo;
	const char *name = ref->name;

	if (strncmp (name, "$imageviews.", 7) == 0) {
		name += 7;
	}

	for (uint32_t i = 0; i < jinfo->num_imageviews; i++) {
		auto viewinfo = &jinfo->imageviews[i];
		if (strcmp (name, viewinfo->name) == 0) {
			return viewinfo;
		}
	}
	Sys_Error ("%d:invalid imageview: %s", ref->line, ref->name);
}

void
QFV_DestroyFramebuffer (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	if (rp->beginInfo.framebuffer) {
		auto device = ctx->device;
		auto dfunc = device->funcs;
		auto bi = &rp->beginInfo;
		dfunc->vkDestroyFramebuffer (device->dev, bi->framebuffer, 0);
		bi->framebuffer = 0;
	}
	if (rp->resources && rp->resources->memory) {
		QFV_DestroyResource (ctx->device, rp->resources);
	}
}

void
QFV_CreateFramebuffer (vulkan_ctx_t *ctx, qfv_renderpass_t *rp,
					   VkExtent2D extent)
{
	auto rctx = ctx->render_context;

	if (rp->resources && !rp->resources->memory) {
		for (uint32_t i = 0; i < rp->resources->num_objects; i++) {
			auto obj = &rp->resources->objects[i];
			if (obj->type == qfv_res_image) {
				obj->image.extent.width = extent.width;
				obj->image.extent.height = extent.height;
			}
		}
		QFV_CreateResource (ctx->device, rp->resources);
	}

	auto fb = rp->framebufferinfo;
	auto attachments = rp->framebuffer.views;
	VkFramebufferCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.attachmentCount = fb->num_attachments,
		.pAttachments = attachments,
		.renderPass = rp->beginInfo.renderPass,
		.width = extent.width,
		.height = extent.height,
		.layers = fb->layers,
	};
	for (uint32_t i = 0; i < fb->num_attachments; i++) {
		if (fb->attachments[i].external) {
			attachments[i] = 0;
			if (!strcmp (fb->attachments[i].external, "$swapchain")) {
				auto sc = ctx->swapchain;
				attachments[i] = sc->imageViews->a[ctx->swapImageIndex];
				cInfo.width = sc->extent.width;
				cInfo.height = sc->extent.height;
			}
		} else {
			auto viewinfo = find_imageview (&fb->attachments[i].view, rp, rctx);
			attachments[i] = viewinfo->object->image_view.view;
			if (rp->outputref.name) {
				viewinfo = find_imageview (&rp->outputref, rp, rctx);
				rp->output = viewinfo->object->image_view.view;
			}
		}
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_FRAMEBUFFER, framebuffer,
						 va (ctx->va_ctx, "framebuffer:%s", rp->label.name));

	rp->beginInfo.framebuffer = framebuffer;
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		sp->inherit.framebuffer = framebuffer;
	}
}

static void
wait_on_fence (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto dev = device->dev;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];

	dfunc->vkWaitForFences (dev, 1, &frame->fence, VK_TRUE, 2000000000);

	QFV_CmdPoolManager_Reset (&frame->cmdpool);
	auto job = ctx->render_context->job;
	DARRAY_RESIZE (&job->commands, 0);
	run_collect (ctx);
}

static void
update_framebuffer (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto job = ctx->render_context->job;
	auto step = QFV_GetStep (params[0], job);
	auto render = step->render;
	auto rp = render->active;

	qfv_output_t output = {};
	Vulkan_ConfigOutput (ctx, &output);
	if ((output.extent.width != render->output.extent.width
		|| output.extent.height != render->output.extent.height)
		&& (Sys_LongTime () - ctx->render_context->size_time) > 2*1000*1000) {
		QFV_DestroyFramebuffer (ctx, rp);
		update_viewport_scissor (render, &output);
		render->output.extent = output.extent;
	}

	if (!rp->beginInfo.framebuffer) {
		QFV_CreateFramebuffer (ctx, rp, render->output.extent);
	}
}

static exprfunc_t wait_on_fence_func[] = {
	{ .func = wait_on_fence },
	{}
};

static exprtype_t *update_framebuffer_params[] = {
	&cexpr_string,
};
static exprfunc_t update_framebuffer_func[] = {
	{ .func = update_framebuffer, .num_params = 1, update_framebuffer_params },
	{}
};
static exprsym_t render_task_syms[] = {
	{ "wait_on_fence", &cexpr_function, wait_on_fence_func },
	{ "update_framebuffer", &cexpr_function, update_framebuffer_func },
	{}
};

void
QFV_Render_Init (vulkan_ctx_t *ctx)
{
	qfv_renderctx_t *rctx = calloc (1, sizeof (*rctx));
	ctx->render_context = rctx;
	rctx->size_time = -1000*1000*1000;

	exprctx_t   ectx = { .hashctx = &rctx->hashctx };
	exprsym_t   syms[] = { {} };
	rctx->task_functions.symbols = syms;
	cexpr_init_symtab (&rctx->task_functions, &ectx);
	rctx->task_functions.symbols = 0;

	rctx->external_attachments =
		(qfv_attachmentinfoset_t) DARRAY_STATIC_INIT (4);

	QFV_Render_AddTasks (ctx, render_task_syms);

	auto device = ctx->device;
	size_t      frames = vulkan_frame_count;
	DARRAY_INIT (&rctx->frames, frames);
	DARRAY_RESIZE (&rctx->frames, frames);
	for (size_t i = 0; i < rctx->frames.size; i++) {
		auto frame = &rctx->frames.a[i];
		frame->fence = QFV_CreateFence (device, 1);
		frame->imageAvailableSemaphore = QFV_CreateSemaphore (device);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_SEMAPHORE,
							 frame->imageAvailableSemaphore,
							 va (ctx->va_ctx, "sc image:%zd", i));
		frame->renderDoneSemaphore = QFV_CreateSemaphore (device);
		QFV_CmdPoolManager_Init (&frame->cmdpool, device,
								 va (ctx->va_ctx, "render pool:%zd", i));
#ifdef TRACY_ENABLE
		auto instance = ctx->instance->instance;
		auto physdev = ctx->device->physDev->dev;
		auto gipa = ctx->vkGetInstanceProcAddr;
		auto gdpa = ctx->instance->funcs->vkGetDeviceProcAddr;
		frame->qftVkCtx = qftCVkContextHostCalibrated (instance, physdev,
													   device->dev, gipa, gdpa);
#endif
	}
}

void
QFV_Render_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	if (rctx->job) {
		__auto_type job = rctx->job;
		for (uint32_t i = 0; i < job->num_renderpasses; i++) {
			dfunc->vkDestroyRenderPass (device->dev, job->renderpasses[i], 0);
		}
		for (uint32_t i = 0; i < job->num_pipelines; i++) {
			dfunc->vkDestroyPipeline (device->dev, job->pipelines[i], 0);
		}
		for (uint32_t i = 0; i < job->num_layouts; i++) {
			dfunc->vkDestroyPipelineLayout (device->dev, job->layouts[i], 0);
		}
		for (uint32_t i = 0; i < job->num_steps; i++) {
			if (job->steps[i].render) {
				auto render = job->steps[i].render;
				for (uint32_t j = 0; j < render->num_renderpasses; j++) {
					auto rp = &render->renderpasses[j];
					if (rp->resources && rp->resources->memory) {
						QFV_DestroyResource (ctx->device, rp->resources);
					}
					free (rp->resources);
					auto bi = &rp->beginInfo;
					if (bi->framebuffer) {
						dfunc->vkDestroyFramebuffer (device->dev,
													 bi->framebuffer, 0);
					}
				}
			}
		}
		DARRAY_CLEAR (&job->commands);
		for (uint32_t i = 0; i < job->num_dsmanagers; i++) {
			QFV_DSManager_Destroy (job->dsmanager[i]);
		}
		free (rctx->job);
	}

	for (uint32_t i = 0; i < rctx->frames.size; i++) {
		auto dev = device->dev;
		auto df = dfunc;
		auto frame = &rctx->frames.a[i];
		df->vkDestroyFence (dev, frame->fence, 0);
		df->vkDestroySemaphore (dev, frame->imageAvailableSemaphore, 0);
		df->vkDestroySemaphore (dev, frame->renderDoneSemaphore, 0);
		QFV_CmdPoolManager_Shutdown (&frame->cmdpool);
	}
	DARRAY_CLEAR (&rctx->frames);

	if (rctx->jobinfo) {
		__auto_type jinfo = rctx->jobinfo;
		for (uint32_t i = 0; i < jinfo->num_dslayouts; i++) {
			__auto_type setLayout = jinfo->dslayouts[i].setLayout;
			dfunc->vkDestroyDescriptorSetLayout (device->dev, setLayout, 0);
		}
		delete_memsuper (jinfo->memsuper);
	}
	if (rctx->task_functions.tab) {
		Hash_DelTable (rctx->task_functions.tab);
	}
	DARRAY_CLEAR (&rctx->external_attachments);
	if (rctx->samplerinfo) {
		auto si = rctx->samplerinfo;
		for (uint32_t i = 0; i < si->num_samplers; i++) {
			auto sci = &si->samplers[i];
			if (sci->sampler) {
				dfunc->vkDestroySampler (device->dev, sci->sampler, 0);
			}
		}
	}
	Hash_DelContext (rctx->hashctx);

	QFV_Render_UI_Shutdown (ctx);
	free (rctx);
}

void
QFV_Render_AddTasks (vulkan_ctx_t *ctx, exprsym_t *task_syms)
{
	__auto_type rctx = ctx->render_context;
	exprctx_t   ectx = { .hashctx = &rctx->hashctx };
	for (exprsym_t *sym = task_syms; sym->name; sym++) {
		Hash_Add (rctx->task_functions.tab, sym);
		for (exprfunc_t *f = sym->value; f->func; f++) {
			for (int i = 0; i < f->num_params; i++) {
				exprenum_t *e = f->param_types[i]->data;
				if (e && !e->symtab->tab) {
					cexpr_init_symtab (e->symtab, &ectx);
				}
			}
		}
	}
}

void
QFV_Render_AddAttachments (vulkan_ctx_t *ctx, uint32_t num_attachments,
						   qfv_attachmentinfo_t **attachments)
{
	auto rctx = ctx->render_context;
	size_t base = rctx->external_attachments.size;
	DARRAY_RESIZE (&rctx->external_attachments, base + num_attachments);
	for (size_t i = 0; i < num_attachments; i++) {
		rctx->external_attachments.a[base + i] = attachments[i];
	}
}

qfv_resobj_t *
QFV_FindResource (const char *name, qfv_renderpass_t *rp)
{
	if (!rp->resources) {
		return 0;
	}
	for (uint32_t i = 0; i < rp->resources->num_objects; i++) {
		auto obj = &rp->resources->objects[i];
		if (!strcmp (obj->name, name)) {
			return obj;
		}
	}
	return 0;
}

qfv_step_t *
QFV_FindStep (const char *name, qfv_job_t *job)
{
	for (uint32_t i = 0; i < job->num_steps; i++) {
		auto step = &job->steps[i];
		if (!strcmp (step->label.name, name)) {
			return step;
		}
	}
	return 0;
}

qfv_step_t *
QFV_GetStep (const exprval_t *param, qfv_job_t *job)
{
	// this is a little evil, but need to update the type after resolving
	// the step name
	auto stepref = (exprval_t *) param;

	// cache the render step referenced, using the parameter type as a flag
	// for whether the caching has been performed.
	if (stepref->type == &cexpr_string) {
		if (cexpr_string.size != cexpr_voidptr.size) {
			Sys_Error ("string and voidptr incompatible sizes");
		}
		auto name = *(const char **)stepref->value;
		stepref->type = &cexpr_voidptr;
		*(qfv_step_t **)stepref->value = QFV_FindStep (name, job);
	}
	return *(qfv_step_t **)stepref->value;
}

qfv_dsmanager_t *
QFV_Render_DSManager (vulkan_ctx_t *ctx, const char *setName)
{
	auto job = ctx->render_context->job;
	for (uint32_t i = 0; i < job->num_dsmanagers; i++) {
		auto ds = job->dsmanager[i];
		if (!strcmp (ds->name, setName)) {
			return ds;
		}
	}
	return 0;
}

static void
create_sampler (vulkan_ctx_t *ctx, qfv_samplercreateinfo_t *sampler)
{
	VkSamplerCreateInfo create = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.flags = sampler->flags,
		.magFilter = sampler->magFilter,
		.minFilter = sampler->minFilter,
		.mipmapMode = sampler->mipmapMode,
		.addressModeU = sampler->addressModeU,
		.addressModeV = sampler->addressModeV,
		.addressModeW = sampler->addressModeW,
		.mipLodBias = sampler->mipLodBias,
		.anisotropyEnable = sampler->anisotropyEnable,
		.maxAnisotropy = sampler->maxAnisotropy,
		.compareEnable = sampler->compareEnable,
		.compareOp = sampler->compareOp,
		.minLod = sampler->minLod,
		.maxLod = sampler->maxLod,
		.borderColor = sampler->borderColor,
		.unnormalizedCoordinates = sampler->unnormalizedCoordinates,
	};
	auto device = ctx->device;
	auto dfunc = device->funcs;
	dfunc->vkCreateSampler (device->dev, &create, 0, &sampler->sampler);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_SAMPLER, sampler->sampler,
						 va (ctx->va_ctx, "sampler:%s", sampler->name));
}

VkSampler
QFV_Render_Sampler (vulkan_ctx_t *ctx, const char *name)
{
	auto si = ctx->render_context->samplerinfo;
	if (!si) {
		return 0;
	}
	for (uint32_t i = 0; i < si->num_samplers; i++) {
		auto sci = &si->samplers[i];
		if (!strcmp (sci->name, name)) {
			if (!sci->sampler) {
				create_sampler (ctx, sci);
			}
			return sci->sampler;
		}
	}
	printf ("sampler %s not found\n", name);
	return 0;
}
