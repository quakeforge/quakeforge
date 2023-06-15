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

static VkCommandBuffer
get_cmd_buffer (vulkan_ctx_t *ctx, int secondary)
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

static void
run_tasks (uint32_t task_count, qfv_taskinfo_t *tasks, void *ctx)
{
	for (uint32_t i = 0; i < task_count; i++) {
		tasks[i].func->func (tasks[i].params, 0, ctx);
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

	VkCommandBuffer cmd = get_cmd_buffer (ctx, 1);
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkResetCommandBuffer (cmd, 0);
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);
	QFV_duCmdBeginLabel (device, cmd, rp->label.name,
						 {VEC4_EXP (rp->label.color)});
	dfunc->vkCmdBeginRenderPass (cmd, &rp->beginInfo, rp->subpassContents);
	for (uint32_t i = 0; i < rp->subpass_count; i++) {
		__auto_type sp = &rp->subpasses[i];
		VkCommandBuffer subcmd = get_cmd_buffer (ctx, 1);
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
	uint32_t   *d = pipeline->dispatch;
	dfunc->vkCmdDispatch (cmd, d[0], d[1], d[2]);
}

static void
run_compute (qfv_compute_t *comp, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type job = rctx->job;

	VkCommandBuffer cmd = get_cmd_buffer (ctx, 0);

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

static qfv_output_t
get_output (vulkan_ctx_t *ctx, plitem_t *item)
{
	qfv_output_t output = {};
	Vulkan_ConfigOutput (ctx, &output);

	plitem_t   *output_def = PL_ObjectForKey (item, "output");
	if (output_def) {
		// QFV_ParseOutput clears the structure, but extent and frames need to
		// be preserved
		qfv_output_t o = output;
		QFV_ParseOutput (ctx, &o, output_def, item);
		output.format = o.format;
		output.finalLayout = o.finalLayout;
	}
	return output;
}

void
QFV_LoadRenderInfo (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;

	plitem_t   *item = Vulkan_GetConfig (ctx, "main_def");
	__auto_type output = get_output (ctx, item);
	Vulkan_Script_SetOutput (ctx, &output);
	rctx->jobinfo = QFV_ParseJobInfo (ctx, item, rctx);
	if (rctx->jobinfo) {
		rctx->jobinfo->plitem = item;
	}
}

typedef struct {
	uint32_t    num_images;
	uint32_t    num_imageviews;
	uint32_t    num_buffers;
	uint32_t    num_bufferviews;
	uint32_t    num_layouts;

	uint32_t    num_steps;
	uint32_t    num_render;
	uint32_t    num_compute;
	uint32_t    num_process;
	uint32_t    num_tasks;

	uint32_t    num_renderpasses;
	uint32_t    num_attachments;
	uint32_t    num_subpasses;
	uint32_t    num_dependencies;
	uint32_t    num_attachmentrefs;
	uint32_t    num_colorblend;
	uint32_t    num_preserve;
	uint32_t    num_graph_pipelines;
	uint32_t    num_comp_pipelines;

	uint32_t    num_descriptorsets;
} objcount_t;

static void
count_as_stuff (qfv_attachmentsetinfo_t *as, objcount_t *counts)
{
	counts->num_attachmentrefs += as->num_input;
	counts->num_attachmentrefs += as->num_color;
	counts->num_colorblend += as->num_color;
	if (as->resolve) {
		counts->num_attachmentrefs += as->num_color;
	}
	if (as->depth) {
		counts->num_attachmentrefs += 1;
	}
	counts->num_preserve += as->num_preserve;
}

static void
count_sp_stuff (qfv_subpassinfo_t *spi, objcount_t *counts)
{
	counts->num_dependencies += spi->num_dependencies;
	if (spi->attachments) {
		count_as_stuff (spi->attachments, counts);
	}
	for (uint32_t i = 0; i < spi->num_pipelines; i++) {
		__auto_type pli = &spi->pipelines[i];
		if (pli->num_graph_stages && !pli->compute_stage) {
			counts->num_graph_pipelines++;
			counts->num_tasks += pli->num_tasks;
		} else {
			Sys_Error ("%s:%s: invalid graphics pipeline",
					   spi->name, pli->name);
		}
	}
}

static void
count_rp_stuff (qfv_renderpassinfo_t *rpi, objcount_t *counts)
{
	counts->num_attachments += rpi->num_attachments;
	counts->num_subpasses += rpi->num_subpasses;
	for (uint32_t i = 0; i < rpi->num_subpasses; i++) {
		count_sp_stuff (&rpi->subpasses[i], counts);
	}
}

static void
count_comp_stuff (qfv_computeinfo_t *ci, objcount_t *counts)
{
	for (uint32_t i = 0; i < ci->num_pipelines; i++) {
		__auto_type pli = &ci->pipelines[i];
		if (!pli->num_graph_stages && pli->compute_stage) {
			counts->num_comp_pipelines++;
			counts->num_tasks += pli->num_tasks;
		} else {
			Sys_Error ("%s:%s: invalid compute pipeline",
					   ci->name, pli->name);
		}
	}
}

static void
count_step_stuff (qfv_stepinfo_t *step, objcount_t *counts)
{
	if ((step->render && step->compute)
		|| (step->render && step->process)
		|| (step->compute && step->process)) {
		Sys_Error ("%s: invalid step: must be one of render/compute/process",
				   step->name);
	}
	if (step->render) {
		__auto_type rinfo = step->render;
		counts->num_renderpasses += rinfo->num_renderpasses;
		for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
			count_rp_stuff (&rinfo->renderpasses[i], counts);
		}
		counts->num_render++;
	}
	if (step->compute) {
		count_comp_stuff (step->compute, counts);
		counts->num_compute++;
	}
	if (step->process) {
		counts->num_process++;
		counts->num_tasks += step->process->num_tasks;
	}
	counts->num_steps++;
}

static void
count_stuff (qfv_jobinfo_t *jobinfo, objcount_t *counts)
{
	counts->num_images += jobinfo->num_images;
	counts->num_imageviews += jobinfo->num_imageviews;
	counts->num_buffers += jobinfo->num_buffers;
	counts->num_bufferviews += jobinfo->num_bufferviews;
	for (uint32_t i = 0; i < jobinfo->num_steps; i++) {
		count_step_stuff (&jobinfo->steps[i], counts);
	}
}

static void
create_resources (vulkan_ctx_t *ctx, objcount_t *counts)
{
	__auto_type rctx = ctx->render_context;
	__auto_type jinfo = rctx->jobinfo;
	__auto_type job = rctx->job;

	job->resources = malloc (sizeof(qfv_resource_t)
								+ counts->num_images * sizeof (qfv_resobj_t)
								+ counts->num_imageviews * sizeof (qfv_resobj_t));
	job->images = (qfv_resobj_t *) &job->resources[1];
	job->image_views = &job->images[counts->num_images];

	job->resources[0] = (qfv_resource_t) {
		.name = "render",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = counts->num_images + counts->num_imageviews,
		.objects = job->images,
	};
	for (uint32_t i = 0; i < counts->num_images; i++) {
		__auto_type img = &jinfo->images[i];
		job->images[i] = (qfv_resobj_t) {
			.name = img->name,
			.type = qfv_res_image,
			.image = {
				.flags = img->flags,
				.type = img->imageType,
				.format = img->format,
				.extent = img->extent,
				.num_mipmaps = img->mipLevels,
				.num_layers = img->arrayLayers,
				.samples = img->samples,
				.tiling = img->tiling,
				.usage = img->usage,
				.initialLayout = img->initialLayout,
			},
		};
	}
	int         error = 0;
	for (uint32_t i = 0; i < counts->num_imageviews; i++) {
		__auto_type view = &jinfo->imageviews[i];
		job->image_views[i] = (qfv_resobj_t) {
			.name = view->name,
			.type = qfv_res_image_view,
			.image_view = {
				.flags = view->flags,
				.type = view->viewType,
				.format = view->format,
				.components = view->components,
				.subresourceRange = view->subresourceRange,
			},
		};
		if (strcmp (view->image.name, "$output.image") == 0) {
			//__auto_type image = jinfo->output.image;
			//job->image_views[i].image_view.external_image = image;
			//job->image_views[i].image_view.image = -1;
		} else {
			qfv_resobj_t *img = 0;
			for (uint32_t j = 0; j < jinfo->num_images; j++) {
				if (strcmp (view->image.name, jinfo->images[j].name) == 0) {
					img = &job->images[j];
				}
			}
			if (img) {
				uint32_t    ind = img - job->resources->objects;
				job->image_views[i].image_view.image = ind;
			} else {
				Sys_Printf ("%d: unknown image reference: %s\n",
							view->image.line, view->image.name);
				error = 1;
			}
		}
	}
	if (error) {
		free (job->resources);
		job->resources = 0;
		return;
	}
	QFV_CreateResource (ctx->device, job->resources);
}

typedef struct {
	VkRenderPassCreateInfo *rpCreate;
	VkAttachmentDescription *attach;
	VkClearValue *clear;
	VkSubpassDescription *subpass;
	VkSubpassDependency *depend;
	VkAttachmentReference *attachref;
	VkPipelineColorBlendAttachmentState *cbAttach;
	uint32_t   *preserve;
	const char **rpName;
	const char **plName;
	VkComputePipelineCreateInfo *cplCreate;
	VkGraphicsPipelineCreateInfo *gplCreate;
	VkPipelineColorBlendStateCreateInfo *cbState;
	qfv_layoutinfo_t *layouts;

	uint32_t   *pl_counts;

	VkPipeline *gpl;
	VkPipeline *cpl;
	VkRenderPass *rp;
} objptr_t;

typedef struct {
	objcount_t  inds;
	objptr_t    ptr;

	vulkan_ctx_t *ctx;
	qfv_jobinfo_t *jinfo;
	exprtab_t  *symtab;
	qfv_renderpassinfo_t *rpi;
	VkRenderPassCreateInfo *rpc;
	qfv_subpassinfo_t *spi;
	VkSubpassDescription *spc;
	qfv_pipelineinfo_t *pli;
	VkGraphicsPipelineCreateInfo *plc;
} objstate_t;

static uint32_t __attribute__((pure))
find_subpass (qfv_dependencyinfo_t *d, uint32_t spind,
			  qfv_subpassinfo_t *subpasses)
{
	if (strcmp (d->name, "$external") == 0) {
		return VK_SUBPASS_EXTERNAL;
	}
	for (uint32_t i = 0; i <= spind; i++) {
		__auto_type s = &subpasses[i];
		if (strcmp (d->name, s->name) == 0) {
			return i;
		}
	}
	Sys_Error ("invalid dependency: [%d] %s", spind, d->name);
}

static VkDescriptorSetLayout
find_descriptorSet (const qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->jinfo->num_descriptorsetlayouts; i++) {
		__auto_type ds = &s->jinfo->descriptorsetlayouts[i];
		if (strcmp (ds->name, ref->name) == 0) {
			if (!ds->setLayout) {
				VkDescriptorSetLayoutCreateInfo cInfo = {
					.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.flags = ds->flags,
					.bindingCount = ds->num_bindings,
					.pBindings = ds->bindings,
				};
				qfv_device_t *device = s->ctx->device;
				qfv_devfuncs_t *dfunc = device->funcs;
				dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0,
													&ds->setLayout);
				QFV_duSetObjectName (device,
									 VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
									 ds->setLayout,
									 va (s->ctx->va_ctx, "descriptorSet:%s",
										 ds->name));
			}
			return ds->setLayout;
		}
	}
	Sys_Error ("%s.%s:%d: invalid descriptor set layout: %s",
			   s->rpi->name, s->spi->name, ref->line, ref->name);
}

