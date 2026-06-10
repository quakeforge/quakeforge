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
#include "QF/model.h"
#include "QF/set.h"
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

static void
build_job_step_enum (vulkan_ctx_t *ctx, qfv_renderctx_t *rctx,
					 qfv_jobstepenum_t *jsenum)
{
	int num_steps = 0;
	{
		auto symtab = &rctx->job_symtab;
		int num_syms = jsenum->num_jobs;
		size_t size = sizeof(exprsym_t[num_syms]);
		rctx->job_type = (exprtype_t) {
			.name = "job",
			.size = sizeof (int),
			.binops = cexpr_enum_binops,
			.get_string = cexpr_enum_get_string,
			.data = &rctx->job_enum,
		};
		rctx->job_enum = (exprenum_t) {
			.type = &rctx->job_type,
			.symtab = &rctx->job_symtab,
		};
		rctx->job_symtab = (exprtab_t) {
			.symbols = cmemalloc (jsenum->memsuper, size),
			.tab = Hash_NewTable (61, cexpr_getkey, 0, 0, &rctx->hashctx),
		};
		for (uint32_t i = 0; i < jsenum->num_jobs; i++) {
			auto job = &jsenum->jobs[i];
			int ind = i;
			symtab->symbols[ind] = (exprsym_t) {
				.name = job->name,
				.type = &rctx->job_type,
				.value = cmemalloc (jsenum->memsuper, sizeof (int)),
			};
			*(int *) symtab->symbols[ind].value = ind;
			Hash_Add (symtab->tab, &symtab->symbols[ind]);

			num_steps += job->num_steps;
			printf ("%s = %d\n", symtab->symbols[ind].name, ind);
		}
	}
	{
		auto symtab = &rctx->step_symtab;
		int num_syms = num_steps + 1;	// + 1 for @
		size_t size = sizeof(exprsym_t[num_syms]);
		rctx->step_type = (exprtype_t) {
			.name = "step",
			.size = sizeof (int),
			.binops = cexpr_enum_binops,
			.get_string = cexpr_enum_get_string,
			.data = &rctx->step_enum,
		};
		rctx->step_enum = (exprenum_t) {
			.type = &rctx->step_type,
			.symtab = &rctx->step_symtab,
		};
		rctx->step_symtab = (exprtab_t) {
			.symbols = cmemalloc (jsenum->memsuper, size),
			.tab = Hash_NewTable (61, cexpr_getkey, 0, 0, &rctx->hashctx),
		};
		int ind = 0;
		for (uint32_t i = 0; i < jsenum->num_jobs; i++) {
			auto job = &jsenum->jobs[i];
			for (uint32_t j = 0; j < job->num_steps; j++, ind++) {
				auto step = &job->steps[j];
				const char *name = vac (ctx->va_ctx, "%s`%s",
										job->name, step->name);
				symtab->symbols[ind] = (exprsym_t) {
					.name = cmemstrdup (jsenum->memsuper, name),
					.type = &rctx->step_type,
					.value = cmemalloc (jsenum->memsuper, sizeof (int)),
				};
				*(int *) symtab->symbols[ind].value = ind;
				Hash_Add (symtab->tab, &symtab->symbols[ind]);

				printf ("%s = %d\n", symtab->symbols[ind].name,
						*(int *) symtab->symbols[ind].value);
			}
		}
		{
			symtab->symbols[ind] = (exprsym_t) {
				.name = cmemstrdup (jsenum->memsuper, "@"),
				.type = &rctx->step_type,
				.value = cmemalloc (jsenum->memsuper, sizeof (int)),
			};
			*(int *) symtab->symbols[ind].value = -1;
			Hash_Add (symtab->tab, &symtab->symbols[ind]);

			printf ("%s = %d\n", symtab->symbols[ind].name,
					*(int *) symtab->symbols[ind].value);
		}
	}
}

