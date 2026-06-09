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

#define IMPLEMENT_QFV_REND_Funcs

#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"
#include "QF/Vulkan/swapchain.h"

#include "r_cvar.h"
#include "vid_vulkan.h"

#include "vkparse.h"

VkCommandBuffer
QFV_GetCmdBuffer (vulkan_ctx_t *ctx, bool secondary)
{
	auto rctx = ctx->render_context;
	auto rframe = &rctx->frames.a[ctx->curFrame];
	return QFV_CmdPoolManager_CmdBuffer (rframe->active_pool, secondary);
}

void
QFV_AppendCmdBuffer (vulkan_ctx_t *ctx, VkCommandBuffer cmd)
{
	__auto_type rctx = ctx->render_context;
	__auto_type graph = rctx->graph;
	DARRAY_APPEND (&graph->commands, cmd);
}

void
QFV_SetDescriptorSet (vulkan_ctx_t *ctx, uint32_t frame,
					  uint32_t ds_index, VkDescriptorSet set)
{
	auto rctx = ctx->render_context;
	if (frame == ~0u) {
		for (size_t i = 0; i < rctx->frames.size; i++) {
			auto rframe = &rctx->frames.a[i];
			rframe->descriptor_sets[ds_index] = set;
		}
	} else {
		auto rframe = &rctx->frames.a[frame];
		rframe->descriptor_sets[ds_index] = set;
	}
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

void
QFV_UpdateViewportScissor (qfv_render_t *render, const qfv_output_t *output)
{
	for (uint32_t i = 0; i < render->num_renderpasses; i++) {
		renderpass_update_viewport_sissor (&render->renderpasses[i], output);
	}
}

static void
run_tasks (uint32_t task_count, qfv_taskinfo_t *tasks, qfv_taskctx_t *ctx)
{
	auto device = ctx->ctx->device;
	for (uint32_t i = 0; i < task_count; i++) {
		if (ctx->cmd) {
			QFV_duCmdBeginLabel (device, ctx->cmd, tasks[i].name,
								 {0.2, 0.8, 0.3, 1});
		}
		tasks[i].func->func (tasks[i].params, 0, (exprctx_t *) ctx);
		if (ctx->cmd) {
			QFV_duCmdEndLabel (device, ctx->cmd);
		}
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

	if (sp->num_inputs) {
		auto ctx = taskctx->ctx;
		auto rctx = ctx->render_context;
		auto rframe = &rctx->frames.a[ctx->curFrame];
		auto input = &rframe->subpass_inputs[sp->frame_index];
		auto fb = &taskctx->renderpass->framebuffer;
		if (fb->update_frame != input->update_frame) {
			input->update_frame = fb->update_frame;
			VkDescriptorImageInfo image_info[sp->num_inputs];
			VkWriteDescriptorSet image_write[sp->num_inputs];
			for (uint32_t i = 0; i < sp->num_inputs; i++) {
				image_info[i] = (VkDescriptorImageInfo) {
					.imageView = fb->views[sp->input_indices[i]],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
				image_write[i] = (VkWriteDescriptorSet) {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = input->set,
					.dstBinding = i,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
					.pImageInfo = &image_info[i],
				};
			}
			dfunc->vkUpdateDescriptorSets (device->dev, sp->num_inputs,
										   image_write, 0, 0);
		}
	}
	dfunc->vkBeginCommandBuffer (taskctx->cmd, &sp->beginInfo);
	QFV_duCmdBeginLabel (device, taskctx->cmd, sp->label.name,
						 {VEC4_EXP (sp->label.color)});
	{
		qftVkScopedZone (taskctx->frame->qftVkCtx, taskctx->cmd, "subpass");

		for (uint32_t i = 0; i < sp->pipeline_count; i++) {
			__auto_type pipeline = &sp->pipelines[i];
			QFV_duCmdBeginLabel (device, taskctx->cmd, pipeline->label.name,
								 {VEC4_EXP (pipeline->label.color)});
			run_pipeline (pipeline, taskctx);
			QFV_duCmdEndLabel (device, taskctx->cmd);
		}
	}

	QFV_duCmdEndLabel (device, taskctx->cmd);

	dfunc->vkEndCommandBuffer (taskctx->cmd);
}

void
QFV_RunRenderPassCmd (VkCommandBuffer cmd, qfv_taskctx_t *taskctx,
					  qfv_renderpass_t *rp)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];

	qfZoneNamed (zone, true);
	qftVkScopedZone (frame->qftVkCtx, cmd, "renderpass");

	QFV_duCmdBeginLabel (device, cmd, rp->label.name,
						 {VEC4_EXP (rp->label.color)});
	dfunc->vkCmdBeginRenderPass (cmd, &rp->beginInfo, rp->subpassContents);
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		auto sp = &rp->subpasses[i];
		qfv_taskctx_t tctx = {
			.ctx = taskctx->ctx,
			.frame = taskctx->frame,
			.renderpass = rp,
			.subpass = sp,
			.cmd = QFV_GetCmdBuffer (ctx, true),
			.data = taskctx->data,
		};
		sp->call_count = 0;
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, tctx.cmd,
							 vac (ctx->va_ctx, "renderpass:%s:%s:%" PRIu64,
								  rp->label.name, sp->label.name,
								  ctx->frameNumber));
		run_subpass (sp, &tctx);
		dfunc->vkCmdExecuteCommands (cmd, 1, &tctx.cmd);
		//printf ("subpass calls:%s:%s:%d\n", rp->label.name, sp->label.name,
		//		sp->call_count);

		//FIXME comment is a bit off as exactly one buffer is always
		//submitted
		//
		//Regardless of whether any commands were submitted for this
		//subpass, must step through each and every subpass, otherwise
		//the attachments won't be transitioned correctly.
		//However, only if not the last (or only) subpass.
		if (i < rp->subpass_count - 1) {
			//auto np = &rp->subpasses[i + 1];
			//printf ("%s -> %s\n", sp->label.name, np->label.name);
			dfunc->vkCmdNextSubpass (cmd, rp->subpassContents);
		}
	}

	dfunc->vkCmdEndRenderPass (cmd);
	QFV_duCmdEndLabel (device, cmd);
}