static qfv_layoutinfo_t *
find_layout (const qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->inds.num_layouts; i++) {
		if (strcmp (s->ptr.layouts[i].name, ref->name) == 0) {
			return &s->ptr.layouts[i];
		}
	}
	if (!QFV_ParseLayoutInfo (s->ctx, s->jinfo->memsuper, s->symtab, ref->name,
							  &s->ptr.layouts[s->inds.num_layouts])) {
		Sys_Error ("%s.%s:%d: invalid layout: %s",
				   s->rpi->name, s->spi->name, ref->line, ref->name);
	}
	__auto_type li = &s->ptr.layouts[s->inds.num_layouts++];
	li->name = ref->name;
	VkDescriptorSetLayout sets[li->num_sets];
	for (uint32_t i = 0; i < li->num_sets; i++) {
		sets[i] = find_descriptorSet (&li->sets[i], s);
	}
	VkPipelineLayoutCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = li->num_sets,
		.pSetLayouts = sets,
		.pushConstantRangeCount = li->num_ranges,
		.pPushConstantRanges = li->ranges,
	};
	qfv_device_t *device = s->ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCreatePipelineLayout (device->dev, &cInfo, 0, &li->layout);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, li->layout,
						 va (s->ctx->va_ctx, "layout:%s", li->name));
	return li;
}

