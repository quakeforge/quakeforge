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

static void
run_pipeline (qfv_pipeline_t *pipeline, VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCmdBindPipeline (cmd, pipeline->bindPoint, pipeline->pipeline);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &pipeline->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &pipeline->scissor);
	if (pipeline->num_descriptor_sets) {
		dfunc->vkCmdBindDescriptorSets (cmd, pipeline->bindPoint,
										pipeline->layout,
										pipeline->first_descriptor_set,
										pipeline->num_descriptor_sets,
										pipeline->descriptor_sets,
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
run_subpass (qfv_subpass_t_ *sp, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type cmd = sp->cmd;
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

void
QFV_RunRenderPass (qfv_renderpass_t_ *rp, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type cmd = rp->cmd;

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
		run_subpass (sp, ctx);
		dfunc->vkCmdExecuteCommands (cmd, 1, &sp->cmd);
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
	rctx->renderinfo = QFV_ParseRenderInfo (ctx, item, rctx);
	if (rctx->renderinfo) {
		rctx->renderinfo->plitem = item;
	}
}

typedef struct {
	uint32_t    num_images;
	uint32_t    num_views;
	uint32_t    num_layouts;

	uint32_t    num_renderpasses;
	uint32_t    num_attachments;
	uint32_t    num_subpasses;
	uint32_t    num_dependencies;
	uint32_t    num_attachmentrefs;
	uint32_t    num_colorblend;
	uint32_t    num_preserve;
	uint32_t    num_pipelines;
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
	counts->num_pipelines += spi->num_pipelines;
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
count_stuff (qfv_renderinfo_t *renderinfo, objcount_t *counts)
{
	counts->num_images += renderinfo->num_images;
	counts->num_views += renderinfo->num_views;
	counts->num_renderpasses += renderinfo->num_renderpasses;
	for (uint32_t i = 0; i < renderinfo->num_renderpasses; i++) {
		count_rp_stuff (&renderinfo->renderpasses[i], counts);
	}
}

static void
create_resources (vulkan_ctx_t *ctx, objcount_t *counts)
{
	__auto_type rctx = ctx->render_context;
	__auto_type rinfo = rctx->renderinfo;
	__auto_type render = rctx->render;

	render->resources = malloc (sizeof(qfv_resource_t)
								+ counts->num_images * sizeof (qfv_resobj_t)
								+ counts->num_views * sizeof (qfv_resobj_t));
	render->images = (qfv_resobj_t *) &render->resources[1];
	render->image_views = &render->images[counts->num_images];

	render->resources[0] = (qfv_resource_t) {
		.name = "render",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = counts->num_images + counts->num_views,
		.objects = render->images,
	};
	for (uint32_t i = 0; i < counts->num_images; i++) {
		__auto_type img = &rinfo->images[i];
		render->images[i] = (qfv_resobj_t) {
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
	for (uint32_t i = 0; i < counts->num_views; i++) {
		__auto_type view = &rinfo->views[i];
		render->image_views[i] = (qfv_resobj_t) {
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
			__auto_type image = rinfo->output.image;
			render->image_views[i].image_view.external_image = image;
			render->image_views[i].image_view.image = -1;
		} else {
			qfv_resobj_t *img = 0;
			for (uint32_t j = 0; j < rinfo->num_images; j++) {
				if (strcmp (view->image.name, rinfo->images[j].name) == 0) {
					img = &render->images[j];
				}
			}
			if (img) {
				uint32_t    ind = img - render->resources->objects;
				render->image_views[i].image_view.image = ind;
			} else {
				Sys_Printf ("%d: unknown image reference: %s\n",
							view->image.line, view->image.name);
				error = 1;
			}
		}
	}
	if (error) {
		free (render->resources);
		render->resources = 0;
		return;
	}
	QFV_CreateResource (ctx->device, render->resources);
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
	const char **plName;
	VkGraphicsPipelineCreateInfo *plCreate;
	VkPipelineColorBlendStateCreateInfo *cbState;
	qfv_layoutinfo_t *layouts;

	VkPipeline *pl;
	VkRenderPass *rp;
} objptr_t;

typedef struct {
	objcount_t  inds;
	objptr_t    ptr;
	vulkan_ctx_t *ctx;
	qfv_renderinfo_t *rinfo;
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
	for (uint32_t i = 0; i < s->rinfo->num_descriptorsets; i++) {
		__auto_type ds = &s->rinfo->descriptorsets[i];
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

static VkPipelineLayout
find_layout (const qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->inds.num_layouts; i++) {
		if (strcmp (s->ptr.layouts[i].name, ref->name) == 0) {
			return s->ptr.layouts[i].layout;
		}
	}
	if (!QFV_ParseLayoutInfo (s->ctx, s->rinfo->memsuper, s->symtab, ref->name,
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
	return li->layout;
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
		plc->layout = find_layout (&pli->layout, s);
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
	s->plc = &s->ptr.plCreate[s->inds.num_pipelines];
	s->spc = &s->ptr.subpass[s->inds.num_subpasses];
	__auto_type pln = &s->ptr.plName[s->inds.num_pipelines];
	__auto_type cbs = &s->ptr.cbState[s->inds.num_pipelines];

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
		s->inds.num_pipelines++;
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

static void
init_pipeline (qfv_pipeline_t *pl, vulkan_ctx_t *ctx, qfv_pipelineinfo_t *ipl,
			   objstate_t *s)
{
	*pl = (qfv_pipeline_t) {
		.label = {
			.name = ipl->name,
			.color = ipl->color,
		},
		.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.pipeline = s->ptr.pl[s->inds.num_pipelines],
		.layout = find_layout (&ipl->layout, s),
		.task_count = ipl->num_tasks,
		.tasks = ipl->tasks,
	};
}

static void
init_subpass (qfv_subpass_t_ *sp, vulkan_ctx_t *ctx, qfv_subpassinfo_t *isp,
			  qfv_pipeline_t *pl, objstate_t *s)
{
	*sp = (qfv_subpass_t_) {
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
		.pipelines = &pl[s->inds.num_pipelines],
	};
	for (uint32_t i = 0; i < isp->num_pipelines; i++) {
		init_pipeline (&sp->pipelines[i], ctx, &isp->pipelines[i], s);
		s->inds.num_pipelines++;
	}
}

static void
init_renderpass (qfv_renderpass_t_ *rp, vulkan_ctx_t *ctx,
				 qfv_renderpassinfo_t *irp, VkClearValue *cv,
				 qfv_subpass_t_ *sp, qfv_pipeline_t *pl, objstate_t *s)
{
	*rp = (qfv_renderpass_t_) {
		.vulkan_ctx = ctx,
		.label.name = irp->name,
		.label.color = irp->color,
		.subpass_count = irp->num_subpasses,
		.subpasses = &sp[s->inds.num_subpasses],
		.beginInfo = (VkRenderPassBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = s->ptr.rp[s->inds.num_renderpasses],
			.clearValueCount = irp->num_attachments,
			.pClearValues = &cv[s->inds.num_attachments],
		},
		.subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
	};
	s->inds.num_attachments += irp->num_attachments;
	for (uint32_t i = 0; i < irp->num_subpasses; i++) {
		init_subpass (&rp->subpasses[i], ctx, &irp->subpasses[i], pl, s);
		rp->subpasses[i].inherit.renderPass = rp->beginInfo.renderPass;
		rp->subpasses[i].inherit.subpass = i;
		s->inds.num_subpasses++;
	}
}

static void
init_render (vulkan_ctx_t *ctx, objcount_t *counts, objstate_t s)
{
	__auto_type rctx = ctx->render_context;
	__auto_type rinfo = rctx->renderinfo;
	__auto_type render = rctx->render;
	size_t      size = 0;
	size += counts->num_renderpasses * sizeof (qfv_renderpass_t_);
	size += counts->num_attachments * sizeof (VkClearValue);
	size += counts->num_subpasses * sizeof (qfv_subpass_t_);
	size += counts->num_pipelines * sizeof (qfv_pipeline_t);

	__auto_type rp = (qfv_renderpass_t_ *) calloc (1, size);
	__auto_type cv = (VkClearValue *) &rp[counts->num_renderpasses];
	__auto_type sp = (qfv_subpass_t_ *) &cv[counts->num_attachments];
	__auto_type pl = (qfv_pipeline_t *) &sp[counts->num_subpasses];
	memcpy (cv, s.ptr.clear, counts->num_attachments * sizeof (VkClearValue));
	uint32_t    num_layouts = s.inds.num_layouts;
	s.inds = (objcount_t) {};
	s.inds.num_layouts = num_layouts;
	for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
		init_renderpass (&rp[i], ctx, &rinfo->renderpasses[i], cv, sp, pl, &s);
		s.inds.num_renderpasses++;
	}

	render->num_renderpasses = rinfo->num_renderpasses;
	render->renderpasses = rp;
}

static void
create_renderpasses (vulkan_ctx_t *ctx, objcount_t *counts)
{
	__auto_type rctx = ctx->render_context;
	__auto_type rinfo = rctx->renderinfo;

	exprctx_t   ectx = {
		.hashctx = &ctx->script_context->hashctx,
	};

	VkRenderPass rp[counts->num_renderpasses];
	VkPipeline pl[counts->num_pipelines];
	VkRenderPassCreateInfo rpCreate[counts->num_renderpasses];
	VkAttachmentDescription attach[counts->num_attachments];
	VkClearValue clear[counts->num_attachments];
	VkSubpassDescription subpass[counts->num_subpasses];
	VkSubpassDependency depend[counts->num_dependencies];
	VkAttachmentReference attachref[counts->num_attachmentrefs];
	VkPipelineColorBlendAttachmentState cbAttach[counts->num_colorblend];
	uint32_t    preserve[counts->num_preserve];
	const char *plName[counts->num_pipelines];
	VkGraphicsPipelineCreateInfo plCreate[counts->num_pipelines];
	VkPipelineColorBlendStateCreateInfo cbState[counts->num_pipelines];
	qfv_layoutinfo_t layouts[counts->num_pipelines];
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
			.plName    = plName,
			.plCreate  = plCreate,
			.cbState   = cbState,
			.layouts   = layouts,
			.rp        = rp,
			.pl        = pl,
		},
		.ctx = ctx,
		.rinfo = rinfo,
		.symtab = QFV_CreateSymtab (rinfo->plitem, "properties", 0, 0, &ectx),
	};
	for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
		init_rpCreate (i, rinfo, &s);
		s.inds.num_renderpasses++;
	}
	if (s.inds.num_renderpasses != counts->num_renderpasses
		|| s.inds.num_attachments != counts->num_attachments
		|| s.inds.num_subpasses != counts->num_subpasses
		|| s.inds.num_dependencies != counts->num_dependencies
		|| s.inds.num_attachmentrefs != counts->num_attachmentrefs
		|| s.inds.num_colorblend != counts->num_colorblend
		|| s.inds.num_preserve != counts->num_preserve
		|| s.inds.num_pipelines != counts->num_pipelines) {
		Sys_Error ("create_renderpasses: something was missed");
	}

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	uint32_t    plInd = 0;
	for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
		__auto_type rpi = &rinfo->renderpasses[i];
		dfunc->vkCreateRenderPass (device->dev, &s.ptr.rpCreate[i], 0, &rp[i]);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_RENDER_PASS, rp[i],
							 va (ctx->va_ctx, "renderpass:%s", rpi->name));
		for (uint32_t j = 0; j < rpi->num_subpasses; j++) {
			__auto_type spi = &rpi->subpasses[j];
			for (uint32_t k = 0; k < spi->num_pipelines; k++) {
				s.ptr.plCreate[plInd++].renderPass = rp[i];
			}
		}
	}
	dfunc->vkCreateGraphicsPipelines (device->dev, 0, s.inds.num_pipelines,
									  s.ptr.plCreate, 0, pl);
	for (uint32_t i = 0; i < s.inds.num_pipelines; i++) {
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_PIPELINE, pl[i],
							 va (ctx->va_ctx, "pipeline:%s", s.ptr.plName[i]));
	}

	rinfo->num_layouts = s.inds.num_layouts;
	size_t      layout_size = rinfo->num_layouts * sizeof (qfv_layoutinfo_t);
	rinfo->layouts = cmemalloc (rinfo->memsuper, layout_size);
	memcpy (rinfo->layouts, s.ptr.layouts, layout_size);

	init_render (ctx, counts, s);
}

void
QFV_BuildRender (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;

	rctx->render = calloc (1, sizeof (qfv_render_t));

	objcount_t  counts = {};
	count_stuff (rctx->renderinfo, &counts);
	create_resources (ctx, &counts);
	create_renderpasses (ctx, &counts);
}

static VkImageView
find_view (qfv_reference_t *ref, qfv_renderctx_t *rctx)
{
	__auto_type rinfo = rctx->renderinfo;
	__auto_type render = rctx->render;
	const char *name = ref->name;

	if (strncmp (name, "$views.", 7) == 0) {
		name += 7;
	}

	for (uint32_t i = 0; i < rinfo->num_views; i++) {
		__auto_type vi = &rinfo->views[i];
		__auto_type vo = &render->image_views[i];
		if (strcmp (name, vi->name) == 0) {
			return vo->image_view.view;
		}
	}
	Sys_Error ("%d:invalid view: %s", ref->line, ref->name);
}

void
QFV_DestroyFramebuffer (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	__auto_type rctx = ctx->render_context;
	__auto_type render = rctx->render;
	__auto_type rp = &render->renderpasses[0];

	if (rp->beginInfo.framebuffer) {
		VkFramebuffer framebuffer = rp->beginInfo.framebuffer;
		rp->beginInfo.framebuffer = 0;
		dfunc->vkDestroyFramebuffer (device->dev, framebuffer, 0);
	}
}

void
QFV_CreateFramebuffer (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;
	__auto_type rinfo = rctx->renderinfo;
	__auto_type render = rctx->render;
	__auto_type rpInfo = &rinfo->renderpasses[0];
	__auto_type rp = &render->renderpasses[0];

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
		attachments[i] = find_view (&rpInfo->attachments[i].view, rctx);
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
	if (rctx->render) {
		QFV_DestroyFramebuffer (ctx); //FIXME do properly
		__auto_type r = rctx->render;
		if (r->resources) {
			QFV_DestroyResource (ctx->device, r->resources);
			free (r->resources);
		}
		for (uint32_t i = 0; i < r->num_renderpasses; i++) {
			__auto_type rp = &r->renderpasses[i];
			for (uint32_t j = 0; j < rp->subpass_count; j++) {
				__auto_type sp = &rp->subpasses[j];
				for (uint32_t k = 0; k < sp->pipeline_count; k++) {
					__auto_type pl = &sp->pipelines[k];
					dfunc->vkDestroyPipeline (device->dev, pl->pipeline, 0);
				}
			}
			dfunc->vkDestroyRenderPass (device->dev,
										rp->beginInfo.renderPass, 0);
		}
		free (rctx->render);
	}
	if (rctx->renderinfo) {
		__auto_type rinfo = rctx->renderinfo;
		for (uint32_t i = 0; i < rinfo->num_descriptorsets; i++) {
			__auto_type setLayout = rinfo->descriptorsets[i].setLayout;
			dfunc->vkDestroyDescriptorSetLayout (device->dev, setLayout, 0);
		}
		for (uint32_t i = 0; i < rinfo->num_layouts; i++) {
			__auto_type layout = rinfo->layouts[i].layout;
			dfunc->vkDestroyPipelineLayout (device->dev, layout, 0);
		}
		delete_memsuper (rinfo->memsuper);
	}
	if (rctx->task_functions.tab) {
		Hash_DelTable (rctx->task_functions.tab);
	}
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