static void
run_renderpass (qfv_renderpass_t *rp, qfv_taskctx_t *taskctx)
{
	qfZoneNamed (zone, true);
	qfZoneName (zone, rp->label.name, rp->label.name_len);
	qfZoneColor (zone, rp->label.color32);

	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "cmd:render:%s:%" PRIu64,
							  rp->label.name, ctx->frameNumber));
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_RunRenderPassCmd (cmd, taskctx, rp);

	dfunc->vkEndCommandBuffer (cmd);
	DARRAY_APPEND (&graph->commands, cmd);
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
		QFV_PushBlackboard (ctx, cmd, pipeline);
		dfunc->vkCmdDispatch (cmd, d[0], d[1], d[2]);
	}
}

static void
run_compute (qfv_compute_t *comp, qfv_taskctx_t *taskctx, qfv_step_t *step)
{
	qfZoneNamed (zone, true);
	qfZoneName (zone, step->label.name, step->label.name_len);
	qfZoneColor (zone, step->label.color32);
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto graph = taskctx->graph;

	VkCommandBuffer cmd = QFV_GetCmdBuffer (ctx, false);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
						 vac (ctx->va_ctx, "cmd:compute:%s:%" PRIu64,
							  step->label.name, ctx->frameNumber));

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, step->label.name,
						 {VEC4_EXP (step->label.color)});

	for (uint32_t i = 0; i < comp->pipeline_count; i++) {
		__auto_type pipeline = &comp->pipelines[i];
		run_compute_pipeline (pipeline, cmd, ctx);
	}
	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
	DARRAY_APPEND (&graph->commands, cmd);
}

static void
run_process (qfv_step_t *step, qfv_taskctx_t *taskctx)
{
	auto proc = step->process;
	auto ctx = taskctx->ctx;
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];
	qfZoneNamed (zone, true);
	qfZoneName (zone, proc->label.name, proc->label.name_len);
	qfZoneColor (zone, proc->label.color32);
	qfv_taskctx_t tctx = *taskctx;
	tctx.step = step;
	tctx.frame = frame;
	run_tasks (proc->task_count, proc->tasks, &tctx);
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
						 vac (ctx->va_ctx, "cmd:render:%s:%" PRIu64,
							  "tracy", ctx->frameNumber));
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
QFV_RunRenderPass (qfv_taskctx_t *taskctx, qfv_renderpass_t *renderpass,
				   uint32_t width, uint32_t height)
{
	qfZoneNamed (zone, true);
	qfv_output_t output = {
		.extent = {
			.width = width,
			.height = height,
		},
	};
	renderpass_update_viewport_sissor (renderpass, &output);
	run_renderpass (renderpass, taskctx);
}