static void
init_plCreate (VkGraphicsPipelineCreateInfo *plc, const qfv_pipelineinfo_t *pli,
			   objstate_t *s)
{
	if (pli->num_graph_stages) {
		plc->stageCount = pli->num_graph_stages;
	}
	if (pli->graph_stages) {
		plc->pStages = pli->graph_stages;
	}
	if (pli->vertexInput) {
		plc->pVertexInputState = pli->vertexInput;
	}
	if (pli->inputAssembly) {
		plc->pInputAssemblyState = pli->inputAssembly;
	}
	if (pli->tessellation) {
		plc->pTessellationState = pli->tessellation;
	}
	if (pli->viewport) {
		plc->pViewportState = pli->viewport;
	}
	if (pli->rasterization) {
		plc->pRasterizationState = pli->rasterization;
	}
	if (pli->multisample) {
		plc->pMultisampleState = pli->multisample;
	}
	if (pli->depthStencil) {
		plc->pDepthStencilState = pli->depthStencil;
	}
	if (pli->colorBlend) {
		VkPipelineColorBlendStateCreateInfo *cb;
		cb = (VkPipelineColorBlendStateCreateInfo *) plc->pColorBlendState;
		*cb = *pli->colorBlend;
	}
	if (pli->dynamic) {
		plc->pDynamicState = pli->dynamic;
	}
	if (pli->layout.name) {
		__auto_type li = find_layout (&pli->layout, s);
		plc->layout = li->layout;
		s->inds.num_descriptorsets += li->num_sets;
	}
}

