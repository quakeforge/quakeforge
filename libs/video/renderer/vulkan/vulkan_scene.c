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

#include "QF/Vulkan/qf_scene.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"

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
Vulkan_Scene_AddEntity (vulkan_ctx_t *ctx, entity_t *entity)
{
	scenectx_t *sctx = ctx->scene_context;
	scnframe_t *sframe = &sctx->frames.a[ctx->curFrame];

	entdata_t  *entdata = 0;
	//lock
	int         id = -entity->id;
	if (!set_is_member (sframe->pooled_entities, id)) {
		if (sframe->entity_pool.size < sframe->entity_pool.maxSize) {
			set_add (sframe->pooled_entities, id);
			entity->renderer.render_id = sframe->entity_pool.size++;
			entdata = sframe->entity_pool.a + entity->renderer.render_id;
		} else {
			entity->renderer.render_id = -1;
		}
	}
	//unlock
	if (entdata) {
		mat4f_t     f;
		if (entity->transform) { //FIXME give world entity an entity :P
			mat4ftranspose (f, Transform_GetWorldMatrixPtr (entity->transform));
			entdata->xform[0] = f[0];
			entdata->xform[1] = f[1];
			entdata->xform[2] = f[2];
		} else {
			entdata->xform[0] = (vec4f_t) { 1, 0, 0, 0 };
			entdata->xform[1] = (vec4f_t) { 0, 1, 0, 0 };
			entdata->xform[2] = (vec4f_t) { 0, 0, 1, 0 };
		}
		entdata->color = (vec4f_t) { QuatExpand (entity->renderer.colormod) };
	}
	return entity->renderer.render_id;
}

static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	0, 0, 0
};

void
Vulkan_Scene_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "scene init");

	scenectx_t *sctx = calloc (1, sizeof (scenectx_t)
							   + sizeof (qfv_resource_t)
							   + sizeof (qfv_resobj_t));
	ctx->scene_context = sctx;
	sctx->max_entities = qfv_max_entities;

	size_t      frames = ctx->frames.size;
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

	sctx->pool = Vulkan_CreateDescriptorPool (ctx, "entity_pool");
	sctx->setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "entity_set");
	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = sctx->setLayout;
	}
	__auto_type sets = QFV_AllocateDescriptorSet (device, sctx->pool, layouts);

	entdata_t  *entdata;
	dfunc->vkMapMemory (device->dev, sctx->entities->memory, 0, VK_WHOLE_SIZE,
						0, (void **) &entdata);

	VkBuffer    buffer = sctx->entities->objects[0].buffer.buffer;
	size_t      entdata_size = qfv_max_entities * sizeof (entdata_t);
	for (size_t i = 0; i < frames; i++) {
		__auto_type sframe = &sctx->frames.a[i];

		sframe->descriptors = sets->a[i];
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
	free (sets);
	qfvPopDebug (ctx);
}

void
Vulkan_Scene_Shutdown (vulkan_ctx_t *ctx)
{
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
