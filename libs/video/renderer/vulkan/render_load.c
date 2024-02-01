/*
	render_load.c

	Vulkan render manager loading/creation

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
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/pipeline.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/swapchain.h"
#include "vid_vulkan.h"

#include "vkparse.h"

static qfv_output_t
get_output (vulkan_ctx_t *ctx, plitem_t *item)
{
	qfZoneScoped (true);
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
QFV_LoadRenderInfo (vulkan_ctx_t *ctx, plitem_t *item)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	auto output = get_output (ctx, item);
	Vulkan_Script_SetOutput (ctx, &output);
	rctx->jobinfo = QFV_ParseJobInfo (ctx, item, rctx);
	if (rctx->jobinfo) {
		rctx->jobinfo->plitem = item;
	}
}

void
QFV_LoadSamplerInfo (vulkan_ctx_t *ctx, plitem_t *item)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	rctx->samplerinfo = QFV_ParseSamplerInfo (ctx, item, rctx);
	if (rctx->samplerinfo) {
		rctx->samplerinfo->plitem = item;
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

	uint32_t    num_ds_indices;
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
	counts->num_attachments += rpi->framebuffer.num_attachments;
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
	if (step->render && step->compute && !step->process) {
		Sys_Error ("%s: invalid step: must have process for render+compute",
				   step->name);
	}
	if (!step->render && !step->compute && !step->process) {
		Sys_Error ("%s: invalid step: must have at least one of "
				   "process/render/compute", step->name);
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
	counts->num_tasks += jobinfo->newscene_num_tasks;
	counts->num_tasks += jobinfo->init_num_tasks;
}

static qfv_imageinfo_t * __attribute__((pure))
find_imageinfo (qfv_jobinfo_t *jobinfo, const qfv_reference_t *ref)
{
	if (strcmp (ref->name, "$output.image") == 0) {
		//auto image = jinfo->output.image;
		//job->image_views[i].image_view.external_image = image;
		//job->image_views[i].image_view.image = -1;
	} else {
		for (uint32_t i = 0; i < jobinfo->num_images; i++) {
			auto img = &jobinfo->images[i];
			if (strcmp (ref->name, img->name) == 0) {
				return img;
			}
		}
	}
	Sys_Printf ("%d: unknown image reference: %s\n", ref->line, ref->name);
	return 0;
}

static qfv_imageviewinfo_t * __attribute__((pure))
find_imageviewinfo (qfv_jobinfo_t *jobinfo, const qfv_reference_t *ref)
{
	for (uint32_t i = 0; i < jobinfo->num_imageviews; i++) {
		auto imgview = &jobinfo->imageviews[i];
		if (strcmp (ref->name, imgview->name) == 0) {
			return imgview;
		}
	}
	Sys_Printf ("%d: unknown image view reference: %s\n", ref->line, ref->name);
	return 0;
}

static void
create_resources (vulkan_ctx_t *ctx, qfv_renderpass_t *rp)
{
	auto rctx = ctx->render_context;
	auto jinfo = rctx->jobinfo;

	auto fbi = rp->framebufferinfo;
	uint32_t num_attachments = 0;
	for (uint32_t i = 0; i < fbi->num_attachments; i++) {
		auto attach = &fbi->attachments[i];
		if (!attach->external) {
			num_attachments++;
		}
	}
	if (!num_attachments) {
		rp->resources = 0;
		return;
	}

	size_t      size = sizeof (qfv_resource_t);
	size += sizeof (qfv_resobj_t [num_attachments]);
	size += sizeof (qfv_resobj_t [num_attachments]);

	rp->resources = calloc (1, size);
	auto images = (qfv_resobj_t *) &rp->resources[1];
	auto image_views = &images[num_attachments];

	rp->resources[0] = (qfv_resource_t) {
		.name = rp->label.name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 * num_attachments,
		.objects = images,
	};
	int         error = 0;
	for (uint32_t i = 0; i < num_attachments; i++) {
		auto attach = &fbi->attachments[i];
		auto imgview = find_imageviewinfo (jinfo, &attach->view);
		if (!imgview) {
			error = 1;
			continue;
		}
		auto img = find_imageinfo (jinfo, &imgview->image);
		if (!img) {
			error = 1;
			continue;
		}
		images[i] = (qfv_resobj_t) {
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
		img->object = &images[i];

		image_views[i] = (qfv_resobj_t) {
			.name = imgview->name,
			.type = qfv_res_image_view,
			.image_view = {
				.image = img->object - rp->resources->objects,
				.flags = imgview->flags,
				.type = imgview->viewType,
				.format = imgview->format,
				.components = imgview->components,
				.subresourceRange = imgview->subresourceRange,
			},
		};
		if (!image_views[i].image_view.format) {
			image_views[i].image_view.format = img->object->image.format;
		}
		imgview->object = &image_views[i];
	}
	if (error) {
		free (rp->resources);
		rp->resources = 0;
		return;
	}
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
	for (uint32_t i = 0; i < spind; i++) {
		__auto_type s = &subpasses[i];
		if (strcmp (d->name, s->name) == 0) {
			return i;
		}
	}
	Sys_Error ("invalid dependency: [%d] %s", spind, d->name);
}

static uint32_t __attribute__((pure))
find_ds_index (const qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->jinfo->num_dslayouts; i++) {
		__auto_type ds = &s->jinfo->dslayouts[i];
		if (strcmp (ds->name, ref->name) == 0) {
			return i;
		}
	}
	Sys_Error ("%s.%s:%d: invalid descriptor set layout: %s",
			   s->rpi->name, s->spi->name, ref->line, ref->name);
}

static VkDescriptorSetLayout
find_descriptorSet (const qfv_reference_t *ref, objstate_t *s)
{
	auto ds = &s->jinfo->dslayouts[find_ds_index (ref, s)];
	if (ds->setLayout) {
		return ds->setLayout;
	}

	VkDescriptorSetLayoutCreateInfo cInfo = {
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = ds->flags,
		.bindingCount = ds->num_bindings,
		.pBindings = ds->bindings,
	};
	qfv_device_t *device = s->ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	dfunc->vkCreateDescriptorSetLayout (device->dev, &cInfo, 0, &ds->setLayout);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						 ds->setLayout, va (s->ctx->va_ctx, "descriptorSet:%s",
											ds->name));
	return ds->setLayout;
}

#define RUP(x,a) (((x) + ((a) - 1)) & ~((a) - 1))

static uint32_t
parse_pushconstantrange (VkPushConstantRange *range,
						 qfv_pushconstantrangeinfo_t *pushconstantrange,
						 uint32_t offset, objstate_t *s)
{
	uint32_t    range_offset = ~0u;
	for (uint32_t i = 0; i < pushconstantrange->num_pushconstants; i++) {
		__auto_type pushconstant = & pushconstantrange->pushconstants[i];
		uint32_t    size = 0;
		if (pushconstant->offset != ~0u) {
			offset = pushconstant->offset;
		}
		if (pushconstant->size != ~0u) {
			size = pushconstant->size;
		} else {
			switch (pushconstant->type) {
				case qfv_float:
				case qfv_int:
				case qfv_uint:
					size = sizeof (int32_t);
					offset = RUP (offset, sizeof (int32_t));
					break;
				case qfv_vec3:
					size = sizeof (vec3_t);
					offset = RUP (offset, sizeof (vec4f_t));
					break;
				case qfv_vec4:
					size = sizeof (vec4f_t);
					offset = RUP (offset, sizeof (vec4f_t));
					break;
				case qfv_mat4:
					size = sizeof (mat4f_t);
					offset = RUP (offset, sizeof (vec4f_t));
					break;
				default:
					Sys_Error ("%s.%s:%s:%d invalid type: %d",
							   s->rpi->name, s->spi->name, pushconstant->name,
							   pushconstant->line, pushconstant->type);
			}
		}
		if (range_offset == ~0u) {
			range_offset = offset;
		}
		offset += size;
	}
	*range = (VkPushConstantRange) {
		.stageFlags = pushconstantrange->stageFlags,
		.offset = range_offset,
		.size = offset - range_offset,
	};
	return offset;
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
	VkPushConstantRange ranges[li->num_pushconstantranges];
	uint32_t    offset = 0;
	for (uint32_t i = 0; i < li->num_pushconstantranges; i++) {
		offset = parse_pushconstantrange (&ranges[i],
										  &li->pushconstantranges[i],
										  offset, s);
	}
	VkPipelineLayoutCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = li->num_sets,
		.pSetLayouts = sets,
		.pushConstantRangeCount = li->num_pushconstantranges,
		.pPushConstantRanges = ranges,
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
		s->inds.num_ds_indices += li->num_sets;
	}
}

static uint32_t __attribute__((pure))
find_attachment (qfv_reference_t *ref, objstate_t *s)
{
	for (uint32_t i = 0; i < s->rpi->framebuffer.num_attachments; i++) {
		__auto_type a = &s->rpi->framebuffer.attachments[i];
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
		pln[i] = s->spi->pipelines[i].name;
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
	auto rctx = s->ctx->render_context;
	auto ati = &attachments[index];
	auto atc = &s->ptr.attach[s->inds.num_attachments];
	auto cvc = &s->ptr.clear[s->inds.num_attachments];

	if (ati->external) {
		bool found = false;
		for (size_t i = 0; i < rctx->external_attachments.size; i++) {
			auto ext = rctx->external_attachments.a[i];
			if (!strcmp (ati->external, ext->name)) {
				found = true;
				*atc = (VkAttachmentDescription) {
					.flags = ext->flags,
					.format = ext->format,
					.samples = ext->samples,
					.loadOp = ext->loadOp,
					.storeOp = ext->storeOp,
					.stencilLoadOp = ext->stencilLoadOp,
					.stencilStoreOp = ext->stencilStoreOp,
					.initialLayout = ext->initialLayout,
					.finalLayout = ext->finalLayout,
				};
			}
		}
		if (!found) {
			Sys_Error ("%d:%s: invalid external attachment: %s",
					   ati->line, ati->name, ati->external);
		}
	} else {
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
	}
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

	for (uint32_t i = 0; i < s->rpi->framebuffer.num_attachments; i++) {
		init_atCreate (i, s->rpi->framebuffer.attachments, s);
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
		.pNext = s->rpi->pNext,
		.attachmentCount = s->rpi->framebuffer.num_attachments,
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
	uint32_t *ds_indices;
	VkImageView *attachment_views;
} jobptr_t;

static uint32_t convert_color (vec4f_t color)
{
	uint32_t    r = ((int) (color[0] * 255)) & 255;
	uint32_t    g = ((int) (color[1] * 255)) & 255;
	uint32_t    b = ((int) (color[2] * 255)) & 255;
	return (r << 16) | (g << 8) | b;
}

static qfv_label_t
make_label (const char *name, vec4f_t color)
{
	qfv_label_t label = {
		.color = color,
		.color32 = convert_color (color),
		.name_len = name ? strlen (name) : 0,
		.name = name ? name : "",
	};
	return label;
}

static void
init_tasks (uint32_t *task_count, qfv_taskinfo_t **tasks,
			uint32_t num_tasks, qfv_taskinfo_t *intasks,
			jobptr_t *jp, objstate_t *s)
{
	*task_count = num_tasks;
	*tasks = &jp->tasks[s->inds.num_tasks];
	for (uint32_t i = 0; i < num_tasks; i++) {
		(*tasks)[i] = intasks[i];
	}
	s->inds.num_tasks += num_tasks;
}

static void
init_pipeline (qfv_pipeline_t *pl, qfv_pipelineinfo_t *plinfo,
			   jobptr_t *jp, objstate_t *s, int is_compute)
{
	__auto_type li = find_layout (&plinfo->layout, s);
	*pl = (qfv_pipeline_t) {
		.label = make_label (plinfo->name, plinfo->color),
		.disabled = plinfo->disabled,
		.bindPoint = is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE
								: VK_PIPELINE_BIND_POINT_GRAPHICS,
		.pipeline = is_compute ? s->ptr.cpl[s->inds.num_comp_pipelines]
							   : s->ptr.gpl[s->inds.num_graph_pipelines],
		.layout = li->layout,
		.num_indices = li->num_sets,
		.ds_indices = &jp->ds_indices[s->inds.num_ds_indices],
	};
	init_tasks (&pl->task_count, &pl->tasks, plinfo->num_tasks, plinfo->tasks,
			    jp, s);
	s->inds.num_ds_indices += li->num_sets;
	for (uint32_t i = 0; i < pl->num_indices; i++) {
		pl->ds_indices[i] = find_ds_index (&li->sets[i], s);
	}
}

static void
init_subpass (qfv_subpass_t *sp, qfv_subpassinfo_t *isp,
			  jobptr_t *jp, objstate_t *s)
{
	uint32_t    np = s->inds.num_graph_pipelines + s->inds.num_comp_pipelines;
	*sp = (qfv_subpass_t) {
		.label = make_label (isp->name, isp->color),
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
		.label = make_label (rpinfo->name, rpinfo->color),
		.beginInfo = (VkRenderPassBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = s->ptr.rp[s->inds.num_renderpasses],
			.clearValueCount = rpinfo->framebuffer.num_attachments,
			.pClearValues = &jp->clearvalues[s->inds.num_attachments],
		},
		.subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
		.subpass_count = rpinfo->num_subpasses,
		.subpasses = &jp->subpasses[s->inds.num_subpasses],
		.framebuffer = {
			.num_attachments = rpinfo->framebuffer.num_attachments,
			.views = &jp->attachment_views[s->inds.num_attachments],
		},
		.framebufferinfo = &rpinfo->framebuffer,
		.outputref = rpinfo->output,
	};
	create_resources (s->ctx, rp);
	s->inds.num_attachments += rpinfo->framebuffer.num_attachments;
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
		.label = make_label (rinfo->name, rinfo->color),
		.num_renderpasses = rinfo->num_renderpasses,
		.renderpasses = &jp->renderpasses[s->inds.num_renderpasses],
	};
	for (uint32_t i = 0; i < rend->num_renderpasses; i++) {
		init_renderpass (&rend->renderpasses[i], &rinfo->renderpasses[i],
						 jp, s);
		s->inds.num_renderpasses++;
	}
	rend->active = &rend->renderpasses[0];
}

static void
init_compute (qfv_compute_t *comp, qfv_computeinfo_t *cinfo,
			  jobptr_t *jp, objstate_t *s)
{
	uint32_t    np = s->inds.num_graph_pipelines + s->inds.num_comp_pipelines;
	*comp = (qfv_compute_t) {
		.label = make_label (cinfo->name, cinfo->color),
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
		.label = make_label (pinfo->name, pinfo->color),
	};
	init_tasks (&proc->task_count, &proc->tasks, pinfo->num_tasks, pinfo->tasks,
			    jp, s);
}

static void
init_step (uint32_t ind, jobptr_t *jp, objstate_t *s)
{
	__auto_type step = &jp->steps[s->inds.num_steps++];
	__auto_type sinfo = &s->jinfo->steps[ind];

	*step = (qfv_step_t) {
		.label = make_label (sinfo->name, sinfo->color),
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
		step->process->label = step->label;
	}
}

static jobptr_t
create_job (vulkan_ctx_t *ctx, objcount_t *counts, objstate_t *s)
{
	auto rctx = ctx->render_context;
	auto jobinfo = rctx->jobinfo;

	size_t      size = sizeof (qfv_job_t);
	size += sizeof (qfv_step_t       [counts->num_steps]);
	size += sizeof (qfv_render_t     [counts->num_render]);
	size += sizeof (qfv_compute_t    [counts->num_compute]);
	size += sizeof (qfv_process_t    [counts->num_process]);
	size += sizeof (qfv_renderpass_t [counts->num_renderpasses]);
	size += sizeof (qfv_subpass_t    [counts->num_subpasses]);
	size += sizeof (qfv_pipeline_t   [counts->num_graph_pipelines]);
	size += sizeof (qfv_pipeline_t   [counts->num_comp_pipelines]);
	size += sizeof (qfv_taskinfo_t   [counts->num_tasks]);

	size += sizeof (VkClearValue     [counts->num_attachments]);
	size += sizeof (VkRenderPass     [counts->num_renderpasses]);
	size += sizeof (VkPipeline       [counts->num_graph_pipelines]);
	size += sizeof (VkPipeline       [counts->num_comp_pipelines]);
	size += sizeof (VkPipelineLayout [counts->num_layouts]);
	size += sizeof (VkImageView      [counts->num_attachments]);
	size += sizeof (qfv_dsmanager_t *[jobinfo->num_dslayouts]);
	size += sizeof (uint32_t         [counts->num_ds_indices]);

	rctx->job = malloc (size);
	auto job = rctx->job;
	*job = (qfv_job_t) {
		.num_renderpasses = counts->num_renderpasses,
		.num_pipelines = counts->num_graph_pipelines
						 + counts->num_comp_pipelines,
		.num_layouts = s->inds.num_layouts,
		.num_steps = counts->num_steps,
		.commands = DARRAY_STATIC_INIT (16),
		.num_dsmanagers = jobinfo->num_dslayouts,
		.startup_funcs = DARRAY_STATIC_INIT (16),
		.shutdown_funcs = DARRAY_STATIC_INIT (16),
		.clearstate_funcs = DARRAY_STATIC_INIT (16),
	};
	job->steps = (qfv_step_t *) &job[1];
	auto rn = (qfv_render_t *) &job->steps[job->num_steps];
	auto cp = (qfv_compute_t *) &rn[counts->num_render];
	auto pr = (qfv_process_t *) &cp[counts->num_compute];
	auto rp = (qfv_renderpass_t *) &pr[counts->num_process];
	auto sp = (qfv_subpass_t *) &rp[counts->num_renderpasses];
	auto pl = (qfv_pipeline_t *) &sp[counts->num_subpasses];
	auto ti = (qfv_taskinfo_t *) &pl[job->num_pipelines];

	auto cv = (VkClearValue *) &ti[counts->num_tasks];
	job->renderpasses = (VkRenderPass *) &cv[counts->num_attachments];
	job->pipelines = (VkPipeline *) &job->renderpasses[job->num_renderpasses];
	job->layouts = (VkPipelineLayout *) &job->pipelines[job->num_pipelines];
	auto av = (VkImageView *) &job->layouts[counts->num_layouts];
	job->dsmanager = (qfv_dsmanager_t **) &av[counts->num_attachments];
	auto ds = (uint32_t *) &job->dsmanager[jobinfo->num_dslayouts];

	return (jobptr_t) {
		.steps = job->steps,
		.renders = rn,
		.computes = cp,
		.processes = pr,
		.renderpasses = rp,
		.clearvalues = cv,
		.subpasses = sp,
		.pipelines = pl,
		.tasks = ti,
		.ds_indices = ds,
		.attachment_views = av,
	};
}

static void
init_job (vulkan_ctx_t *ctx, objcount_t *counts, jobptr_t jp, objstate_t *s)
{
	auto rctx = ctx->render_context;
	auto job = rctx->job;
	auto jobinfo = rctx->jobinfo;

	for (uint32_t i = 0; i < job->num_renderpasses; i++) {
		job->renderpasses[i] = s->ptr.rp[i];
	}
	for (uint32_t i = 0; i < job->num_pipelines; i++) {
		// compute pipelines come immediately after the graphics pipelines
		job->pipelines[i] = s->ptr.gpl[i];
	}
	for (uint32_t i = 0; i < s->inds.num_layouts; i++) {
		job->layouts[i] = s->ptr.layouts[i].layout;
	}
	for (uint32_t i = s->inds.num_layouts; i < counts->num_layouts; i++) {
		job->layouts[i] = nullptr;
	}
	auto cv = jp.clearvalues;
	memcpy (cv, s->ptr.clear, sizeof (VkClearValue [counts->num_attachments ]));

	for (uint32_t i = 0; i < job->num_steps; i++) {
		init_step (i, &jp, s);
	}
	for (uint32_t i = 0; i < job->num_dsmanagers; i++) {
		auto layoutInfo = &jobinfo->dslayouts[i];
		job->dsmanager[i] = QFV_DSManager_Create (layoutInfo, 16, ctx);
	}
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
		s->inds.num_ds_indices += li->num_sets;
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
del_objstate (void *_state)
{
	objstate_t *state = _state;
	Hash_DelTable (state->symtab->tab);
	free (state->symtab);
}

static void
create_pipeline_layout (const qfv_pipelineinfo_t *pli, objstate_t *s)
{
	if (pli->layout.name) {
		__auto_type li = find_layout (&pli->layout, s);
		s->inds.num_ds_indices += li->num_sets;
	}
}

static void
create_subpass_layouts (uint32_t index, qfv_subpassinfo_t *sub, objstate_t *s)
{
	s->spi = &sub[index];
	s->plc = &s->ptr.gplCreate[s->inds.num_graph_pipelines];
	s->spc = &s->ptr.subpass[s->inds.num_subpasses];

	for (uint32_t i = 0; i < s->spi->num_pipelines; i++) {
		if (s->spi->base_pipeline) {
			create_pipeline_layout (s->spi->base_pipeline, s);
		}
		create_pipeline_layout (&s->spi->pipelines[i], s);
	}
}

static void
create_renderpass_layouts (uint32_t index, const qfv_renderinfo_t *rinfo,
						   objstate_t *s)
{
	s->rpi = &rinfo->renderpasses[index];
	for (uint32_t i = 0; i < s->rpi->num_subpasses; i++) {
		create_subpass_layouts (i, s->rpi->subpasses, s);
	}
}

static void
create_step_render_layouts (uint32_t index, const qfv_stepinfo_t *step,
							objstate_t *s)
{
	__auto_type rinfo = step->render;
	if (!rinfo) {
		return;
	}
	for (uint32_t i = 0; i < rinfo->num_renderpasses; i++) {
		create_renderpass_layouts (i, rinfo, s);
	}
}

static void
create_step_compute_layouts (uint32_t index, const qfv_stepinfo_t *step,
							 objstate_t *s)
{
	__auto_type cinfo = step->compute;
	if (!cinfo) {
		return;
	}

	uint32_t    base = s->inds.num_graph_pipelines;
	for (uint32_t i = 0; i < cinfo->num_pipelines; i++) {
		auto pli = &cinfo->pipelines[i];
		auto li = find_layout (&pli->layout, s);
		s->ptr.plName[base + s->inds.num_comp_pipelines] = pli->name;
		s->inds.num_ds_indices += li->num_sets;
	}
}

static void
create_layouts (vulkan_ctx_t *ctx, objstate_t *s)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	auto jinfo = rctx->jobinfo;

	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		create_step_render_layouts (i, &jinfo->steps[i], s);
	}
	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		create_step_compute_layouts (i, &jinfo->steps[i], s);
	}
}

static void
create_objects (vulkan_ctx_t *ctx, objcount_t *counts)
{
	qfZoneScoped (true);
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
	__attribute__((cleanup (del_objstate))) objstate_t  s = {
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

	// Create pipeline layouts first so they and the descriptor set indices
	// can be counted correctly.
	create_layouts (ctx, &s);
	counts->num_layouts = s.inds.num_layouts;
	counts->num_ds_indices = s.inds.num_ds_indices;

	auto jp = create_job (ctx, counts, &s);

	auto job = rctx->job;
	init_tasks (&job->newscene_task_count, &job->newscene_tasks,
			    jinfo->newscene_num_tasks, jinfo->newscene_tasks,
				&jp, &s);
	init_tasks (&job->init_task_count, &job->init_tasks,
			    jinfo->init_num_tasks, jinfo->init_tasks,
				&jp, &s);
	QFV_Render_Run_Init (ctx);

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
		|| s.inds.num_comp_pipelines != counts->num_comp_pipelines
		|| s.inds.num_layouts > counts->num_layouts) {
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

	counts->num_ds_indices = s.inds.num_ds_indices;

	uint32_t    num_layouts = s.inds.num_layouts;
	uint32_t    num_tasks = s.inds.num_tasks;
	s.inds = (objcount_t) {};
	s.inds.num_layouts = num_layouts;
	s.inds.num_tasks = num_tasks;
	init_job (ctx, counts, jp, &s);
}

void
QFV_BuildRender (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	__auto_type rctx = ctx->render_context;

	objcount_t  counts = {};
	count_stuff (rctx->jobinfo, &counts);

	create_objects (ctx, &counts);
	QFV_Render_Run_Startup (ctx);
}