static uint32_t __attribute__((pure))
find_attachment (qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->rpi->num_attachments; i++) {
		__auto_type a = &s->rpi->attachments[i];
		if (strcmp (ref->name, a->name) == 0) {
			return i;
		}
	}
	Sys_Error ("%s.%s:%d: invalid attachment: %s",
			   s->rpi->name, s->spi->name, ref->line, ref->name);
}

static void
init_arCreate (const qfv_attachmentrefinfo_t *ari, objstate_t *s)
{
	__auto_type arc = &s->ptr.attachref[s->inds.num_attachmentrefs];
	qfv_reference_t ref = {
		.name = ari->name,
		.line = ari->line,
	};

	*arc = (VkAttachmentReference) {
		.attachment = find_attachment (&ref, s),
		.layout = ari->layout,
	};
}

static void
init_cbCreate (const qfv_attachmentrefinfo_t *ari, objstate_t *s)
{
	__auto_type cbc = &s->ptr.cbAttach[s->inds.num_colorblend];

	*cbc = ari->blend;
}

static void
init_spCreate (uint32_t index, qfv_subpassinfo_t *sub, objstate_t *s)
{
	s->spi = &sub[index];
	s->plc = &s->ptr.gplCreate[s->inds.num_graph_pipelines];
	s->spc = &s->ptr.subpass[s->inds.num_subpasses];
	__auto_type pln = &s->ptr.plName[s->inds.num_graph_pipelines];
	__auto_type cbs = &s->ptr.cbState[s->inds.num_graph_pipelines];

	s->ptr.pl_counts[s->inds.num_renderpasses] += s->spi->num_pipelines;

	*s->spc = (VkSubpassDescription) {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
	};
	for (uint32_t i = 0; i < s->spi->num_dependencies; i++) {
		__auto_type d = &s->spi->dependencies[i];
		__auto_type dep = &s->ptr.depend[s->inds.num_dependencies++];
		*dep = (VkSubpassDependency) {
			.srcSubpass = find_subpass (d, index, s->rpi->subpasses),
			.dstSubpass = index,
			.srcStageMask = d->src.stage,
			.dstStageMask = d->dst.stage,
			.srcAccessMask = d->src.access,
			.dstAccessMask = d->dst.access,
			.dependencyFlags = d->flags,
		};
	}

	for (uint32_t i = 0; i < s->spi->num_pipelines; i++) {
		s->plc[i] = (VkGraphicsPipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pColorBlendState = &cbs[i],
			.subpass = index,
		};
		if (s->spi->base_pipeline) {
			init_plCreate (&s->plc[i], s->spi->base_pipeline, s);
		}
		init_plCreate (&s->plc[i], &s->spi->pipelines[i], s);
		pln[i] = s->spi->name;
		s->inds.num_graph_pipelines++;
	}

	__auto_type att = s->spi->attachments;
	if (!att) {
		return;
	}
	for (uint32_t i = 0; i < s->spi->num_pipelines; i++) {
		cbs[i].attachmentCount = att->num_color;
		cbs[i].pAttachments = &s->ptr.cbAttach[s->inds.num_colorblend];
	}
	s->spc->inputAttachmentCount = att->num_input;
	s->spc->pInputAttachments = &s->ptr.attachref[s->inds.num_attachmentrefs];
	for (uint32_t i = 0; i < att->num_input; i++) {
		init_arCreate (&att->input[i], s);
		s->inds.num_attachmentrefs++;
	}
	s->spc->colorAttachmentCount = att->num_color;
	s->spc->pColorAttachments = &s->ptr.attachref[s->inds.num_attachmentrefs];
	for (uint32_t i = 0; i < att->num_color; i++) {
		init_arCreate (&att->color[i], s);
		s->inds.num_attachmentrefs++;
		init_cbCreate (&att->color[i], s);
		s->inds.num_colorblend++;
	}
	if (att->resolve) {
		s->spc->pResolveAttachments
			= &s->ptr.attachref[s->inds.num_attachmentrefs];
		for (uint32_t i = 0; i < att->num_color; i++) {
			init_arCreate (&att->resolve[i], s);
			s->inds.num_attachmentrefs++;
		}
	}
	if (att->depth) {
		s->spc->pDepthStencilAttachment
			= &s->ptr.attachref[s->inds.num_attachmentrefs];
		init_arCreate (att->depth, s);
		s->inds.num_attachmentrefs++;
	}
	s->spc->preserveAttachmentCount = att->num_preserve;
	s->spc->pPreserveAttachments = &s->ptr.preserve[s->inds.num_preserve];
	for (uint32_t i = 0; i < att->num_preserve; i++) {
		s->ptr.preserve[s->inds.num_preserve]
			= find_attachment (&att->preserve[i], s);
		s->inds.num_preserve++;
	}
}

