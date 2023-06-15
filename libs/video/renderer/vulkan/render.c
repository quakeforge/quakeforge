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
#include "QF/va.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/pipeline.h"
#include "vid_vulkan.h"

#include "vkparse.h"

VkCommandBuffer
QFV_GetCmdBufffer (vulkan_ctx_t *ctx, bool secondary)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	VkCommandBufferAllocateInfo cinfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = job->command_pool,
		.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
						   : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd;
	dfunc->vkAllocateCommandBuffers (device->dev, &cinfo, &cmd);
	return cmd;
}

void
QFV_AppendCmdBuffer (vulkan_ctx_t *ctx, VkCommandBuffer cmd)
{
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;
	DARRAY_APPEND (&job->commands, cmd);
}

static void
run_tasks (uint32_t task_count, qfv_taskinfo_t *tasks, qfv_taskctx_t *ctx)
{
	for (uint32_t i = 0; i < task_count; i++) {
		tasks[i].func->func (tasks[i].params, 0, (exprctx_t *) ctx);
	}
}

static void
run_pipeline (qfv_pipeline_t *pipeline, VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCmdBindPipeline (cmd, pipeline->bindPoint, pipeline->pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &pipeline->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &pipeline->scissor);

	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.pipeline = pipeline,
	};
	run_tasks (pipeline->task_count, pipeline->tasks, &taskctx);

	if (pipeline->num_descriptorsets) {
		dfunc->vkCmdBindDescriptorSets (cmd, pipeline->bindPoint,
										pipeline->layout,
										pipeline->first_descriptorset,
										pipeline->num_descriptorsets,
										pipeline->descriptorsets,
										0, 0);
	}
	if (pipeline->num_push_constants) {
		QFV_PushConstants (device, cmd, pipeline->layout,
						   pipeline->num_push_constants,
						   pipeline->push_constants);
	}
}

// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
static void
run_subpass (qfv_subpass_t *sp, VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkResetCommandBuffer (cmd, 0);
	dfunc->vkBeginCommandBuffer (cmd, &sp->beginInfo);
	QFV_duCmdBeginLabel (device, cmd, sp->label.name,
						 {VEC4_EXP (sp->label.color)});

	for (uint32_t i = 0; i < sp->pipeline_count; i++) {
		__auto_type pipeline = &sp->pipelines[i];
		run_pipeline (pipeline, cmd, ctx);
	}

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

static void
run_renderpass (qfv_renderpass_t *rp, vulkan_ctx_t *ctx)
{
	printf ("run_renderpass: %s\n", rp->label.name);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	VkCommandBuffer cmd = QFV_GetCmdBufffer (ctx, false);
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	QFV_duCmdBeginLabel (device, cmd, rp->label.name,
						 {VEC4_EXP (rp->label.color)});
	dfunc->vkCmdBeginRenderPass (cmd, &rp->beginInfo, rp->subpassContents);
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		VkCommandBuffer subcmd = QFV_GetCmdBufffer (ctx, true);
		run_subpass (sp, subcmd, ctx);
		dfunc->vkCmdExecuteCommands (cmd, 1, &subcmd);
		//FIXME comment is a bit off as exactly one buffer is always submitted
		//
		//Regardless of whether any commands were submitted for this
		//subpass, must step through each and every subpass, otherwise
		//the attachments won't be transitioned correctly.
		//However, only if not the last (or only) subpass.
		if (i < rp->subpass_count - 1) {
			dfunc->vkCmdNextSubpass (cmd, rp->subpassContents);
		}
	}
	QFV_CmdEndLabel (device, cmd);
	DARRAY_APPEND (&job->commands, cmd);
}

static void
run_compute_pipeline (qfv_pipeline_t *pipeline, VkCommandBuffer cmd,
					  vulkan_ctx_t *ctx)
{
	printf ("run_compute_pipeline: %s\n", pipeline->label.name);

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCmdBindPipeline (cmd, pipeline->bindPoint, pipeline->pipeline);

	qfv_taskctx_t taskctx = {
		.ctx = ctx,
		.pipeline = pipeline,
	};
	run_tasks (pipeline->task_count, pipeline->tasks, &taskctx);

	if (pipeline->num_descriptorsets) {
		dfunc->vkCmdBindDescriptorSets (cmd, pipeline->bindPoint,
										pipeline->layout,
										pipeline->first_descriptorset,
										pipeline->num_descriptorsets,
										pipeline->descriptorsets,
										0, 0);
	}
	if (pipeline->num_push_constants) {
		QFV_PushConstants (device, cmd, pipeline->layout,
						   pipeline->num_push_constants,
						   pipeline->push_constants);
	}
	vec4u_t     d = pipeline->dispatch;
	dfunc->vkCmdDispatch (cmd, d[0], d[1], d[2]);
}