void
QFV_LoadRenderInfo (vulkan_ctx_t *ctx, plitem_t *item)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	auto output = get_output (ctx, item);
	auto jobs = PL_ObjectForKey (item, "jobs");
	if (jobs) {
		rctx->jobstepenum = QFV_ParseJobStepEnum (ctx, jobs, rctx);
		build_job_step_enum (ctx, rctx, rctx->jobstepenum);
	}
	Vulkan_Script_SetOutput (ctx, &output);
	rctx->graphinfo = QFV_ParseGraphInfo (ctx, item, rctx);
	if (rctx->graphinfo) {
		rctx->graphinfo->plitem = item;
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

void
QFV_LoadEntqueueInfo (vulkan_ctx_t *ctx, plitem_t *item)
{
	static const char *modtype_names[] = {
		"mod_brush",
		"mod_sprite",
		"mod_mesh",
		"mod_light",
	};
	static_assert(countof (modtype_names) == mod_num_types);

	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	rctx->entqueueinfo = QFV_ParseEntqueueInfo (ctx, item, rctx);
	if (rctx->entqueueinfo) {
		auto eqi = rctx->entqueueinfo;
		auto symtab = &rctx->entqueue_symtab;
		auto messages = PL_NewArray ();
		eqi->plitem = PL_ObjectForKey (item, "queues");
		int num_syms = mod_num_types + eqi->num_queues;
		size_t size = sizeof(exprsym_t[num_syms]);
		rctx->entqueue_type = (exprtype_t) {
			.name = "entqueue",
			.size = sizeof (int),
			.binops = cexpr_enum_binops,
			.get_string = cexpr_enum_get_string,
			.data = &rctx->entqueue_enum,
		};
		rctx->entqueue_enum = (exprenum_t) {
			.type = &rctx->entqueue_type,
			.symtab = &rctx->entqueue_symtab,
		};
		rctx->entqueue_symtab = (exprtab_t) {
			.symbols = cmemalloc (eqi->memsuper, size),
			.tab = Hash_NewTable (61, cexpr_getkey, 0, 0, &rctx->hashctx),
		};
		for (uint32_t i = 0; i < mod_num_types; i++) {
			int ind = i;
			symtab->symbols[ind] = (exprsym_t) {
				.name = modtype_names[i],
				.type = &rctx->entqueue_type,
				.value = cmemalloc (eqi->memsuper, sizeof (int)),
			};
			*(int *) symtab->symbols[ind].value = ind;
			Hash_Add (symtab->tab, &symtab->symbols[ind]);
		}
		for (uint32_t i = 0; i < eqi->num_queues; i++) {
			int ind = mod_num_types + i;
			if (Hash_Find (symtab->tab, eqi->queue_names[i])) {
				PL_Message (messages, PL_ObjectAtIndex (eqi->plitem, i),
							"duplicate entqueue name: %s", eqi->queue_names[i]);
			}
			symtab->symbols[ind] = (exprsym_t) {
				.name = eqi->queue_names[i],
				.type = &rctx->entqueue_type,
				.value = cmemalloc (eqi->memsuper, sizeof (int)),
			};
			*(int *) symtab->symbols[ind].value = ind;
			Hash_Add (symtab->tab, &symtab->symbols[ind]);
		}
		if (PL_A_NumObjects (messages)) {
			for (int i = 0; i < PL_A_NumObjects (messages); i++) {
				Sys_Printf ("%s\n", PL_String (PL_ObjectAtIndex (messages, i)));
			}
			Sys_Error ("invalid entqueues");
		}
		PL_Release (messages);
		rctx->entqueue_count = num_syms;
	}
}

qfv_textureinfo_t *
QFV_Render_TextureInfo (vulkan_ctx_t *ctx, plitem_t *info)
{
	auto rctx = ctx->render_context;
	return QFV_ParseTextureInfo (ctx, info, rctx);
}

typedef struct {
	uint32_t    num_images;
	uint32_t    num_imageviews;
	uint32_t    num_buffers;
	uint32_t    num_bufferviews;
	uint32_t    num_framebuffers;
	uint32_t    num_layouts;

	uint32_t    num_pushconstantranges;
	uint32_t    num_pushconstants;

	uint32_t    num_jobs;
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
	uint32_t    num_subpass_inputs;
	uint32_t    num_input_indices;
} objcount_t;

static void
count_as_stuff (qfv_attachmentsetinfo_t *as, objcount_t *counts)
{
	if (as->num_input) {
		counts->num_subpass_inputs++;
		counts->num_input_indices += as->num_input;
	}
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
	spi->subpass_input = counts->num_subpass_inputs;
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
		auto spi = &rpi->subpasses[i];
		count_sp_stuff (spi, counts);
		if (!strcmp (spi->name, "$external")) {
			counts->num_subpasses--;
			if (i != rpi->num_subpasses - 1) {
				Sys_Error ("%s: external subpass not last in list", rpi->name);
			}
			if (spi->attachments) {
				Sys_Error ("%s: external subpass cannot have attachments",
						   rpi->name);
			}
			if (spi->num_pipelines || spi->base_pipeline) {
				Sys_Error ("%s: external subpass cannot have pipelines",
						   rpi->name);
			}
			if (!spi->num_dependencies) {
				Sys_Error ("%s: external subpass has no dependencies",
						   rpi->name);
			}
			for (uint32_t j = 0; j < spi->num_dependencies; j++) {
				auto d = &spi->dependencies[j];
				if (!strcmp (d->name, "$external")) {
					Sys_Error ("%s: external subpass has external dependency",
							   rpi->name);
				}
			}
		}
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
count_step_stuff (qfv_stepinfo_t *step, objcount_t *counts,
				  qfv_graphinfo_t *ginfo, vulkan_ctx_t *ctx)
{
	if (step->render && step->render_template) {
		Sys_Error ("%d:%s: invalid step: cannot have both render and "
				   "render_template", step->line, step->name);
	}
	if (step->render_template && !step->init) {
		Sys_Error ("%d:%s: invalid step: render_template requires init",
				   step->line, step->name);
	}
	if (step->init) {
		qfv_taskctx_t taskctx = {
			.ctx = ctx,
			.memsuper = ginfo->memsuper,
			.stepinfo = step,
		};
		auto init = step->init;
		init->func->func (init->params, 0, (exprctx_t *) &taskctx);
	}
	if (step->render && step->compute && !step->process) {
		Sys_Error ("%d:%s: invalid step: must have process for render+compute",
				   step->line, step->name);
	}
	if (!step->render && !step->compute && !step->process) {
		Sys_Error ("%d:%s: invalid step: must have at least one of "
				   "process/render/compute", step->line, step->name);
	}
	// render_template stuff not counted because it is handled by the
	// step's init function and is used to create render
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
count_job_stuff (qfv_jobinfo_t *jobinfo, qfv_graphinfo_t *ginfo,
				 objcount_t *counts, vulkan_ctx_t *ctx)
{
	for (uint32_t i = 0; i < jobinfo->num_steps; i++) {
		count_step_stuff (&jobinfo->steps[i], counts, ginfo, ctx);
	}
}

static void
count_stuff (qfv_graphinfo_t *graphinfo, objcount_t *counts, vulkan_ctx_t *ctx)
{
	counts->num_images += graphinfo->num_images;
	counts->num_images += graphinfo->resources.num_images;
	counts->num_imageviews += graphinfo->num_imageviews;
	counts->num_imageviews += graphinfo->resources.num_imageviews;
	counts->num_buffers += graphinfo->resources.num_buffers;
	counts->num_bufferviews += graphinfo->resources.num_bufferviews;
	counts->num_framebuffers += graphinfo->num_framebuffers;
	counts->num_jobs += graphinfo->num_jobs;
	for (uint32_t i = 0; i < graphinfo->num_jobs; i++) {
		auto job = &graphinfo->jobs[i];
		count_job_stuff (job, graphinfo, counts, ctx);
	}
	for (uint32_t i = 0; i < graphinfo->num_framebuffers; i++) {
		auto fb = &graphinfo->framebuffers[i];
		counts->num_attachments += fb->num_attachments;
	}
	counts->num_tasks += graphinfo->newscene_num_tasks;
	counts->num_tasks += graphinfo->init_num_tasks;
}

static qfv_imageinfo_t * __attribute__((pure))
find_imageinfo (qfv_imageinfo_t *imageinfo, uint32_t num_images,
				const qfv_reference_t *ref)
{
	if (strcmp (ref->name, "$output.image") == 0) {
		//auto image = ginfo->output.image;
		//graph->image_views[i].image_view.external_image = image;
		//graph->image_views[i].image_view.image = -1;
	} else {
		for (uint32_t i = 0; i < num_images; i++) {
			auto img = &imageinfo[i];
			if (strcmp (ref->name, img->name) == 0) {
				return img;
			}
		}
	}
	Sys_Printf ("%d: unknown image reference: %s\n", ref->line, ref->name);
	return 0;
}

static qfv_imageviewinfo_t * __attribute__((pure))
find_imageviewinfo (qfv_graphinfo_t *graphinfo, const qfv_reference_t *ref)
{
	for (uint32_t i = 0; i < graphinfo->num_imageviews; i++) {
		auto imgview = &graphinfo->imageviews[i];
		if (strcmp (ref->name, imgview->name) == 0) {
			return imgview;
		}
	}
	Sys_Printf ("%d: unknown image view reference: %s\n", ref->line, ref->name);
	return 0;
}

static qfv_bufferinfo_t * __attribute__((pure))
find_bufferinfo (qfv_bufferinfo_t *bufferinfo, uint32_t num_buffers,
				 const qfv_reference_t *ref)
{
	for (uint32_t i = 0; i < num_buffers; i++) {
		auto img = &bufferinfo[i];
		if (strcmp (ref->name, img->name) == 0) {
			return img;
		}
	}
	Sys_Printf ("%d: unknown buffer reference: %s\n", ref->line, ref->name);
	return 0;
}

static uint32_t __attribute__((pure))
find_framebuffer (const qfv_reference_t *ref, qfv_graphinfo_t *ginfo,
				  qfv_renderpassinfo_t *rpi)
{
	for (uint32_t i = 0; i < ginfo->num_framebuffers; i++) {
		auto fb = &ginfo->framebuffers[i];
		if (strcmp (fb->name, ref->name) == 0) {
			return i;
		}
	}
	Sys_Error ("%s:%d: invalid framebuffer: %s",
			   rpi->name, ref->line, ref->name);
}

qfv_framebufferinfo_t *
QFV_FindFramebufferInfo (vulkan_ctx_t *ctx, const qfv_reference_t *ref,
						 const char *rpname)
{
	auto rctx = ctx->render_context;
	auto ginfo = rctx->graphinfo;
	qfv_renderpassinfo_t rpi = {
		.name = rpname,
		.framebuffer.use = *ref,
	};
	uint32_t ind = find_framebuffer (ref, ginfo, &rpi);
	return &ginfo->framebuffers[ind];
}

qfv_bufferinfo_t *
QFV_FindBufferInfo (vulkan_ctx_t *ctx, const char *name)
{
	auto rctx = ctx->render_context;
	auto ginfo = rctx->graphinfo;
	qfv_reference_t ref = { .name = name };
	auto res = &ginfo->resources;
	return find_bufferinfo (res->buffers, res->num_buffers, &ref);
}

qfv_imageinfo_t *
QFV_FindImageInfo (vulkan_ctx_t *ctx, const char *name)
{
	auto rctx = ctx->render_context;
	auto ginfo = rctx->graphinfo;
	qfv_reference_t ref = { .name = name };
	auto res = &ginfo->resources;
	return find_imageinfo (res->images, res->num_images, &ref);
}

static void
setup_image_obj (qfv_resobj_t *image, const qfv_imageinfo_t *img)
{
	*image = (qfv_resobj_t) {
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

static void
setup_imageview_obj (qfv_resobj_t *image_view,
					 const qfv_resobj_t *image,
					 const qfv_imageinfo_t *img,
					 const qfv_imageviewinfo_t *imgview)
{
	*image_view = (qfv_resobj_t) {
		.name = imgview->name,
		.type = qfv_res_image_view,
		.image_view = {
			.image = img->object,
			.flags = imgview->flags,
			.type = imgview->viewType,
			.format = imgview->format,
			.components = imgview->components,
			.subresourceRange = imgview->subresourceRange,
		},
	};
	if (!image_view->image_view.format) {
		image_view->image_view.format = image->image.format;
	}
}

static bool
setup_framebuffers (vulkan_ctx_t *ctx,
					qfv_framebufferinfo_t *fbi, const char *name,
					uint32_t num_attachments, qfv_resource_t *resources,
					qfv_resobj_t *images, qfv_resobj_t *image_views)
{
	auto rctx = ctx->render_context;
	auto gi = rctx->graphinfo;

	resources[0] = (qfv_resource_t) {
		.name = name,
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 * num_attachments,
		.objects = images,
	};
	bool error = false;
	for (uint32_t i = 0; i < num_attachments; i++) {
		auto attach = &fbi->attachments[i];
		auto imgview = find_imageviewinfo (gi, &attach->view);
		if (!imgview) {
			error = true;
			continue;
		}
		auto img = find_imageinfo (gi->images, gi->num_images, &imgview->image);
		if (!img) {
			error = true;
			continue;
		}
		setup_image_obj (&images[i], img);
		img->object = &images[i] - resources->objects;

		setup_imageview_obj (&image_views[i], &images[i], img, imgview);
		imgview->object = &image_views[i] - resources->objects;
	}
	return !error;
}

static qfv_resource_t *
setup_resources (qfv_resourceinfo_t *ri, vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;

	uint32_t num_obj = ri->num_images
					 + ri->num_imageviews
					 + ri->num_buffers
					 + ri->num_bufferviews;
	if (!num_obj) {
		return nullptr;
	}
	size_t size = sizeof (qfv_resource_t) + sizeof (qfv_resobj_t[num_obj]);
	qfv_resource_t *resources = malloc (size);
	*resources = (qfv_resource_t) {
		.name = "render",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = num_obj,
		.objects = (qfv_resobj_t *) &resources[1],
	};
	auto images = resources->objects;
	auto imageviews = &images[ri->num_images];
	auto buffers = &imageviews[ri->num_imageviews];
	auto bufferviews = &buffers[ri->num_buffers];

	bool error = false;

	for (uint32_t i = 0; i < ri->num_images; i++) {
		auto img = &ri->images[i];
		setup_image_obj (&images[i], img);
		img->object = &images[i] - resources->objects;
	}
	for (uint32_t i = 0; i < ri->num_imageviews; i++) {
		auto imgview = &ri->imageviews[i];
		auto img = find_imageinfo (ri->images, ri->num_images, &imgview->image);
		setup_imageview_obj (&imageviews[i], &images[img->object],
							 img, imgview);
		imgview->object = &imageviews[i] - resources->objects;
	}
	for (uint32_t i = 0; i < ri->num_buffers; i++) {
		auto b = &ri->buffers[i];
		VkDeviceSize size = b->size;
		if (b->perframe) {
			b->size = RUP (b->size, 64);
			size = b->size * rctx->frames.size;
		}
		buffers[i] = (qfv_resobj_t) {
			.name = b->name,
			.type = qfv_res_buffer,
			.buffer = {
				.size = size,
				.usage = b->usage,
			},
		};
		b->object = &buffers[i] - resources->objects;
	}
	for (uint32_t i = 0; i < ri->num_bufferviews; i++) {
		auto bv = &ri->bufferviews[i];
		auto b = find_bufferinfo (ri->buffers, ri->num_buffers, &bv->buffer);
		if (!b) {
			error = true;
			continue;
		}
		bufferviews[i] = (qfv_resobj_t) {
			.name = bv->name,
			.buffer_view = {
				.buffer = b->object,
				.format = bv->format,
				.offset = bv->offset,
				.size = bv->range,
			},
		};
	}
	if (error) {
		free (resources);
		resources = nullptr;
	} else {
		QFV_CreateResource (ctx->device, resources);
	}
	return resources;
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

	const qfv_subpassinfo_t *subpass;
	const qfv_pipelineinfo_t *pipeline;

	vulkan_ctx_t *ctx;
	qfv_graphinfo_t *ginfo;
	qfv_jobinfo_t *jinfo;
	exprtab_t  *symtab;
	qfv_renderpassinfo_t *rpi;
	VkRenderPassCreateInfo *rpc;
	qfv_subpassinfo_t *spi;
	VkSubpassDescription *spc;
	qfv_pipelineinfo_t *pli;
	VkGraphicsPipelineCreateInfo *plc;
} objstate_t;

static qfv_resourcearray_t
create_resource_array (objstate_t *s, qfv_framebufferinfo_t *fbi,
					   const char *name)
{
	auto ctx = s->ctx;
	auto rctx = ctx->render_context;

	uint32_t num_attachments = 0;
	for (uint32_t i = 0; i < fbi->num_attachments; i++) {
		auto attach = &fbi->attachments[i];
		if (!attach->external && !fbi->use.name) {
			num_attachments++;
		}
	}
	if (!num_attachments) {
		return (qfv_resourcearray_t) {};
	}

	uint32_t    frames = rctx->frames.size;

	size_t      size = sizeof (qfv_resource_t);
	size += sizeof (qfv_resobj_t [num_attachments]);
	size += sizeof (qfv_resobj_t [num_attachments]);

	qfv_resourcearray_t resources = {
		.array = malloc (frames * size),
		.count = frames,
	};
	auto res = (qfv_resobj_t *) &resources.array[frames];
	for (uint32_t i = 0; i < frames; i++) {
		auto images = res;
		auto image_views = &images[num_attachments];
		res = &image_views[num_attachments];
		if (!setup_framebuffers (ctx, fbi, name,
								 num_attachments, &resources.array[i],
								 images, image_views)) {
			free (resources.array);
			return (qfv_resourcearray_t) {};
		}
	}
	return resources;
}

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
	Sys_Error ("%d: invalid dependency: [%d] %s", d->line, spind, d->name);
}

static uint32_t __attribute__((pure))
find_ds_index (const qfv_reference_t *ref, objstate_t *s)
{
	if (strcmp (ref->name, "$input") == 0) {
		if (!s->subpass) {
			Sys_Error ("not doing a subpass");
		}
		auto subpass = s->subpass;
		if (!subpass->attachments || !subpass->attachments->num_input) {
			Sys_Error ("%d: subpass has no input attachments", subpass->line);
		}
		return s->ginfo->num_dslayouts + subpass->subpass_input;
	}
	for (uint32_t i = 0; i < s->ginfo->num_dslayouts; i++) {
		__auto_type ds = &s->ginfo->dslayouts[i];
		if (strcmp (ds->name, ref->name) == 0) {
			return i;
		}
	}
	Sys_Error ("%s.%s:%d: invalid descriptor set layout: %s",
			   s->rpi->name, s->spi->name, ref->line, ref->name);
}

uint32_t
QFV_GetDSIndex (vulkan_ctx_t *ctx, const char *name)
{
	auto rctx = ctx->render_context;
	auto ginfo = rctx->graphinfo;

	for (uint32_t i = 0; i < ginfo->num_dslayouts; i++) {
		auto ds = &ginfo->dslayouts[i];
		if (strcmp (ds->name, name) == 0) {
			return i;
		}
	}
	Sys_Error ("invalid descriptor set layout: %s", name);
}

static void
create_descriptorSet (qfv_descriptorsetlayoutinfo_t *ds, objstate_t *s)
{
	if (!ds->num_bindings) {
		Sys_Error ("0 bindings in %s", ds->name);
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
						 ds->setLayout, vac (s->ctx->va_ctx, "descriptorSet:%s",
											 ds->name));
}

static VkDescriptorSetLayout
find_descriptorSet (const qfv_reference_t *ref, objstate_t *s)
{
	uint32_t ds_index = find_ds_index (ref, s);
	auto ds = &s->ginfo->dslayouts[ds_index];
	if (ds->setLayout) {
		return ds->setLayout;
	}

	if (ds_index >= s->ginfo->num_dslayouts) {
		auto memsuper = s->ginfo->memsuper;
		auto subpass = s->subpass;
		auto attach = subpass->attachments;
		size_t size = sizeof (VkDescriptorSetLayoutBinding[attach->num_input]);
		VkDescriptorSetLayoutBinding *bindings;
		bindings = cmemalloc (memsuper, size);
		for (uint32_t i = 0; i < attach->num_input; i++) {
			bindings[i] = (VkDescriptorSetLayoutBinding) {
				.binding = i,
				.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			};
		}
		*ds = (qfv_descriptorsetlayoutinfo_t) {
			.name = cmemstrdup (memsuper, vac (s->ctx->va_ctx, "%s:$input",
											   subpass->name)),
			.num_bindings = attach->num_input,
			.bindings = bindings,
		};
		create_descriptorSet (ds, s);
	} else {
		create_descriptorSet (ds, s);
	}
	return ds->setLayout;
}

#define RUP(x,a) (((x) + ((a) - 1)) & ~((a) - 1))
static uint32_t pc_type_sizes[] = {
	[qfv_float] = sizeof (float),
	[qfv_int]   = sizeof (int32_t),
	[qfv_uint]  = sizeof (uint32_t),
	[qfv_vec2]  = sizeof (vec2f_t),
	[qfv_vec3]  = sizeof (vec3_t),
	[qfv_vec4]  = sizeof (vec4f_t),
	[qfv_mat4]  = sizeof (mat4f_t),
	[qfv_ptr]   = sizeof (VkDeviceAddress),
};

static uint32_t pc_type_align[] = {
	[qfv_float] = alignof (float),
	[qfv_int]   = alignof (int32_t),
	[qfv_uint]  = alignof (uint32_t),
	[qfv_vec2]  = alignof (vec2f_t),
	[qfv_vec3]  = alignof (vec4f_t),
	[qfv_vec4]  = alignof (vec4f_t),
	[qfv_mat4]  = alignof (mat4f_t),
	[qfv_ptr]   = alignof (VkDeviceAddress),
};

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
			if (pushconstant->type > countof (pc_type_sizes)) {
				Sys_Error ("%s.%s:%s:%d invalid type: %d",
						   s->rpi->name, s->spi->name, pushconstant->name,
						   pushconstant->line, pushconstant->type);
			}
			size = pc_type_sizes[pushconstant->type];
			uint32_t align = pc_type_align[pushconstant->type];
			offset = RUP (offset, align);
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
	auto memsuper = s->ginfo->memsuper;
	if (!QFV_ParseLayoutInfo (s->ctx, memsuper, s->symtab, ref->name,
							  &s->ptr.layouts[s->inds.num_layouts])) {
		Sys_Error ("%s.%s:%d: invalid layout: %s",
				   s->rpi->name, s->spi->name, ref->line, ref->name);
	}
	__auto_type li = &s->ptr.layouts[s->inds.num_layouts++];
	li->name = ref->name;
	VkDescriptorSetLayout sets[li->num_sets + 1] = {};
	for (uint32_t i = 0; i < li->num_sets; i++) {
		sets[i] = find_descriptorSet (&li->sets[i], s);
	}
	VkPushConstantRange ranges[li->num_pushconstantranges + 1] = {};
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
						 vac (s->ctx->va_ctx, "layout:%s", li->name));
	return li;
}

static void
init_plCreate (VkGraphicsPipelineCreateInfo *plc, const qfv_pipelineinfo_t *pli,
			   objstate_t *s)
{
	s->pipeline = pli;
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
	s->pipeline = nullptr;
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

static bool
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
	bool external = !strcmp (s->spi->name, "$external");
	for (uint32_t i = 0; i < s->spi->num_dependencies; i++) {
		__auto_type d = &s->spi->dependencies[i];
		__auto_type dep = &s->ptr.depend[s->inds.num_dependencies++];
		*dep = (VkSubpassDependency) {
			.srcSubpass = find_subpass (d, index, s->rpi->subpasses),
			.dstSubpass = external ? VK_SUBPASS_EXTERNAL : index,
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
		return external;
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
	return external;
}

static void
init_atCreate (qfv_attachmentinfo_t *ati, objstate_t *s)
{
	auto rctx = s->ctx->render_context;
	auto atc = &s->ptr.attach[s->inds.num_attachments];
	auto cvc = &s->ptr.clear[s->inds.num_attachments];

	if (ati->external) {
		bool found = false;
		for (size_t i = 0; i < rctx->external_attachments.size; i++) {
			auto ext = rctx->external_attachments.a[i];
			if (!strcmp (ati->external, ext->name)) {
				found = true;
				*ati = *ext;
				break;
			}
		}
		if (!found) {
			Sys_Error ("%d:%s: invalid external attachment: %s",
					   ati->line, ati->name, ati->external);
		}
	}
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

	auto fbi = &s->rpi->framebuffer;
	auto attachments = &s->ptr.attach[s->inds.num_attachments];
	auto subpasses = &s->ptr.subpass[s->inds.num_subpasses];
	auto dependencies = &s->ptr.depend[s->inds.num_dependencies];

	qfv_framebufferinfo_t *use_fb = nullptr;
	fbi->use_index = ~0u;
	if (fbi->use.name) {
		fbi->use_index = find_framebuffer (&fbi->use, s->ginfo, s->rpi);
		use_fb = &s->ginfo->framebuffers[fbi->use_index];
	}

	for (uint32_t i = 0; i < fbi->num_attachments; i++) {
		auto attachment = &fbi->attachments[i];
		attachment->index = ~0u;
		auto attach = *attachment;
		if (use_fb) {
			bool found = false;
			for (uint32_t j = 0; j < use_fb->num_attachments; j++) {
				auto use_attach = &use_fb->attachments[j];
				if (strcmp (attach.name, use_attach->name) == 0) {
					attachment->index = j;
					attachment->view = use_attach->view;
					attach.index = j;
					found = true;
					break;
				}
			}
			if (!found) {
				Sys_Error ("%s:%d: invalid use attachment '%s'",
						   s->rpi->name, attach.line, attach.name);
			}
			auto use_attach = &use_fb->attachments[attach.index];
			//FIXME this is very dependent on field order in vkparse.plist
			if (!set_is_member (attach.specified, 0)) {
				attach.flags = use_attach->flags;
			}
			if (!set_is_member (attach.specified, 1)) {
				attach.format = use_attach->format;
			}
			if (!set_is_member (attach.specified, 2)) {
				attach.samples = use_attach->samples;
			}
			if (!set_is_member (attach.specified, 3)) {
				attach.loadOp = use_attach->loadOp;
			}
			if (!set_is_member (attach.specified, 4)) {
				attach.storeOp = use_attach->storeOp;
			}
			if (!set_is_member (attach.specified, 5)) {
				attach.stencilLoadOp = use_attach->stencilLoadOp;
			}
			if (!set_is_member (attach.specified, 6)) {
				attach.stencilStoreOp = use_attach->stencilStoreOp;
			}
			if (!set_is_member (attach.specified, 7)) {
				attach.initialLayout = use_attach->initialLayout;
			}
			if (!set_is_member (attach.specified, 8)) {
				attach.finalLayout = use_attach->finalLayout;
			}
			if (!set_is_member (attach.specified, 9)) {
				attach.clearValue = use_attach->clearValue;
			}
		}
		init_atCreate (&attach, s);
		attachment->loadOp = attach.loadOp;
		attachment->stencilLoadOp = attach.stencilLoadOp;
		s->inds.num_attachments++;
	}

	uint32_t    num_dependencies = s->inds.num_dependencies;
	for (uint32_t i = 0; i < s->rpi->num_subpasses; i++) {
		if (init_spCreate (i, s->rpi->subpasses, s)) {
			s->rpi->num_subpasses--;
			continue;
		}
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
	qfv_job_t *jobs;
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
	VkDescriptorSet *descriptor_sets;
	qfv_subpassinput_t *subpass_inputs;
	uint32_t *input_indices;
	VkImageView *attachment_views;
} graphptr_t;

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
			graphptr_t *gp, objstate_t *s)
{
	*task_count = num_tasks;
	*tasks = &gp->tasks[s->inds.num_tasks];
	for (uint32_t i = 0; i < num_tasks; i++) {
		(*tasks)[i] = intasks[i];
	}
	s->inds.num_tasks += num_tasks;
}

static void
init_pipeline (qfv_pipeline_t *pl, qfv_pipelineinfo_t *plinfo,
			   graphptr_t *gp, objstate_t *s, int is_compute)
{
	auto blackboard = &s->ctx->render_context->blackboard;
	auto li = find_layout (&plinfo->layout, s);
	uint32_t layout_ind = li - s->ptr.layouts;
	*pl = (qfv_pipeline_t) {
		.label = make_label (plinfo->name, plinfo->color),
		.disabled = plinfo->disabled,
		.bindPoint = is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE
								: VK_PIPELINE_BIND_POINT_GRAPHICS,
		.pipeline = is_compute ? s->ptr.cpl[s->inds.num_comp_pipelines]
							   : s->ptr.gpl[s->inds.num_graph_pipelines],
		.layout = li->layout,
		.num_indices = li->num_sets,
		.ds_indices = &gp->ds_indices[s->inds.num_ds_indices],
		.first_push_constant = blackboard->layout_start[layout_ind],
		.num_push_constants = blackboard->layout_count[layout_ind],
	};
	init_tasks (&pl->task_count, &pl->tasks, plinfo->num_tasks, plinfo->tasks,
			    gp, s);
	s->inds.num_ds_indices += li->num_sets;
	for (uint32_t i = 0; i < pl->num_indices; i++) {
		pl->ds_indices[i] = find_ds_index (&li->sets[i], s);
	}
}

static void
init_subpass (qfv_subpass_t *sp, qfv_subpassinfo_t *isp,
			  graphptr_t *gp, objstate_t *s)
{
	s->subpass = isp;
	auto attachments = isp->attachments;
	uint32_t    ni = attachments ? attachments->num_input : 0;
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
		.pipelines = &gp->pipelines[np],
		.frame_index = s->inds.num_subpass_inputs,
		.num_inputs = ni,
		.input_indices = &gp->input_indices[s->inds.num_input_indices],
	};
	if (ni) {
		for (uint32_t i = 0; i < ni; i++) {
			qfv_reference_t ref = {
				.name = attachments->input[i].name,
				.line = attachments->input[i].line,
			};
			sp->input_indices[i] = find_attachment (&ref, s);
		}
		s->inds.num_subpass_inputs++;
		s->inds.num_input_indices += ni;
	}
	for (uint32_t i = 0; i < isp->num_pipelines; i++) {
		init_pipeline (&sp->pipelines[i], &isp->pipelines[i], gp, s, 0);
		s->inds.num_graph_pipelines++;
	}
	s->subpass = nullptr;
}

static uint32_t __attribute__((pure))
clear_value_count (qfv_renderpassinfo_t *rpinfo)
{
	bool have_clear = false;
	for (uint32_t i = 0; i < rpinfo->framebuffer.num_attachments; i++) {
		auto ai = &rpinfo->framebuffer.attachments[i];
		if (ai->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
			|| ai->stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
			have_clear = true;
			break;
		}
	}
	if (!have_clear) {
		return 0;
	}
	return rpinfo->framebuffer.num_attachments;
}

static void
init_renderpass (qfv_renderpass_t *rp, qfv_renderpassinfo_t *rpinfo,
				 graphptr_t *gp, objstate_t *s)
{
	s->rpi = rpinfo;
	*rp = (qfv_renderpass_t) {
		.vulkan_ctx = s->ctx,
		.label = make_label (rpinfo->name, rpinfo->color),
		.beginInfo = (VkRenderPassBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = s->ptr.rp[s->inds.num_renderpasses],
			.clearValueCount = clear_value_count (rpinfo),
			.pClearValues = &gp->clearvalues[s->inds.num_attachments],
		},
		.subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
		.subpass_count = rpinfo->num_subpasses,
		.subpasses = &gp->subpasses[s->inds.num_subpasses],
		.framebuffer = {
			.num_attachments = rpinfo->framebuffer.num_attachments,
			.views = &gp->attachment_views[s->inds.num_attachments],
		},
		.framebufferinfo = &rpinfo->framebuffer,
		.outputref = rpinfo->output,
	};
	rp->resources = create_resource_array (s, rp->framebufferinfo,
										   rp->label.name);
	s->inds.num_attachments += rpinfo->framebuffer.num_attachments;
	for (uint32_t i = 0; i < rpinfo->num_subpasses; i++) {
		init_subpass (&rp->subpasses[i], &rpinfo->subpasses[i], gp, s);
		rp->subpasses[i].inherit.renderPass = rp->beginInfo.renderPass;
		rp->subpasses[i].inherit.subpass = i;
		s->inds.num_subpasses++;
	}
}

static void
init_render (qfv_render_t *rend, qfv_renderinfo_t *rinfo,
			 graphptr_t *gp, objstate_t *s)
{
	*rend = (qfv_render_t) {
		.label = make_label (rinfo->name, rinfo->color),
		.num_renderpasses = rinfo->num_renderpasses,
		.renderpasses = &gp->renderpasses[s->inds.num_renderpasses],
	};
	for (uint32_t i = 0; i < rend->num_renderpasses; i++) {
		init_renderpass (&rend->renderpasses[i], &rinfo->renderpasses[i],
						 gp, s);
		s->inds.num_renderpasses++;
	}
	rend->active = &rend->renderpasses[0];
}

static void
init_compute (qfv_compute_t *comp, qfv_computeinfo_t *cinfo,
			  graphptr_t *gp, objstate_t *s)
{
	uint32_t    np = s->inds.num_graph_pipelines + s->inds.num_comp_pipelines;
	*comp = (qfv_compute_t) {
		.label = make_label (cinfo->name, cinfo->color),
		.pipelines = &gp->pipelines[np],
		.pipeline_count = cinfo->num_pipelines,
	};
	for (uint32_t i = 0; i < cinfo->num_pipelines; i++) {
		init_pipeline (&comp->pipelines[i], &cinfo->pipelines[i], gp, s, 1);
		s->inds.num_comp_pipelines++;
	}
}

static void
init_process (qfv_process_t *proc, qfv_processinfo_t *pinfo,
			  graphptr_t *gp, objstate_t *s)
{
	*proc = (qfv_process_t) {
		.label = make_label (pinfo->name, pinfo->color),
	};
	init_tasks (&proc->task_count, &proc->tasks, pinfo->num_tasks, pinfo->tasks,
			    gp, s);
}

static void
init_step (uint32_t ind, graphptr_t *gp, objstate_t *s)
{
	__auto_type step = &gp->steps[s->inds.num_steps++];
	__auto_type sinfo = &s->jinfo->steps[ind];

	*step = (qfv_step_t) {
		.label = make_label (sinfo->name, sinfo->color),
		.step_info = sinfo,
	};
	if (sinfo->render) {
		step->render = &gp->renders[s->inds.num_render++];
		init_render (step->render, sinfo->render, gp, s);
	}
	if (sinfo->compute) {
		step->compute = &gp->computes[s->inds.num_compute++];
		init_compute (step->compute, sinfo->compute, gp, s);
	}
	if (sinfo->process) {
		step->process = &gp->processes[s->inds.num_process++];
		init_process (step->process, sinfo->process, gp, s);
		step->process->label = step->label;
	}
}

static void
init_job (uint32_t ind, graphptr_t *gp, objstate_t *s)
{
	auto job = &gp->jobs[s->inds.num_jobs++];
	auto jinfo = &s->ginfo->jobs[ind];

	*job = (qfv_job_t) {
		.label = make_label (jinfo->name, jinfo->color),
		.num_steps = jinfo->num_steps,
		.steps = &gp->steps[s->inds.num_steps],
		.commands = DARRAY_STATIC_INIT (16),
	};
	s->jinfo = jinfo;
	for (uint32_t i = 0; i < jinfo->num_steps; i++) {
		init_step (i, gp, s);
	}
}

static graphptr_t
create_graph (vulkan_ctx_t *ctx, objcount_t *counts, objstate_t *s)
{
	auto rctx = ctx->render_context;
	auto graphinfo = rctx->graphinfo;
	uint32_t frames = rctx->frames.size;

	uint32_t     num_dslayouts = graphinfo->num_dslayouts
							   + counts->num_subpass_inputs;
	size_t      size = sizeof (qfv_graph_t);
	size += sizeof (qfv_job_t        [counts->num_jobs]);
	size += sizeof (qfv_step_t       [counts->num_steps]);
	size += sizeof (qfv_render_t     [counts->num_render]);
	size += sizeof (qfv_compute_t    [counts->num_compute]);
	size += sizeof (qfv_process_t    [counts->num_process]);
	size += sizeof (qfv_renderpass_t [counts->num_renderpasses]);
	size += sizeof (qfv_subpass_t    [counts->num_subpasses]);
	size += sizeof (qfv_pipeline_t   [counts->num_graph_pipelines]);
	size += sizeof (qfv_pipeline_t   [counts->num_comp_pipelines]);
	size += sizeof (qfv_taskinfo_t   [counts->num_tasks]);

	size += sizeof (qfv_framebuffer_t[counts->num_framebuffers]);
	size += sizeof (qfv_resourcearray_t[counts->num_framebuffers]);
	size += sizeof (VkClearValue     [counts->num_attachments]);
	size += sizeof (VkRenderPass     [counts->num_renderpasses]);
	size += sizeof (VkPipeline       [counts->num_graph_pipelines]);
	size += sizeof (VkPipeline       [counts->num_comp_pipelines]);
	size += sizeof (VkPipelineLayout [counts->num_layouts]);
	size += sizeof (VkImageView      [counts->num_attachments]);
	size += sizeof (qfv_dsmanager_t *[num_dslayouts]);
	size += sizeof (VkDescriptorSet  [frames * num_dslayouts]);
	size += sizeof (qfv_subpassinput_t[frames * counts->num_subpass_inputs]);
	size += sizeof (uint32_t         [counts->num_input_indices]);
	size += sizeof (uint32_t         [counts->num_ds_indices]);

	rctx->graph = malloc (size);
	auto graph = rctx->graph;
	*graph = (qfv_graph_t) {
		.num_renderpasses = counts->num_renderpasses,
		.num_pipelines = counts->num_graph_pipelines
						 + counts->num_comp_pipelines,
		.num_layouts = counts->num_layouts,
		.num_jobs = counts->num_jobs,
		.num_dsmanagers = num_dslayouts,
		.num_framebuffers = counts->num_framebuffers,
		.startup_funcs = DARRAY_STATIC_INIT (16),
		.shutdown_funcs = DARRAY_STATIC_INIT (16),
		.clearstate_funcs = DARRAY_STATIC_INIT (16),
	};
	graph->jobs = (qfv_job_t *) &graph[1];
	graph->steps = (qfv_step_t *) &graph->jobs[graph->num_jobs];
	auto rn = (qfv_render_t *) &graph->steps[counts->num_steps];
	auto cp = (qfv_compute_t *) &rn[counts->num_render];
	auto pr = (qfv_process_t *) &cp[counts->num_compute];
	auto rp = (qfv_renderpass_t *) &pr[counts->num_process];
	auto sp = (qfv_subpass_t *) &rp[counts->num_renderpasses];
	auto pl = (qfv_pipeline_t *) &sp[counts->num_subpasses];
	auto ti = (qfv_taskinfo_t *) &pl[graph->num_pipelines];

	auto fb = (qfv_framebuffer_t *) &ti[counts->num_tasks];
	auto fbr = (qfv_resourcearray_t *) &fb[counts->num_framebuffers];
	graph->framebuffers = fb;
	graph->framebuffer_resources = fbr;
	auto cv = (VkClearValue *) &fbr[counts->num_framebuffers];
	graph->renderpasses = (VkRenderPass *) &cv[counts->num_attachments];
	graph->pipelines = (VkPipeline *) &graph->renderpasses[graph->num_renderpasses];
	graph->layouts = (VkPipelineLayout *) &graph->pipelines[graph->num_pipelines];
	auto av = (VkImageView *) &graph->layouts[counts->num_layouts];
	graph->dsmanager = (qfv_dsmanager_t **) &av[counts->num_attachments];
	auto ds = (VkDescriptorSet *) &graph->dsmanager[graph->num_dsmanagers];
	auto si = (qfv_subpassinput_t *) &ds[frames * graph->num_dsmanagers];
	auto ii = (uint32_t *) &si[frames * counts->num_subpass_inputs];
	auto dsi = (uint32_t *) &ii[counts->num_input_indices];

	for (uint32_t i = 0; i < counts->num_framebuffers; i++) {
		auto fb = &graph->framebuffers[i];
		auto fbi = &graphinfo->framebuffers[i];
		auto fbr = &graph->framebuffer_resources[i];
		*fb = (qfv_framebuffer_t) {
			.layers = fbi->layers,
			.num_attachments = fbi->num_attachments,
			.views = &av[s->inds.num_attachments],
			.update_frame = ~UINT64_C(0),
		};
		for (uint32_t j = 0; j < fbi->num_attachments; j++) {
			fb->views[j] = nullptr;
		}
		*fbr = create_resource_array (s, fbi, fbi->name);
		s->inds.num_attachments += fbi->num_attachments;
	}

	return (graphptr_t) {
		.jobs = graph->jobs,
		.steps = graph->steps,
		.renders = rn,
		.computes = cp,
		.processes = pr,
		.renderpasses = rp,
		.clearvalues = cv,
		.subpasses = sp,
		.pipelines = pl,
		.tasks = ti,
		.ds_indices = dsi,
		.descriptor_sets = ds,
		.subpass_inputs = si,
		.input_indices = ii,
		.attachment_views = av,
	};
}

static void
init_graph (vulkan_ctx_t *ctx, objcount_t *counts, graphptr_t gp, objstate_t *s)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graph;
	auto graphinfo = rctx->graphinfo;

	for (uint32_t i = 0; i < graph->num_renderpasses; i++) {
		graph->renderpasses[i] = s->ptr.rp[i];
	}
	for (uint32_t i = 0; i < graph->num_pipelines; i++) {
		// compute pipelines come immediately after the graphics pipelines
		graph->pipelines[i] = s->ptr.gpl[i];
	}
	for (uint32_t i = 0; i < s->inds.num_layouts; i++) {
		graph->layouts[i] = s->ptr.layouts[i].layout;
	}
	for (uint32_t i = s->inds.num_layouts; i < counts->num_layouts; i++) {
		graph->layouts[i] = nullptr;
	}
	auto cv = gp.clearvalues;
	memcpy (cv, s->ptr.clear, sizeof (VkClearValue [counts->num_attachments]));

	for (uint32_t i = 0; i < graph->num_dsmanagers; i++) {
		auto layoutInfo = &graphinfo->dslayouts[i];
		graph->dsmanager[i] = QFV_DSManager_Create (layoutInfo, 16, ctx);
	}
	for (uint32_t i = 0; i < graph->num_jobs; i++) {
		init_job (i, &gp, s);
	}

	graph->resources = setup_resources (&graphinfo->resources, ctx);
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
	s->pipeline = pli;
	if (pli->layout.name) {
		__auto_type li = find_layout (&pli->layout, s);
		s->inds.num_ds_indices += li->num_sets;
	}
	s->pipeline = nullptr;
}

static void
create_subpass_layouts (uint32_t index, qfv_subpassinfo_t *sub, objstate_t *s)
{
	s->subpass = &sub[index];
	s->spi = &sub[index];
	s->plc = &s->ptr.gplCreate[s->inds.num_graph_pipelines];
	s->spc = &s->ptr.subpass[s->inds.num_subpasses];

	if (sub[index].attachments && sub[index].attachments->num_input) {
		qfv_reference_t ref = { .name = "$input" };
		find_descriptorSet (&ref, s);
	}

	for (uint32_t i = 0; i < s->spi->num_pipelines; i++) {
		if (s->spi->base_pipeline) {
			create_pipeline_layout (s->spi->base_pipeline, s);
		}
		create_pipeline_layout (&s->spi->pipelines[i], s);
	}
	s->subpass = nullptr;
}

static void
create_renderpass_layouts (uint32_t index, const qfv_renderinfo_t *rinfo,
						   objstate_t *s)
{
	s->rpi = &rinfo->renderpasses[index];
	for (uint32_t i = 0; i < s->rpi->num_subpasses; i++) {
		create_subpass_layouts (i, s->rpi->subpasses, s);
	}
	s->rpi = nullptr;
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
		s->pipeline = pli;
		auto li = find_layout (&pli->layout, s);
		s->ptr.plName[base + s->inds.num_comp_pipelines] = pli->name;
		s->inds.num_ds_indices += li->num_sets;
		s->pipeline = nullptr;
	}
}

static void
create_layouts (vulkan_ctx_t *ctx, objstate_t *s)
{
	qfZoneScoped (true);
	auto rctx = ctx->render_context;
	auto ginfo = rctx->graphinfo;

	for (uint32_t j = 0; j < ginfo->num_jobs; j++) {
		auto jinfo = &ginfo->jobs[j];
		for (uint32_t i = 0; i < jinfo->num_steps; i++) {
			create_step_render_layouts (i, &jinfo->steps[i], s);
		}
		for (uint32_t i = 0; i < jinfo->num_steps; i++) {
			create_step_compute_layouts (i, &jinfo->steps[i], s);
		}
	}
}

static const char *
blackboard_getkey (const void *_pc, void *data)
{
	const qfv_pushconstantinfo_t *pc = _pc;
	return pc->name;
}

static void
create_blackboard (vulkan_ctx_t *ctx, const objcount_t *counts, graphptr_t gp,
				   objstate_t *s)
{
	auto rctx = ctx->render_context;
	//auto graph = rctx->graph;
	if (!counts->num_pushconstantranges || !counts->num_pushconstants) {
		return;
	}
	VkShaderStageFlags stageFlags[counts->num_pushconstants];
	qfv_pushconstantinfo_t bb_constants[counts->num_pushconstants];
	qfv_pushconstantinfo_t pc_constants[counts->num_pushconstants];
	uint32_t map[counts->num_pushconstants];

	uint32_t cind = 0;
	uint32_t mind = 0;

	for (uint32_t i = 0; i < counts->num_layouts; i++) {
		auto layout = &s->ptr.layouts[i];
		uint32_t pc_offset = 0;
		for (uint32_t j = 0; j < layout->num_pushconstantranges; j++) {
			auto r = layout->pushconstantranges[j];
			for (uint32_t k = 0; k < r.num_pushconstants; k++ ) {
				auto c = r.pushconstants[k];
				if (c.offset != ~0u) {
					// used for creating overlapping pushconstant ranges
					// so probably don't want a blackboard variable for it
					pc_offset = c.offset + c.size;
					continue;
				}
				if (c.size == ~0u) {
					c.size = pc_type_sizes[c.type];
				}
				stageFlags[cind] = r.stageFlags;
				bb_constants[cind] = c;
				pc_offset = RUP (pc_offset, pc_type_align[c.type]);
				c.offset = pc_offset;
				pc_offset += c.size;
				pc_constants[cind] = c;

				map[mind++] = cind++;
			}
		}
	}

	uint32_t bb_offset = 0;
	for (uint32_t i = 0; i < cind; i++) {
		auto pc = &bb_constants[i];
		for (uint32_t j = 0; j < i; j++) {
			if (bb_constants[j].name
				&& strcmp (bb_constants[j].name, pc->name) == 0) {
				map[i] = j;
				pc->name = nullptr;
				break;
			}
		}
		if (!pc->name) {
			continue;
		}
		bb_offset = RUP (bb_offset, pc_type_align[pc->type]);
		pc->offset = bb_offset;
		bb_offset += pc->size;
	}
	auto bb = &rctx->blackboard;
	auto memsuper = rctx->graphinfo->memsuper;
	bb->symbols = Hash_NewTable (253, blackboard_getkey, 0, 0, &rctx->hashctx);
	bb->data = cmemalloc (memsuper, bb_offset);
	bb->push_constants = cmemalloc (memsuper,
									sizeof (qfv_push_constants_t[cind]));
	bb->layout_start = cmemalloc (memsuper,
								  sizeof (uint32_t[counts->num_layouts]));
	bb->layout_count = cmemalloc (memsuper,
								  sizeof (uint32_t[counts->num_layouts]));
	for (uint32_t i = 0; i < cind; i++) {
		uint32_t ind = map[i];
		auto pc = &pc_constants[i];
		auto bc = &bb_constants[ind];
		bb->push_constants[i] = (qfv_push_constants_t) {
			.stageFlags = stageFlags[i],
			.offset = pc->offset,
			.size = pc->size,
			.data = bb->data + bc->offset,
		};
		if (bc->name) {
			qfv_pushconstantinfo_t *c = cmemalloc (memsuper, sizeof (*c));
			*c = *bc;
			Hash_Add (bb->symbols, c);
			if (developer & SYS_vulkan) {
				printf ("%3d %d %2d %s\n", c->offset, c->type, c->size,
						c->name);
			}
			bc->name = nullptr;
		}
	}
	bb->layout_start[0] = 0;
	for (uint32_t i = 0; i < counts->num_layouts; i++) {
		if (i > 0) {
			bb->layout_start[i] = bb->layout_start[i - 1]
								+ bb->layout_count[i - 1];
		}
		bb->layout_count[i] = 0;
		auto layout = &s->ptr.layouts[i];
		for (uint32_t j = 0; j < layout->num_pushconstantranges; j++) {
			auto r = layout->pushconstantranges[j];
			for (uint32_t k = 0; k < r.num_pushconstants; k++ ) {
				auto c = r.pushconstants[k];
				if (c.offset != ~0u) {
					continue;
				}
				bb->layout_count[i] ++;
			}
		}
	}
}

static void
create_objects (vulkan_ctx_t *ctx, objcount_t *counts, VkPipelineCache cache)
{
	qfZoneScoped (true);
	__auto_type rctx = ctx->render_context;
	__auto_type ginfo = rctx->graphinfo;

	if (counts->num_subpass_inputs) {
		uint32_t num_dslayouts = ginfo->num_dslayouts;
		uint32_t count = num_dslayouts + counts->num_subpass_inputs;
		size_t asize = sizeof (qfv_descriptorsetlayoutinfo_t[count]);
		size_t csize = sizeof (qfv_descriptorsetlayoutinfo_t[num_dslayouts]);
		void *new_layouts = cmemalloc (ginfo->memsuper, asize);
		memset (new_layouts, 0, asize);
		memcpy (new_layouts, ginfo->dslayouts, csize);
		ginfo->dslayouts = new_layouts;
		ginfo->num_splayouts = counts->num_subpass_inputs;
	}

	VkRenderPass renderpasses[counts->num_renderpasses + 1] = {};
	VkPipeline pipelines[counts->num_graph_pipelines
						 + counts->num_comp_pipelines + 1] = {};
	VkRenderPassCreateInfo rpCreate[counts->num_renderpasses + 1] = {};
	VkAttachmentDescription attach[counts->num_attachments + 1] = {};
	VkClearValue clear[counts->num_attachments + 1] = {};
	VkSubpassDescription subpass[counts->num_subpasses + 1] = {};
	VkSubpassDependency depend[counts->num_dependencies + 1] = {};
	VkAttachmentReference attachref[counts->num_attachmentrefs + 1] = {};
	VkPipelineColorBlendAttachmentState
		cbAttach[counts->num_colorblend + 1] = {};
	uint32_t    preserve[counts->num_preserve + 1] = {};
	const char *rpName[counts->num_renderpasses + 1] = {};
	const char *plName[counts->num_graph_pipelines
					   + counts->num_comp_pipelines + 1] = {};
	VkComputePipelineCreateInfo cplCreate[counts->num_comp_pipelines + 1] = {};
	VkGraphicsPipelineCreateInfo
		gplCreate[counts->num_graph_pipelines + 1] = {};
	VkPipelineColorBlendStateCreateInfo
		cbState[counts->num_graph_pipelines + 1] = {};
	qfv_layoutinfo_t layouts[counts->num_graph_pipelines
							 + counts->num_comp_pipelines + 1] = {};
	uint32_t    pl_counts[counts->num_renderpasses + 1] = {};

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
		.ginfo = ginfo,
		.symtab = QFV_CreateSymtab (ginfo->plitem, "properties", 0, 0, &ectx),
	};

	// Create pipeline layouts first so they and the descriptor set indices
	// can be counted correctly.
	create_layouts (ctx, &s);
	counts->num_layouts = s.inds.num_layouts;
	counts->num_ds_indices = s.inds.num_ds_indices;
	for (uint32_t i = 0; i < counts->num_layouts; i++) {
		auto layout = &s.ptr.layouts[i];
		counts->num_pushconstantranges += layout->num_pushconstantranges;
		for (uint32_t j = 0; j < layout->num_pushconstantranges; j++) {
			auto range = &layout->pushconstantranges[j];
			counts->num_pushconstants += range->num_pushconstants;
		}
	}
	s.inds.num_ds_indices = 0;

	auto gp = create_graph (ctx, counts, &s);

	create_blackboard (ctx, counts, gp, &s);

	auto graph = rctx->graph;
	init_tasks (&graph->newscene_task_count, &graph->newscene_tasks,
			    ginfo->newscene_num_tasks, ginfo->newscene_tasks,
				&gp, &s);
	init_tasks (&graph->init_task_count, &graph->init_tasks,
			    ginfo->init_num_tasks, ginfo->init_tasks,
				&gp, &s);

	uint32_t num_descriptor_sets = graph->num_dsmanagers;
	for (size_t i = 0; i < rctx->frames.size; i++) {
		auto frame = &rctx->frames.a[i];
		frame->descriptor_sets = gp.descriptor_sets + i * num_descriptor_sets;
		for (uint32_t j = 0; j < num_descriptor_sets; j++) {
			frame->descriptor_sets[0] = 0;
		}
	}
	QFV_Render_Run_Init (ctx);

	for (uint32_t j = 0; j < ginfo->num_jobs; j++) {
		auto jinfo = &ginfo->jobs[j];
		for (uint32_t i = 0; i < jinfo->num_steps; i++) {
			create_step_render_objects (i, &jinfo->steps[i], &s);
		}
		for (uint32_t i = 0; i < jinfo->num_steps; i++) {
			create_step_compute_objects (i, &jinfo->steps[i], &s);
		}
		for (uint32_t i = 0; i < jinfo->num_steps; i++) {
			create_step_process_objects (i, &jinfo->steps[i], &s);
		}
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
		|| s.inds.num_ds_indices != counts->num_ds_indices
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
							 vac (ctx->va_ctx, "renderpass:%s", rpName[i]));
		for (uint32_t j = 0; j < pl_counts[i]; j++) {
			s.ptr.gplCreate[plInd++].renderPass = renderpasses[i];
		}
	}
	if (s.inds.num_graph_pipelines) {
		dfunc->vkCreateGraphicsPipelines (device->dev, cache,
										  s.inds.num_graph_pipelines,
										  s.ptr.gplCreate, 0, pipelines);
	}
	if (s.inds.num_comp_pipelines) {
		__auto_type p = &pipelines[s.inds.num_graph_pipelines];
		dfunc->vkCreateComputePipelines (device->dev, cache,
										 s.inds.num_comp_pipelines,
										 s.ptr.cplCreate, 0, p);
	}
	for (uint32_t i = 0;
		 i < s.inds.num_graph_pipelines + s.inds.num_comp_pipelines; i++) {
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_PIPELINE, pipelines[i],
							 vac (ctx->va_ctx, "pipeline:%s", plName[i]));
	}

	counts->num_ds_indices = s.inds.num_ds_indices;

	uint32_t    num_layouts = s.inds.num_layouts;
	uint32_t    num_tasks = s.inds.num_tasks;
	s.inds = (objcount_t) {};
	s.inds.num_layouts = num_layouts;
	s.inds.num_tasks = num_tasks;
	init_graph (ctx, counts, gp, &s);

	uint32_t num_subpass_inputs = counts->num_subpass_inputs;
	for (size_t i = 0; i < rctx->frames.size; i++) {
		auto frame = &rctx->frames.a[i];
		frame->subpass_inputs = gp.subpass_inputs + i * num_subpass_inputs;
		for (uint32_t j = 0; j < num_subpass_inputs; j++) {
			uint32_t ds_index = ginfo->num_dslayouts + j;
			auto dsmanager = graph->dsmanager[ds_index];
			auto set = QFV_DSManager_AllocSet (dsmanager);
			frame->descriptor_sets[ds_index] = set;
			frame->subpass_inputs[j] = (qfv_subpassinput_t) {
				.update_frame = ~UINT64_C(0),
				.set = set,
			};
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, set,
								 vac (ctx->va_ctx, "%s:%zd",
									  dsmanager->name, i));
		}
	}
}