static void
init_atCreate (uint32_t index, qfv_attachmentinfo_t *attachments, objstate_t *s)
{
	__auto_type ati = &attachments[index];
	__auto_type atc = &s->ptr.attach[s->inds.num_attachments];
	__auto_type cvc = &s->ptr.clear[s->inds.num_attachments];

	*atc = (VkAttachmentDescription) {
		.flags = ati->flags,
		.format = ati->format,
		.samples = ati->samples,
		.loadOp = ati->loadOp,
		.storeOp = ati->storeOp,
		.stencilLoadOp = ati->stencilLoadOp,
		.stencilStoreOp = ati->stencilStoreOp,
		.initialLayout = ati->initialLayout,
		.finalLayout = ati->finalLayout,
	};
	*cvc = ati->clearValue;
}

static void
init_rpCreate (uint32_t index, const qfv_renderinfo_t *rinfo, objstate_t *s)
{
	s->rpi = &rinfo->renderpasses[index];
	s->rpc = &s->ptr.rpCreate[s->inds.num_renderpasses];
	s->ptr.rpName[s->inds.num_renderpasses] = s->rpi->name;

	__auto_type attachments = &s->ptr.attach[s->inds.num_attachments];
	__auto_type subpasses = &s->ptr.subpass[s->inds.num_subpasses];
	__auto_type dependencies = &s->ptr.depend[s->inds.num_dependencies];

	for (uint32_t i = 0; i < s->rpi->num_attachments; i++) {
		init_atCreate (i, s->rpi->attachments, s);
		s->inds.num_attachments++;
	}

	uint32_t    num_dependencies = s->inds.num_dependencies;
	for (uint32_t i = 0; i < s->rpi->num_subpasses; i++) {
		init_spCreate (i, s->rpi->subpasses, s);
		s->inds.num_subpasses++;
	}
	num_dependencies = s->inds.num_dependencies - num_dependencies;

	*s->rpc = (VkRenderPassCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = s->rpi->num_attachments,
		.pAttachments = attachments,
		.subpassCount = s->rpi->num_subpasses,
		.pSubpasses = subpasses,
		.dependencyCount = num_dependencies,
		.pDependencies = dependencies,
	};
}

typedef struct {
	qfv_step_t *steps;
	qfv_render_t *renders;
	qfv_compute_t *computes;
	qfv_process_t *processes;
	qfv_renderpass_t *renderpasses;
	VkClearValue *clearvalues;
	qfv_subpass_t *subpasses;
	qfv_pipeline_t *pipelines;
	qfv_taskinfo_t *tasks;
	VkDescriptorSet *descriptorsets;
} jobptr_t;

static void
init_pipeline (qfv_pipeline_t *pl, qfv_pipelineinfo_t *plinfo,
			   jobptr_t *jp, objstate_t *s, int is_compute)
{
	__auto_type li = find_layout (&plinfo->layout, s);
	*pl = (qfv_pipeline_t) {
		.label = {
			.name = plinfo->name,
			.color = plinfo->color,
		},
		.bindPoint = is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE
								: VK_PIPELINE_BIND_POINT_GRAPHICS,
		.pipeline = is_compute ? s->ptr.cpl[s->inds.num_comp_pipelines]
							   : s->ptr.gpl[s->inds.num_graph_pipelines],
		.layout = li->layout,
		.task_count = plinfo->num_tasks,
		.tasks = &jp->tasks[s->inds.num_tasks],
		.descriptorsets = &jp->descriptorsets[s->inds.num_descriptorsets],
	};
	s->inds.num_tasks += plinfo->num_tasks;
	s->inds.num_descriptorsets += li->num_sets;
	for (uint32_t i = 0; i < li->num_sets; i++) {
		pl->descriptorsets[i] = 0;
	}
	for (uint32_t i = 0; i < pl->task_count; i++) {
		pl->tasks[i] = plinfo->tasks[i];
	}
}

static void
init_subpass (qfv_subpass_t *sp, qfv_subpassinfo_t *isp,
			  jobptr_t *jp, objstate_t *s)
{
	uint32_t    np = s->inds.num_graph_pipelines + s->inds.num_comp_pipelines;
	*sp = (qfv_subpass_t) {
		.label = {
			.name = isp->name,
			.color = isp->color,
		},
		.inherit = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		},
		.beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
				| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			.pInheritanceInfo = &sp->inherit,
		},
		.pipeline_count = isp->num_pipelines,
		.pipelines = &jp->pipelines[np],
	};
	for (uint32_t i = 0; i < isp->num_pipelines; i++) {
		init_pipeline (&sp->pipelines[i], &isp->pipelines[i], jp, s, 0);
		s->inds.num_graph_pipelines++;
	}
}

