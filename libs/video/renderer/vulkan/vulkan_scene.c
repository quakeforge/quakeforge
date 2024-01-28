/*
	vulkan_scene.c

	Vulkan scene handling

	Copyright (C) 2022       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/5/24

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

#include <string.h>

#include "QF/mathlib.h"
#include "QF/set.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_scene.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static const int qfv_max_entities = 4096;	//FIXME should make dynamic

int
Vulkan_Scene_MaxEntities (vulkan_ctx_t *ctx)
{
	scenectx_t *sctx = ctx->scene_context;
	return sctx->max_entities;
}

VkDescriptorSet
Vulkan_Scene_Descriptors (vulkan_ctx_t *ctx)
{
	scenectx_t *sctx = ctx->scene_context;
	scnframe_t *sframe = &sctx->frames.a[ctx->curFrame];
	return sframe->descriptors;
}

int
Vulkan_Scene_AddEntity (vulkan_ctx_t *ctx, entity_t entity)
{
	scenectx_t *sctx = ctx->scene_context;
	scnframe_t *sframe = &sctx->frames.a[ctx->curFrame];

	entdata_t  *entdata = 0;
	int         render_id = -1;
	//lock
	int         ent_id = Ent_Index (entity.id + 1);//nullent -> 0
	if (!set_is_member (sframe->pooled_entities, ent_id)) {
		if (sframe->entity_pool.size < sframe->entity_pool.maxSize) {
			set_add (sframe->pooled_entities, ent_id);
			render_id = sframe->entity_pool.size++;
			entdata = sframe->entity_pool.a + render_id;
		}
	} else {
		if (!Entity_Valid (entity)) {
			return 0;	//FIXME see below
		} else {
			auto renderer = Entity_GetRenderer (entity);
			return renderer->render_id;
		}
	}
	if (Entity_Valid (entity)) {
		auto renderer = Entity_GetRenderer (entity);
		renderer->render_id = render_id;
	}
	//unlock
	if (entdata) {
		mat4f_t     f;
		vec4f_t     color;
		if (Entity_Valid (entity)) { //FIXME give world entity an entity :P
			transform_t transform = Entity_Transform (entity);
			auto renderer = Entity_GetRenderer (entity);
			mat4ftranspose (f, Transform_GetWorldMatrixPtr (transform));
			entdata->xform[0] = f[0];
			entdata->xform[1] = f[1];
			entdata->xform[2] = f[2];
			color = (vec4f_t) { QuatExpand (renderer->colormod) };
		} else {
			entdata->xform[0] = (vec4f_t) { 1, 0, 0, 0 };
			entdata->xform[1] = (vec4f_t) { 0, 1, 0, 0 };
			entdata->xform[2] = (vec4f_t) { 0, 0, 1, 0 };
			color = (vec4f_t) { 1, 1, 1, 1 };
		}
		entdata->color = color;
	}
	return render_id;
}

void
Vulkan_Scene_Flush (vulkan_ctx_t *ctx)
{
	scenectx_t *sctx = ctx->scene_context;
	scnframe_t *sframe = &sctx->frames.a[ctx->curFrame];

	set_empty (sframe->pooled_entities);
	sframe->entity_pool.size = 0;
}

static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	0, 0, 0
};

static void
scene_draw_viewmodel (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	entity_t    ent = vr_data.view_model;
	if (!Entity_Valid (ent)) {
		return;
	}
	auto renderer = Entity_GetRenderer (ent);
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel
		|| !r_drawentities
		|| !renderer->model)
		return;

	EntQueue_AddEntity (r_ent_queue, ent, renderer->model->type);
}

static void
scene_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "scene shutdown");
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	scenectx_t *sctx = ctx->scene_context;

	for (size_t i = 0; i < sctx->frames.size; i++) {
		__auto_type sframe = &sctx->frames.a[i];
		set_delete (sframe->pooled_entities);
	}

	dfunc->vkUnmapMemory (device->dev, sctx->entities->memory);
	QFV_DestroyResource (device, sctx->entities);
	free (sctx->frames.a);
	free (sctx);
	qfvPopDebug (ctx);
}

static void
scene_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfZoneScoped (true);
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto sctx = ctx->scene_context;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&sctx->frames, frames);
	DARRAY_RESIZE (&sctx->frames, frames);
	sctx->frames.grow = 0;

	sctx->entities = (qfv_resource_t *) &sctx[1];
	*sctx->entities = (qfv_resource_t) {
		.name = "scene",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		.num_objects = 1,
		.objects = (qfv_resobj_t *) &sctx->entities[1],
	};
	sctx->entities->objects[0] = (qfv_resobj_t) {
		.name = "entities",
		.type = qfv_res_buffer,
		.buffer = {
			.size = frames * qfv_max_entities * sizeof (entdata_t),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};

	QFV_CreateResource (device, sctx->entities);

	auto dsmanager = QFV_Render_DSManager (ctx, "entity_set");

	entdata_t  *entdata;
	dfunc->vkMapMemory (device->dev, sctx->entities->memory, 0, VK_WHOLE_SIZE,
						0, (void **) &entdata);

	VkBuffer    buffer = sctx->entities->objects[0].buffer.buffer;
	size_t      entdata_size = qfv_max_entities * sizeof (entdata_t);
	for (size_t i = 0; i < frames; i++) {
		__auto_type sframe = &sctx->frames.a[i];

		sframe->descriptors = QFV_DSManager_AllocSet (dsmanager);;
		VkDescriptorBufferInfo bufferInfo = {
			buffer, i * entdata_size, entdata_size
		};
		VkWriteDescriptorSet write[1];
		write[0] = base_buffer_write;
		write[0].dstSet = sframe->descriptors;
		write[0].pBufferInfo = &bufferInfo;
		dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);

		sframe->entity_pool = (entdataset_t) {
			.maxSize = qfv_max_entities,
			.a = entdata + i * qfv_max_entities,
		};
		sframe->pooled_entities = set_new ();
	}
	qfvPopDebug (ctx);
}

static void
scene_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "scene init");

	QFV_Render_AddShutdown (ctx, scene_shutdown);
	QFV_Render_AddStartup (ctx, scene_startup);

	scenectx_t *sctx = calloc (1, sizeof (scenectx_t)
							   + sizeof (qfv_resource_t)
							   + sizeof (qfv_resobj_t));
	ctx->scene_context = sctx;
	sctx->max_entities = qfv_max_entities;
}

static exprfunc_t scene_draw_viewmodel_func[] = {
	{ .func = scene_draw_viewmodel },
	{}
};

static exprfunc_t scene_init_func[] = {
	{ .func = scene_init },
	{}
};

static exprsym_t scene_task_syms[] = {
	{ "scene_draw_viewmodel", &cexpr_function, scene_draw_viewmodel_func },
	{ "scene_init", &cexpr_function, scene_init_func },
	{}
};

void
Vulkan_Scene_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, scene_task_syms);
}

void
Vulkan_NewScene (scene_t *scene, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	auto sctx = ctx->scene_context;
	sctx->scene = scene;

	for (int i = 0; i < 256; i++) {
		d_lightstylevalue[i] = 264;		// normal light value
	}

	r_refdef.worldmodel = scene->worldmodel;
	EntQueue_Clear (r_ent_queue);

	R_ClearParticles ();

	QFV_Render_NewScene (scene, ctx);
}