static void
run_deletion_queue (vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;
	uint64_t frame = ctx->frameNumber;
	while (!PQUEUE_IS_EMPTY (&rctx->deletion_queue)) {
		if (frame < PQUEUE_PEEK (&rctx->deletion_queue).deletion_frame) {
			break;
		}
		auto del = PQUEUE_REMOVE (&rctx->deletion_queue);
		QFV_DestroyResource (device, del.resources);
		if (del.framebuffer) {
			dfunc->vkDestroyFramebuffer (device->dev, del.framebuffer, 0);
		}
		if (del.semaphore) {
			dfunc->vkDestroySemaphore (device->dev, del.semaphore, 0);
		}
		if (del.image_view) {
			dfunc->vkDestroyImageView (device->dev, del.image_view, 0);
		}
	}
}

void
QFV_RunRenderJob (vulkan_ctx_t *ctx)
{
	qfZoneNamed (zone, true);
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;
	auto job = &graph->jobs[0];
	int64_t start = Sys_LongTime ();

	run_deletion_queue (ctx);

	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.graph = graph,
		.job = job,
	};

	for (uint32_t i = 0; i < job->num_steps; i++) {
		int64_t step_start = Sys_LongTime ();
		__auto_type step = &job->steps[i];
		if (!step->process) {
			// run render and compute steps automatically only if there's no
			// process for the step (the idea is the process uses the compute
			// and renderpass objects for its own purposes).
			if (step->render) {
				run_renderpass (step->render->active, &taskctx);
			}
			if (step->compute) {
				run_compute (step->compute, &taskctx, step);
			}
		}
		if (step->process) {
			run_process (step, &taskctx);
		}
		update_time (&step->time, step_start, Sys_LongTime ());
	}

	qfMessageL ("update_time");
	ctx->frameNumber++;
	if (++ctx->curFrame >= rctx->frames.size) {
		ctx->curFrame = 0;
	}
	update_time (&job->time, start, Sys_LongTime ());
}

static qfv_imageviewinfo_t * __attribute__((pure))
find_imageview (qfv_reference_t *ref, qfv_renderpass_t *rp,
				qfv_renderctx_t *rctx)
{
	auto jinfo = rctx->graphinfo;
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
	auto rctx = ctx->render_context;
	uint32_t frames = rctx->frames.size;
	auto resources = &rp->resources;
	qfv_resource_t *res = nullptr;
	// destroy the resources only if the renderpass owns the resources
	// (ie, don't touch the shared resources)
	if (resources->array) {
		res = &rp->resources.array[rp->resources.active];
	}
	qfv_delete_t del = {
		.resources = res,
		.framebuffer = rp->beginInfo.framebuffer,
		.deletion_frame = ctx->frameNumber + frames,
	};
	if (res) {
		qfv_resourcearray_next (&rp->resources);
	}
	PQUEUE_INSERT (&rctx->deletion_queue, del);
	rp->beginInfo.framebuffer = 0;
}

void
QFV_CreateFramebuffer (vulkan_ctx_t *ctx, qfv_renderpass_t *rp,
					   VkExtent2D extent)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	auto resources = &rp->resources;
	if (!resources->array && rp->framebufferinfo->use.name) {
		uint32_t use_index = rp->framebufferinfo->use_index;
		resources = &graph->framebuffer_resources[use_index];
	}

	auto res = &resources->array[resources->active];

	if (res && !res->memory) {
		for (uint32_t i = 0; i < res->num_objects; i++) {
			auto obj = &res->objects[i];
			if (obj->type == qfv_res_image) {
				obj->image.extent.width = extent.width;
				obj->image.extent.height = extent.height;
			}
		}
		QFV_CreateResource (ctx->device, res);
	}

	auto fb = rp->framebufferinfo;
	uint32_t layers = fb->layers;
	if (fb->use.name) {
		auto graphinfo = rctx->graphinfo;
		layers = graphinfo->framebuffers[fb->use_index].layers;
	}
	auto views = rp->framebuffer.views;
	VkFramebufferCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.attachmentCount = fb->num_attachments,
		.pAttachments = views,
		.renderPass = rp->beginInfo.renderPass,
		.width = extent.width,
		.height = extent.height,
		.layers = layers,
	};
	for (uint32_t i = 0; i < fb->num_attachments; i++) {
		if (fb->attachments[i].external) {
			views[i] = 0;
			if (!strcmp (fb->attachments[i].external, "$swapchain")) {
				auto sc = ctx->swapchain;
				views[i] = sc->imageViews->a[ctx->swapImageIndex];
				cInfo.width = sc->extent.width;
				cInfo.height = sc->extent.height;
			}
		} else {
			auto objects = res->objects;
			auto viewinfo = find_imageview (&fb->attachments[i].view, rp, rctx);
			views[i] = objects[viewinfo->object].image_view.view;
			if (rp->outputref.name) {
				viewinfo = find_imageview (&rp->outputref, rp, rctx);
				rp->output = objects[viewinfo->object].image_view.view;
			}
		}
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_FRAMEBUFFER, framebuffer,
						 vac (ctx->va_ctx, "framebuffer:%s", rp->label.name));

	rp->beginInfo.framebuffer = framebuffer;
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		sp->inherit.framebuffer = framebuffer;
	}
	rp->framebuffer.update_frame = ctx->frameNumber;
}