static void
init_renderpass (qfv_renderpass_t *rp, qfv_renderpassinfo_t *rpinfo,
				 jobptr_t *jp, objstate_t *s)
{
	*rp = (qfv_renderpass_t) {
		.vulkan_ctx = s->ctx,
		.label.name = rpinfo->name,
		.label.color = rpinfo->color,
		.subpass_count = rpinfo->num_subpasses,
		.subpasses = &jp->subpasses[s->inds.num_subpasses],
		.beginInfo = (VkRenderPassBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = s->ptr.rp[s->inds.num_renderpasses],
			.clearValueCount = rpinfo->num_attachments,
			.pClearValues = &jp->clearvalues[s->inds.num_attachments],
		},
		.subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
	};
	s->inds.num_attachments += rpinfo->num_attachments;
	for (uint32_t i = 0; i < rpinfo->num_subpasses; i++) {
		init_subpass (&rp->subpasses[i], &rpinfo->subpasses[i], jp, s);
		rp->subpasses[i].inherit.renderPass = rp->beginInfo.renderPass;
		rp->subpasses[i].inherit.subpass = i;
		s->inds.num_subpasses++;
	}
}

static void
init_render (qfv_render_t *rend, qfv_renderinfo_t *rinfo,
			 jobptr_t *jp, objstate_t *s)
{
	*rend = (qfv_render_t) {
		.label.color = rinfo->color,
		.label.name = rinfo->name,
		.num_renderpasses = rinfo->num_renderpasses,
		.renderpasses = &jp->renderpasses[s->inds.num_renderpasses],
	};
	s->inds.num_renderpasses += rinfo->num_renderpasses;
	for (uint32_t i = 0; i < rend->num_renderpasses; i++) {
		init_renderpass (&rend->renderpasses[i], &rinfo->renderpasses[i],
						 jp, s);
	}
	rend->active = &rend->renderpasses[0];
}

static void
init_compute (qfv_compute_t *comp, qfv_computeinfo_t *cinfo,
			  jobptr_t *jp, objstate_t *s)
{
	uint32_t    np = s->inds.num_graph_pipelines + s->inds.num_comp_pipelines;
	*comp = (qfv_compute_t) {
		.label.color = cinfo->color,
		.label.name = cinfo->name,
		.pipelines = &jp->pipelines[np],
		.pipeline_count = cinfo->num_pipelines,
	};
	for (uint32_t i = 0; i < cinfo->num_pipelines; i++) {
		init_pipeline (&comp->pipelines[i], &cinfo->pipelines[i], jp, s, 1);
		s->inds.num_comp_pipelines++;
	}
}

static void
init_process (qfv_process_t *proc, qfv_processinfo_t *pinfo,
			  jobptr_t *jp, objstate_t *s)
{
	*proc = (qfv_process_t) {
		.label.color = pinfo->color,
		.label.name = pinfo->name,
		.tasks = &jp->tasks[s->inds.num_tasks],
		.task_count = pinfo->num_tasks,
	};
	s->inds.num_tasks += pinfo->num_tasks;
	for (uint32_t i = 0; i < proc->task_count; i++) {
		proc->tasks[i] = pinfo->tasks[i];
	}
}

static void
init_step (uint32_t ind, jobptr_t *jp, objstate_t *s)
{
	__auto_type step = &jp->steps[s->inds.num_steps++];
	__auto_type sinfo = &s->jinfo->steps[ind];

	*step = (qfv_step_t) {
		.label.name = sinfo->name,
		.label.color = sinfo->color,
	};
	if (sinfo->render) {
		step->render = &jp->renders[s->inds.num_render++];
		init_render (step->render, sinfo->render, jp, s);
	}
	if (sinfo->compute) {
		step->compute = &jp->computes[s->inds.num_compute++];
		init_compute (step->compute, sinfo->compute, jp, s);
	}
	if (sinfo->process) {
		step->process = &jp->processes[s->inds.num_process++];
		init_process (step->process, sinfo->process, jp, s);
	}
}