static void
dump_graph (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	auto graph = rctx->graphinfo;
	printf ("digraph expr_%p {\n", graph);
	printf ("  graph [label=\"%s\"];\n", "render graph");
	printf ("  layout=dot; rankdir=TB; compound=true;\n");
	for (uint32_t g = 0; g < graph->num_jobs; g++) {
		auto job = &graph->jobs[g];
		printf ("  subgraph cluster_job_%s {\n", job->name);
		printf ("    label=\"%s\";\n", job->name);
		for (uint32_t i = 0; i < job->num_steps; i++) {
			auto step = &job->steps[i];
			printf ("    s_%p [label=\"%s\"];\n", step, step->name);
			for (uint32_t j = 0; j < step->num_dependencies; j++) {
				auto dep = &step->dependencies[j];
				qfv_stepinfo_t *dep_step = nullptr;
				for (uint32_t k = 0; k < job->num_steps; k++) {
					if (!strcmp (job->steps[k].name, dep->name)) {
						dep_step = &job->steps[k];
						break;
					}
				}
				printf ("    s_%p -> \"s_%p\";\n", step, dep_step);
			}
		}
		printf ("  }\n");
	}
	printf ("}\n");
}

void
QFV_BuildRender (vulkan_ctx_t *ctx, dstring_t *cache_data)
{
	qfZoneScoped (true);
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;

	objcount_t  counts = {};
	count_stuff (rctx->graphinfo, &counts, ctx);

	VkPipelineCache cache = VK_NULL_HANDLE;
	if (cache_data) {
		cache = QFV_CreatePipelineCache (device, cache_data);
	}

	create_objects (ctx, &counts, cache);
	if (developer & SYS_vulkan) {
		dump_graph (ctx);
	}
	if (cache_data) {
		QFV_GetPipelineCacheData (device, cache, cache_data);
		dfunc->vkDestroyPipelineCache (device->dev, cache, nullptr);
	}
	QFV_Render_Run_Startup (ctx);
}