void
QFV_QueueResourceDelete (vulkan_ctx_t *ctx, qfv_resource_t *res)
{
	auto rctx = ctx->render_context;
	uint32_t frames = rctx->frames.size;
	qfv_delete_t del = {
		.resources = res,
		.deletion_frame = ctx->frameNumber + frames,
	};
	PQUEUE_INSERT (&rctx->deletion_queue, del);
}

void
QFV_QueueImageViewDelete (vulkan_ctx_t *ctx, VkImageView image_view)
{
	auto rctx = ctx->render_context;
	uint32_t frames = rctx->frames.size;
	qfv_delete_t del = {
		.image_view = image_view,
		.deletion_frame = ctx->frameNumber + frames,
	};
	PQUEUE_INSERT (&rctx->deletion_queue, del);
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

	auto res = dfunc->vkWaitForFences (dev, 1, &frame->fence,
									   VK_TRUE, 2000000000);
	if (res != VK_SUCCESS) {
		Sys_Error ("vkWaitForFences:%zd: %d", ctx->frameNumber, res);
	}

	QFV_CmdPoolManager_Reset (&frame->render_cmdpool);
	frame->active_pool = &frame->render_cmdpool;
	run_collect (ctx);
}

static void
update_framebuffer (const exprval_t **params, exprval_t *result,
					exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	VkExtent2D *extent = nullptr;
	if (graph->num_framebuffers) {
		//FIXME select framebuffer robustly
		auto fb = &graph->framebuffers[scr_fisheye != 0];
		extent = &fb->extent;
	}
	auto step = QFV_GetStep (params[0], graph);
	if (step) {
		extent = &step->render->output.extent;
	}

	qfv_output_t output = {};
	Vulkan_ConfigOutput (ctx, &output);
	int64_t size_time = rctx->size_time;
	if ((output.extent.width != extent->width
		|| output.extent.height != extent->height)
		&& (size_time < 0 || Sys_LongTime () - size_time > 2*1000*1000)) {
		if (step) {
			auto render = step->render;
			auto rp = render->active;
			QFV_DestroyFramebuffer (ctx, rp);
			QFV_UpdateViewportScissor (render, &output);
		} else {
			for (uint32_t i = 0; i < graph->num_framebuffers; i++) {
				auto fbr = &graph->framebuffer_resources[i];
				auto res = &fbr->array[fbr->active];
				if (res->memory) {
					QFV_QueueResourceDelete (ctx, res);
					qfv_resourcearray_next (fbr);
				}
			}
		}
		*extent = output.extent;
	}

	if (step) {
		auto render = step->render;
		auto rp = render->active;
		if (!rp->beginInfo.framebuffer) {
			QFV_CreateFramebuffer (ctx, rp, render->output.extent);
		}
	}
}

static void
fullscreen_pass (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto renderpass = taskctx->renderpass;
	auto subpass = taskctx->subpass;
	auto pipeline = taskctx->pipeline;
	auto cmd = taskctx->cmd;

	if (!subpass->num_inputs) {
		Sys_Error ("fullscreen_pass with no subpass inputs: %s:%s:%s",
				   renderpass->label.name, subpass->label.name,
				   pipeline->label.name);
	}
	QFV_BindDescriptors (ctx, cmd, pipeline);
	QFV_PushBlackboard (ctx, cmd, pipeline);
	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
	subpass->call_count++;
}

//FIXME probably should be part of submit_render but with options, or maybe
//need sync, dunno yet
static void
submit_depth (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto queue = &device->queue;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = graph->commands.size,
		.pCommandBuffers = graph->commands.a,
	};
	dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo, VK_NULL_HANDLE);
	DARRAY_RESIZE (&graph->commands, 0);
}

static void
submit_render (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto queue = &device->queue;
	auto frame = &rctx->frames.a[ctx->curFrame];
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = graph->commands.size,
		.pCommandBuffers = graph->commands.a,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &frame->renderDoneSemaphore,
	};
	dfunc->vkResetFences (device->dev, 1, &frame->fence);
	dfunc->vkQueueSubmit (queue->queue, 1, &submitInfo, frame->fence);
	DARRAY_RESIZE (&graph->commands, 0);
}