static void
init_job (vulkan_ctx_t *ctx, objcount_t *counts, objstate_t s)
{
	__auto_type rctx = ctx->render_context;

	size_t      size = sizeof (qfv_job_t);

	size += counts->num_renderpasses * sizeof (VkRenderPass);
	size += counts->num_graph_pipelines * sizeof (VkPipeline);
	size += counts->num_comp_pipelines * sizeof (VkPipeline);
	size += s.inds.num_layouts * sizeof (VkPipelineLayout);

	size += counts->num_steps * sizeof (qfv_step_t);
	size += counts->num_render * sizeof (qfv_render_t);
	size += counts->num_compute * sizeof (qfv_compute_t);
	size += counts->num_process * sizeof (qfv_process_t);
	size += counts->num_renderpasses * sizeof (qfv_renderpass_t);
	size += counts->num_attachments * sizeof (VkClearValue);
	size += counts->num_subpasses * sizeof (qfv_subpass_t);
	size += counts->num_graph_pipelines * sizeof (qfv_pipeline_t);
	size += counts->num_comp_pipelines * sizeof (qfv_pipeline_t);
	size += counts->num_tasks * sizeof (qfv_taskinfo_t);
	size += counts->num_descriptorsets * sizeof (VkDescriptorSet);

	rctx->job = malloc (size);
	__auto_type job = rctx->job;
	*job = (qfv_job_t) {
		.num_renderpasses = counts->num_renderpasses,
		.num_pipelines = counts->num_graph_pipelines
						 + counts->num_comp_pipelines,
		.num_layouts = s.inds.num_layouts,
		.num_steps = counts->num_steps,
		.commands = DARRAY_STATIC_INIT (16),
	};
	job->renderpasses = (VkRenderPass *) &job[1];
	job->pipelines = (VkPipeline *) &job->renderpasses[job->num_renderpasses];
	job->layouts = (VkPipelineLayout *) &job->pipelines[job->num_pipelines];
	job->steps = (qfv_step_t *) &job->layouts[job->num_layouts];
	__auto_type rn = (qfv_render_t *) &job->steps[job->num_steps];
	__auto_type cp = (qfv_compute_t *) &rn[counts->num_render];
	__auto_type pr = (qfv_process_t *) &cp[counts->num_compute];
	__auto_type rp = (qfv_renderpass_t *) &pr[counts->num_process];
	__auto_type cv = (VkClearValue *) &rp[counts->num_renderpasses];
	__auto_type sp = (qfv_subpass_t *) &cv[counts->num_attachments];
	__auto_type pl = (qfv_pipeline_t *) &sp[counts->num_subpasses];
	__auto_type ti = (qfv_taskinfo_t *) &pl[job->num_pipelines];
	__auto_type ds = (VkDescriptorSet *) &ti[counts->num_tasks];
	jobptr_t jp = {
		.steps = job->steps,
		.renders = rn,
		.computes = cp,
		.processes = pr,
		.renderpasses = rp,
		.clearvalues = cv,
		.subpasses = sp,
		.pipelines = pl,
		.tasks = ti,
		.descriptorsets = ds,
	};

	for (uint32_t i = 0; i < job->num_renderpasses; i++) {
		job->renderpasses[i] = s.ptr.rp[i];
	}
	for (uint32_t i = 0; i < job->num_pipelines; i++) {
		// compute pipelines come immediately after the graphics pipelines
		job->pipelines[i] = s.ptr.gpl[i];
	}
	for (uint32_t i = 0; i < s.inds.num_layouts; i++) {
		job->layouts[i] = s.ptr.layouts[i].layout;
	}
	memcpy (cv, s.ptr.clear, counts->num_attachments * sizeof (VkClearValue));

	uint32_t    num_layouts = s.inds.num_layouts;
	s.inds = (objcount_t) {};
	s.inds.num_layouts = num_layouts;
	for (uint32_t i = 0; i < job->num_steps; i++) {
		init_step (i, &jp, &s);
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	VkCommandPoolCreateInfo poolCInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = device->queue.queueFamily,
	};
	dfunc->vkCreateCommandPool (device->dev, &poolCInfo, 0, &job->command_pool);
}

static void
create_step_render_objects (uint32_t index, const qfv_stepinfo_t *step,
							objstate_t *s)
{
	__auto_type rinfo = step->render;
	if (!rinfo) {
		return;
	}
	for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
		s->ptr.pl_counts[s->inds.num_renderpasses] = 0;
		init_rpCreate (i, rinfo, s);
		s->inds.num_renderpasses++;
	}
}

static void
create_step_compute_objects (uint32_t index, const qfv_stepinfo_t *step,
							 objstate_t *s)
{
	__auto_type cinfo = step->compute;
	if (!cinfo) {
		return;
	}

	uint32_t    base = s->inds.num_graph_pipelines;
	for (uint32_t i = 0; i < cinfo->num_pipelines; i++) {
		__auto_type pli = &cinfo->pipelines[i];
		__auto_type plc = &s->ptr.cplCreate[s->inds.num_comp_pipelines];
		__auto_type li = find_layout (&pli->layout, s);
		s->ptr.plName[base + s->inds.num_comp_pipelines] = pli->name;
		*plc = (VkComputePipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.flags = pli->flags,
			.stage = *pli->compute_stage,
			.layout = li->layout,
		};
		plc->stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		s->inds.num_comp_pipelines++;
		s->inds.num_descriptorsets += li->num_sets;
	}
}

static void
create_step_process_objects (uint32_t index, const qfv_stepinfo_t *step,
							 objstate_t *s)
{
	__auto_type pinfo = step->process;
	if (!pinfo) {
		return;
	}
	// nothing to create at this stage
}

