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

	for (uint32_t i = 0; i < sp->pipline_count; i++) {
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
}

typedef struct {
	uint32_t    num_images;
	uint32_t    num_views;

	uint32_t    num_renderpasses;
	uint32_t    num_attachments;
	uint32_t    num_subpasses;
	uint32_t    num_dependencies;
	uint32_t    num_attachmentrefs;
	uint32_t    num_colorblend;
	uint32_t    num_preserve;
	uint32_t    num_pipelines;
	uint32_t    num_tasks;
	uint32_t    num_stages;
} objcount_t;

static void
count_pl_stuff (qfv_pipelineinfo_t *pli, objcount_t *counts)
{
	counts->num_tasks += pli->num_tasks;
	counts->num_stages += pli->num_graph_stages;
}

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
	for (uint32_t i = 0; i < spi->num_pipelines; i++) {
		count_pl_stuff (spi->pipelines, counts);
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

void
QFV_BuildRender (vulkan_ctx_t *ctx)
{
	__auto_type rctx = ctx->render_context;

	rctx->render = calloc (1, sizeof (qfv_render_t));

	objcount_t  counts = {};
	count_stuff (rctx->renderinfo, &counts);
	create_resources (ctx, &counts);
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
	__auto_type rctx = ctx->render_context;
	if (rctx->render) {
		if (rctx->render->resources) {
			QFV_DestroyResource (ctx->device, rctx->render->resources);
			free (rctx->render->resources);
		}
		free (rctx->render);
	}
	if (rctx->renderinfo) {
		delete_memsuper (rctx->renderinfo->memsuper);
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