static void
blackboard_set_bufferptr (const exprval_t **params, exprval_t *result,
						  exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto rctx = ctx->render_context;
	auto varname = *(const char **) params[1]->value;
	auto bufname = *(const char **) params[0]->value;

	auto bb = &rctx->blackboard;
	if (!bb->symbols) {
		Sys_Error ("no blackboard");
	}

	auto pc = (qfv_pushconstantinfo_t *) Hash_Find (bb->symbols, varname);
	if (!pc) {
		Sys_Error ("invalid blackboard var %s\n", varname);
	}
	if (pc->type != qfv_ptr) {
		Sys_Error ("blackboard var %s not a pointer\n", varname);
	}

	auto addr = QFV_GetBufferAddress (ctx, bufname, ctx->curFrame);
	if (!addr) {
		Sys_Error ("invalid buffer %s\n", bufname);
	}

	auto ptr = (VkDeviceAddress *) (bb->data + pc->offset);
	*ptr = addr;
}

static exprfunc_t wait_on_fence_func[] = {
	{ .func = wait_on_fence },
	{}
};

static exprtype_t *update_framebuffer_params[] = {
	&cexpr_void,
};
static exprfunc_t update_framebuffer_func[] = {
	{ .func = update_framebuffer, .num_params = 1, update_framebuffer_params },
	{}
};
static exprfunc_t fullscreen_pass_func[] = {
	{ .func = fullscreen_pass },
	{}
};

static exprfunc_t submit_depth_func[] = {
	{ .func = submit_depth },
	{}
};

static exprfunc_t submit_render_func[] = {
	{ .func = submit_render },
	{}
};

static exprtype_t *blackboard_set_bufferptr_params[] = {
	&cexpr_string,
	&cexpr_string,
};
static exprfunc_t blackboard_set_bufferptr_func[] = {
	{ .func = blackboard_set_bufferptr, .num_params = 2,
		blackboard_set_bufferptr_params },
	{}
};

static exprsym_t render_task_syms[] = {
	{ "wait_on_fence", &cexpr_function, wait_on_fence_func },
	{ "update_framebuffer", &cexpr_function, update_framebuffer_func },
	{ "fullscreen_pass", &cexpr_function, fullscreen_pass_func },
	{ "submit_depth", &cexpr_function, submit_depth_func },
	{ "submit_render", &cexpr_function, submit_render_func },

	{ "blackboard_set_bufferptr", &cexpr_function,
		blackboard_set_bufferptr_func },
	{}
};

static int
qfv_deletion_compare (const qfv_delete_t *a, const qfv_delete_t *b, void *data)
{
	uint64_t diff = (int64_t) a->deletion_frame - (int64_t) b->deletion_frame;
	return -((diff >> 32) | !!diff);
}

void
QFV_Render_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	qfv_renderctx_t *rctx = malloc (sizeof (qfv_renderctx_t));
	ctx->render_context = rctx;

	*rctx = (qfv_renderctx_t) {
		.task_functions = { .symbols = &(exprsym_t) {} },
		.external_attachments = DARRAY_STATIC_INIT (4),
		.frames = DARRAY_STATIC_INIT (vulkan_frame_count),
		.deletion_queue = {
			.q = DARRAY_STATIC_INIT (8),
			.compare = qfv_deletion_compare,
		},
		.size_time = -1,
	};
	DARRAY_RESIZE (&rctx->frames, vulkan_frame_count);

	update_framebuffer_params[0] = &rctx->step_type;

	cexpr_init_symtab (&rctx->task_functions,
					   &(exprctx_t) { .hashctx = &rctx->hashctx });
	// clear temporary symbol list pointer
	rctx->task_functions.symbols = nullptr;

	QFV_Render_AddTasks (ctx, render_task_syms);

	auto device = ctx->device;
	for (size_t i = 0; i < rctx->frames.size; i++) {
		auto frame = &rctx->frames.a[i];
		*frame = (qfv_renderframe_t) {
			.fence = QFV_CreateFence (device, 1),
			.renderDoneSemaphore = QFV_CreateSemaphore (device),
			.active_pool = &frame->render_cmdpool,
		};
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_FENCE, frame->fence,
							 vac (ctx->va_ctx, "render:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_SEMAPHORE,
							 frame->renderDoneSemaphore,
							 vac (ctx->va_ctx, "render done:%zd", i));
		QFV_CmdPoolManager_Init (&frame->render_cmdpool, device,
								 vac (ctx->va_ctx, "render pool:%zd", i));
		QFV_CmdPoolManager_Init (&frame->output_cmdpool, device,
								 vac (ctx->va_ctx, "output pool:%zd", i));
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

static void
tf_free_syms (void *_sym, void *data)
{
	exprsym_t  *sym = _sym;
	for (exprfunc_t *f = sym->value; f->func; f++) {
		for (int i = 0; i < f->num_params; i++) {
			exprenum_t *e = f->param_types[i]->data;
			if (e && e->symtab->tab) {
				Hash_DelTable (e->symtab->tab);
				e->symtab->tab = 0;
			}
		}
	}
}

void
QFV_Render_Run_Init (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	if (rctx->graph) {
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
		};
		auto graph = rctx->graph;
		run_tasks (graph->init_task_count, graph->init_tasks, &taskctx);
	}
}