static void
create_objects (vulkan_ctx_t *ctx, objcount_t *counts)
{
	__auto_type rctx = ctx->render_context;
	__auto_type jinfo = rctx->jobinfo;

	VkRenderPass renderpasses[counts->num_renderpasses];
	VkPipeline pipelines[counts->num_graph_pipelines
						 + counts->num_comp_pipelines];
	VkRenderPassCreateInfo rpCreate[counts->num_renderpasses];
	VkAttachmentDescription attach[counts->num_attachments];
	VkClearValue clear[counts->num_attachments];
	VkSubpassDescription subpass[counts->num_subpasses];
	VkSubpassDependency depend[counts->num_dependencies];
	VkAttachmentReference attachref[counts->num_attachmentrefs];
	VkPipelineColorBlendAttachmentState cbAttach[counts->num_colorblend];
	uint32_t    preserve[counts->num_preserve];
	const char *rpName[counts->num_renderpasses];
	const char *plName[counts->num_graph_pipelines
					   + counts->num_comp_pipelines];
	VkComputePipelineCreateInfo cplCreate[counts->num_comp_pipelines];
	VkGraphicsPipelineCreateInfo gplCreate[counts->num_graph_pipelines];
	VkPipelineColorBlendStateCreateInfo cbState[counts->num_graph_pipelines];
	qfv_layoutinfo_t layouts[counts->num_graph_pipelines
							 + counts->num_comp_pipelines];
	uint32_t    pl_counts[counts->num_renderpasses];

	exprctx_t   ectx = { .hashctx = &ctx->script_context->hashctx };
	objstate_t  s = {
		.ptr = {
			.rpCreate  = rpCreate,
			.attach    = attach,
			.clear     = clear,
			.subpass   = subpass,
			.depend    = depend,
			.attachref = attachref,
			.cbAttach  = cbAttach,
			.preserve  = preserve,
			.rpName    = rpName,
			.plName    = plName,
			.cplCreate = cplCreate,
			.gplCreate = gplCreate,
			.cbState   = cbState,
			.layouts   = layouts,
			.pl_counts = pl_counts,
			.rp        = renderpasses,
			.gpl       = pipelines,
			.cpl       = pipelines + counts->num_graph_pipelines,
		},
		.ctx = ctx,
		.jinfo = jinfo,
		.symtab = QFV_CreateSymtab (jinfo->plitem, "properties", 0, 0, &ectx),
	};
	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		create_step_render_objects (i, &jinfo->steps[i], &s);
	}
	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		create_step_compute_objects (i, &jinfo->steps[i], &s);
	}
	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		create_step_process_objects (i, &jinfo->steps[i], &s);
	}
	if (s.inds.num_renderpasses != counts->num_renderpasses
		|| s.inds.num_attachments != counts->num_attachments
		|| s.inds.num_subpasses != counts->num_subpasses
		|| s.inds.num_dependencies != counts->num_dependencies
		|| s.inds.num_attachmentrefs != counts->num_attachmentrefs
		|| s.inds.num_colorblend != counts->num_colorblend
		|| s.inds.num_preserve != counts->num_preserve
		|| s.inds.num_graph_pipelines != counts->num_graph_pipelines
		|| s.inds.num_comp_pipelines != counts->num_comp_pipelines) {
		Sys_Error ("create_objects: something was missed");
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    plInd = 0;
	for (uint32_t i = 0; i < counts->num_renderpasses; i++) {
		dfunc->vkCreateRenderPass (device->dev, &s.ptr.rpCreate[i], 0,
								   &renderpasses[i]);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_RENDER_PASS,
							 renderpasses[i],
							 va (ctx->va_ctx, "renderpass:%s", rpName[i]));
		for (uint32_t j = 0; j < pl_counts[i]; j++) {
			s.ptr.gplCreate[plInd++].renderPass = renderpasses[i];
		}
	}
	if (s.inds.num_graph_pipelines) {
		dfunc->vkCreateGraphicsPipelines (device->dev, 0,
										  s.inds.num_graph_pipelines,
										  s.ptr.gplCreate, 0, pipelines);
	}
	if (s.inds.num_comp_pipelines) {
		__auto_type p = &pipelines[s.inds.num_graph_pipelines];
		dfunc->vkCreateComputePipelines (device->dev, 0,
										 s.inds.num_comp_pipelines,
										 s.ptr.cplCreate, 0, p);
	}
	for (uint32_t i = 0;
		 i < s.inds.num_graph_pipelines + s.inds.num_comp_pipelines; i++) {
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_PIPELINE, pipelines[i],
							 va (ctx->va_ctx, "pipeline:%s", plName[i]));
	}

	counts->num_descriptorsets = s.inds.num_descriptorsets;
	init_job (ctx, counts, s);
}

void
QFV_BuildRender (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;

	objcount_t  counts = {};
	count_stuff (rctx->jobinfo, &counts);

	create_objects (ctx, &counts);
	create_resources (ctx, &counts);
}

static VkImageView __attribute__((pure, used))
find_imageview (qfv_reference_t *ref, qfv_renderctx_t *rctx)
{
	__auto_type jinfo = rctx->jobinfo;
	__auto_type job = rctx->job;
	const char *name = ref->name;

	if (strncmp (name, "$imageviews.", 7) == 0) {
		name += 7;
	}

	for (uint32_t i = 0; i < jinfo->num_imageviews; i++) {
		__auto_type vi = &jinfo->imageviews[i];
		__auto_type vo = &job->image_views[i];
		if (strcmp (name, vi->name) == 0) {
			return vo->image_view.view;
		}
	}
	Sys_Error ("%d:invalid imageview: %s", ref->line, ref->name);
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