static void
run_compute (qfv_compute_t *comp, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	VkCommandBuffer cmd = QFV_GetCmdBufffer (ctx, false);

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	for (uint32_t i = 0; i < comp->pipeline_count; i++) {
		__auto_type pipeline = &comp->pipelines[i];
		run_compute_pipeline (pipeline, cmd, ctx);
	}
	dfunc->vkEndCommandBuffer (cmd);
	DARRAY_APPEND (&job->commands, cmd);
}

static void
run_process (qfv_process_t *proc, vulkan_ctx_t *ctx)
{
	qfv_taskctx_t taskctx = {
		.ctx = ctx,
	};
	run_tasks (proc->task_count, proc->tasks, &taskctx);
}

void
QFV_RunRenderJob (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	for (uint32_t i = 0; i < job->num_steps; i++) {
		__auto_type step = &job->steps[i];
		printf ("run_step: %s\n", step->label.name);
		if (step->render) {
			run_renderpass (step->render->active, ctx);
		}
		if (step->compute) {
			run_compute (step->compute, ctx);
		}
		if (step->process) {
			run_process (step->process, ctx);
		}
	}
}

void
QFV_DestroyFramebuffer (vulkan_ctx_t *ctx)
{
#if 0
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;
	__auto_type rp = &job->renderpasses[0];

	if (rp->beginInfo.framebuffer) {
		VkFramebuffer framebuffer = rp->beginInfo.framebuffer;
		rp->beginInfo.framebuffer = 0;
		dfunc->vkDestroyFramebuffer (device->dev, framebuffer, 0);
	}
#endif
}

void
QFV_CreateFramebuffer (vulkan_ctx_t *ctx)
{
#if 0
	__auto_type rctx = ctx->render_context;
	__auto_type rinfo = rctx->renderinfo;
	__auto_type job = rctx->job;
	__auto_type rpInfo = &rinfo->renderpasses[0];
	__auto_type rp = &job->renderpasses[0];

	VkImageView attachments[rpInfo->num_attachments];
	VkFramebufferCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.attachmentCount = rpInfo->num_attachments,
		.pAttachments = attachments,
		.renderPass = rp->beginInfo.renderPass,
		.width = rpInfo->framebuffer.width,
		.height = rpInfo->framebuffer.height,
		.layers = rpInfo->framebuffer.layers,
	};
	for (uint32_t i = 0; i < rpInfo->num_attachments; i++) {
		attachments[i] = find_imageview (&rpInfo->attachments[i].view, rctx);
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_FRAMEBUFFER, framebuffer,
						 va (ctx->va_ctx, "framebuffer:%s", rpInfo->name));

	rp->beginInfo.framebuffer = framebuffer;
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		sp->inherit.framebuffer = framebuffer;
	}
#endif
}

void
QFV_Render_Init (vulkan_ctx_t *ctx)
{
	qfv_renderctx_t *rctx = calloc (1, sizeof (*rctx));
	ctx->render_context = rctx;

	exprctx_t   ectx = { .hashctx = &rctx->hashctx };
	exprsym_t   syms[] = { {} };
	rctx->task_functions.symbols = syms;
	cexpr_init_symtab (&rctx->task_functions, &ectx);
	rctx->task_functions.symbols = 0;
}

void
QFV_Render_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	if (rctx->job) {
		__auto_type job = rctx->job;
		QFV_DestroyFramebuffer (ctx); //FIXME do properly
		for (uint32_t i = 0; i < job->num_renderpasses; i++) {
			dfunc->vkDestroyRenderPass (device->dev, job->renderpasses[i], 0);
		}
		for (uint32_t i = 0; i < job->num_pipelines; i++) {
			dfunc->vkDestroyPipeline (device->dev, job->pipelines[i], 0);
		}
		for (uint32_t i = 0; i < job->num_layouts; i++) {
			dfunc->vkDestroyPipelineLayout (device->dev, job->layouts[i], 0);
		}
		if (job->resources) {
			QFV_DestroyResource (ctx->device, job->resources);
			free (job->resources);
		}
		if (job->command_pool) {
			dfunc->vkDestroyCommandPool (device->dev, job->command_pool, 0);
		}
		DARRAY_CLEAR (&job->commands);
		free (rctx->job);
	}
	if (rctx->jobinfo) {
		__auto_type jinfo = rctx->jobinfo;
		for (uint32_t i = 0; i < jinfo->num_descriptorsetlayouts; i++) {
			__auto_type setLayout = jinfo->descriptorsetlayouts[i].setLayout;
			dfunc->vkDestroyDescriptorSetLayout (device->dev, setLayout, 0);
		}
		delete_memsuper (jinfo->memsuper);
	}
	if (rctx->task_functions.tab) {
		Hash_DelTable (rctx->task_functions.tab);
	}
	Hash_DelContext (rctx->hashctx);
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