void
QFV_Render_Run_Startup (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	if (rctx->graph) {
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
		};
		auto graph = rctx->graph;
		for (size_t i = 0; i < graph->startup_funcs.size; i++) {
			graph->startup_funcs.a[i] ((exprctx_t *) &taskctx);
		}
	}
}

void
QFV_Render_Run_ClearState (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	if (rctx->graph) {
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
		};
		auto graph = rctx->graph;
		for (size_t i = 0; i < graph->clearstate_funcs.size; i++) {
			graph->clearstate_funcs.a[i] ((exprctx_t *) &taskctx);
		}
	}
}

static void
destroy_resource_array (vulkan_ctx_t *ctx, qfv_resourcearray_t *resources)
{
	for (uint32_t k = 0; k < resources->count; k++) {
		auto res = &resources->array[k];
		if (res && res->memory) {
			QFV_DestroyResource (ctx->device, res);
		}
	}
	free (resources->array);
}

void
QFV_Render_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	auto rctx = ctx->render_context;
	if (rctx->graph) {
		auto graph = rctx->graph;
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
			.graph = graph,
		};
		for (size_t i = graph->shutdown_funcs.size; i-- > 0; ) {
			graph->shutdown_funcs.a[i] ((exprctx_t *) &taskctx);
		}
		for (uint32_t i = 0; i < graph->num_renderpasses; i++) {
			dfunc->vkDestroyRenderPass (device->dev, graph->renderpasses[i], 0);
		}
		for (uint32_t i = 0; i < graph->num_pipelines; i++) {
			dfunc->vkDestroyPipeline (device->dev, graph->pipelines[i], 0);
		}
		for (uint32_t i = 0; i < graph->num_layouts; i++) {
			dfunc->vkDestroyPipelineLayout (device->dev, graph->layouts[i], 0);
		}
		for (uint32_t k = 0; k < graph->num_jobs; k++) {
			auto job = &graph->jobs[k];
			for (uint32_t i = 0; i < job->num_steps; i++) {
				if (job->steps[i].render) {
					auto render = job->steps[i].render;
					for (uint32_t j = 0; j < render->num_renderpasses; j++) {
						auto rp = &render->renderpasses[j];
						auto bi = &rp->beginInfo;
						if (bi->framebuffer) {
							dfunc->vkDestroyFramebuffer (device->dev,
														 bi->framebuffer, 0);
						}
						destroy_resource_array (ctx, &rp->resources);
					}
				}
			}
		}
		for (uint32_t i = 0; i < graph->num_framebuffers; i++) {
			destroy_resource_array (ctx, &graph->framebuffer_resources[i]);
		}
		DARRAY_CLEAR (&graph->commands);
		for (uint32_t i = 0; i < graph->num_dsmanagers; i++) {
			QFV_DSManager_Destroy (graph->dsmanager[i]);
		}
		QFV_DestroyResource (device, graph->resources);
		free (rctx->graph);
	}

	for (uint32_t i = 0; i < rctx->frames.size; i++) {
		auto dev = device->dev;
		auto df = dfunc;
		auto frame = &rctx->frames.a[i];
		df->vkDestroyFence (dev, frame->fence, 0);
		df->vkDestroySemaphore (dev, frame->renderDoneSemaphore, 0);
		QFV_CmdPoolManager_Shutdown (&frame->render_cmdpool);
		QFV_CmdPoolManager_Shutdown (&frame->output_cmdpool);
		qftCVkContextDestroy (frame->qftVkCtx);
	}
	DARRAY_CLEAR (&rctx->frames);

	if (rctx->blackboard.symbols) {
		Hash_DelTable (rctx->blackboard.symbols);
	}

	if (rctx->graphinfo) {
		auto jinfo = rctx->graphinfo;
		uint32_t count = jinfo->num_dslayouts + jinfo->num_splayouts;
		for (uint32_t i = 0; i < count; i++) {
			__auto_type setLayout = jinfo->dslayouts[i].setLayout;
			dfunc->vkDestroyDescriptorSetLayout (device->dev, setLayout, 0);
		}
		delete_memsuper (jinfo->memsuper);
	}
	if (rctx->task_functions.tab) {
		Hash_ForEach (rctx->task_functions.tab, tf_free_syms, 0);
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
	if (rctx->entqueueinfo) {
		Hash_DelTable (rctx->entqueue_symtab.tab);
		delete_memsuper (rctx->entqueueinfo->memsuper);
	}
	Hash_DelTable (rctx->job_symtab.tab);
	Hash_DelTable (rctx->step_symtab.tab);
	delete_memsuper (rctx->jobstepenum->memsuper);
	Hash_DelContext (rctx->hashctx);

	QFV_Render_UI_Shutdown (ctx);
	free (rctx);
}

void
QFV_Render_AddTasks (vulkan_ctx_t *ctx, exprsym_t *task_syms)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	exprctx_t ectx = { .hashctx = &rctx->hashctx };
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
QFV_Render_AddStartup (vulkan_ctx_t *ctx, qfv_initfunc_f func)
{
	auto rctx = ctx->render_context;
	DARRAY_APPEND (&rctx->graph->startup_funcs, func);
}

void
QFV_Render_AddShutdown (vulkan_ctx_t *ctx, qfv_initfunc_f func)
{
	auto rctx = ctx->render_context;
	DARRAY_APPEND (&rctx->graph->shutdown_funcs, func);
}

void
QFV_Render_AddClearState (vulkan_ctx_t *ctx, qfv_initfunc_f func)
{
	auto rctx = ctx->render_context;
	DARRAY_APPEND (&rctx->graph->clearstate_funcs, func);
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
QFV_FindResource (vulkan_ctx_t *ctx, const char *name, qfv_renderpass_t *rp)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;

	auto resources = &rp->resources;

	if (!resources->array) {
		uint32_t use_index = rp->framebufferinfo->use_index;
		resources = &graph->framebuffer_resources[use_index];
	}
	auto res = &resources->array[resources->active];
	for (uint32_t i = 0; i < res->num_objects; i++) {
		auto obj = &res->objects[i];
		if (!strcmp (obj->name, name)) {
			return obj;
		}
	}
	return 0;
}

qfv_step_t *
QFV_FindStep (const char *name, qfv_graph_t *graph)
{
	auto job = &graph->jobs[0];
	for (uint32_t i = 0; i < job->num_steps; i++) {
		auto step = &job->steps[i];
		if (!strcmp (step->label.name, name)) {
			return step;
		}
	}
	return 0;
}

#define MAX_BUFFER_BYTES (16*1024*1024)

VkDeviceAddress
QFV_GetBufferAddress (vulkan_ctx_t *ctx, const char *name, uint32_t frame)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;
	auto jinfo = rctx->graphinfo;
	auto buffer = QFV_FindBufferInfo (ctx, name);
	if (!buffer) {
		return 0;
	}
	uint32_t ind = buffer - jinfo->buffers;
	VkDeviceAddress offset = 0;
	if (buffer->perframe) {
		offset = frame * buffer->size;
	}
	return graph->resources->objects[ind].buffer.address + offset;
}

VkDeviceAddress
QFV_GetBufferOffset (vulkan_ctx_t *ctx, const char *name, uint32_t frame)
{
	auto buffer = QFV_FindBufferInfo (ctx, name);
	if (!buffer) {
		return 0;
	}
	VkDeviceAddress offset = 0;
	if (buffer->perframe) {
		offset = frame * buffer->size;
	}
	return offset;
}

VkDeviceSize
QFV_GetBufferSize (vulkan_ctx_t *ctx, const char *name)
{
	auto buffer = QFV_FindBufferInfo (ctx, name);
	if (!buffer) {
		return 0;
	}
	return buffer->size;
}

void
QFV_UpdateBuffer (vulkan_ctx_t *ctx, const char *name, uint32_t offset,
				  void *data, uint32_t size)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;
	auto jinfo = rctx->graphinfo;
	auto bufferinfo = QFV_FindBufferInfo (ctx, name);
	if (!bufferinfo) {
		return;
	}
	uint32_t ind = bufferinfo - jinfo->buffers;
	auto buffer = graph->resources->objects[ind].buffer.buffer;

	if (offset + size > bufferinfo->size) {
		Sys_Error ("bad offset + size in update to %s: %u %u:%"PRIu64,
				   name, offset, size, bufferinfo->size);
	}
	uint32_t frame = ctx->curFrame;
	auto dstOffset = offset;
	if (bufferinfo->perframe) {
		dstOffset += frame * bufferinfo->size;
	}
	qfv_packet_t *packet = nullptr;
	while (size) {
		uint32_t count = size;
		if (count > MAX_BUFFER_BYTES) {
			count = MAX_BUFFER_BYTES;
		}
		if (packet) {
			QFV_PacketSubmit (packet);
		}
		packet = QFV_PacketAcquire (ctx->staging, "QFV_UpdateBuffer");
		void *pkt_data = QFV_PacketExtend (packet, count);
		memcpy (pkt_data, data, count);
		auto sb = &bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
		//FIXME config?
		auto db = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
		QFV_PacketCopyBuffer (packet, buffer, dstOffset, sb, db);
		dstOffset += count;
		size -= count;
	}
	QFV_PacketSubmit (packet);
}

void
QFV_PushBlackboard (vulkan_ctx_t *ctx, VkCommandBuffer cmd,
					qfv_pipeline_t *pipeline)
{
	auto rctx = ctx->render_context;
	auto device = ctx->device;

	if (pipeline->num_push_constants) {
		auto layout = pipeline->layout;
		auto blackboard = &rctx->blackboard;
		auto first = pipeline->first_push_constant;
		auto count = pipeline->num_push_constants;
		auto push_constants = blackboard->push_constants + first;
		QFV_PushConstants (device, cmd, layout, count, push_constants);
	}
}

void
QFV_BindDescriptors (vulkan_ctx_t *ctx, VkCommandBuffer cmd,
					 qfv_pipeline_t *pipeline)
{
	if (pipeline->num_indices) {
		auto device = ctx->device;
		auto dfunc = device->funcs;
		auto rctx = ctx->render_context;
		auto rframe = &rctx->frames.a[ctx->curFrame];
		VkDescriptorSet sets[pipeline->num_indices];
		for (uint32_t i = 0; i < pipeline->num_indices; i++) {
			sets[i] = rframe->descriptor_sets[pipeline->ds_indices[i]];
		}
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										pipeline->layout,
										0, pipeline->num_indices, sets, 0, 0);
	}
}


void *
QFV_GetBlackboardVar (struct vulkan_ctx_s *ctx, const char *name)
{
	auto rctx = ctx->render_context;
	auto bb = &rctx->blackboard;
	if (!bb->symbols) {
		return nullptr;
	}
	auto pc = (qfv_pushconstantinfo_t *) Hash_Find (bb->symbols, name);
	if (!pc) {
		return nullptr;
	}
	return bb->data + pc->offset;
}

qfv_step_t *
QFV_GetStep (const exprval_t *param, qfv_graph_t *graph)
{
	int index = *(int *) param->value;
	if (index < 0) {
		return nullptr;
	}
	return &graph->steps[index];
}

qfv_dsmanager_t *
QFV_Render_DSManager (vulkan_ctx_t *ctx, const char *setName)
{
	qfZoneScoped (true);
	auto graph = ctx->render_context->graph;
	for (uint32_t i = 0; i < graph->num_dsmanagers; i++) {
		auto ds = graph->dsmanager[i];
		if (!strcmp (ds->name, setName)) {
			return ds;
		}
	}
	Sys_Printf ("descriptor set '%s' not found\n", setName);
	return 0;
}

void
QFV_CreateSampler (vulkan_ctx_t *ctx, qfv_samplercreateinfo_t *sampler)
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
						 vac (ctx->va_ctx, "sampler:%s", sampler->name));
}

VkSampler
QFV_Render_Sampler (vulkan_ctx_t *ctx, const char *name)
{
	qfZoneScoped (true);
	auto si = ctx->render_context->samplerinfo;
	if (!si) {
		return 0;
	}
	for (uint32_t i = 0; i < si->num_samplers; i++) {
		auto sci = &si->samplers[i];
		if (!strcmp (sci->name, name)) {
			if (!sci->sampler) {
				QFV_CreateSampler (ctx, sci);
			}
			return sci->sampler;
		}
	}
	printf ("sampler %s not found\n", name);
	return 0;
}

void
QFV_Render_NewScene (struct scene_s *scene, vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	auto frame = &rctx->frames.a[ctx->curFrame];
	auto graph = rctx->graph;

	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.frame = frame,
		.data = scene,
	};

	run_tasks (graph->newscene_task_count, graph->newscene_tasks, &taskctx);
}
