/*
	vulkan_alias.c

	Vulkan alias model pipeline

	Copyright (C) 2012       Bill Currie <bill@taniwha.org>
	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
	Date: 2021/1/26

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vrect.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"

#include "r_internal.h"
#include "vid_vulkan.h"

void
Vulkan_DrawAlias (entity_t *ent, struct vulkan_ctx_s *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	model_t    *model = ent->model;
	aliashdr_t *hdr;
	qfv_alias_mesh_t *mesh;
	qfv_alias_skin_t *skin;
	float       vertex_constants[17];
	byte        fragment_constants[3][4];

	if (!(hdr = model->aliashdr)) {
		hdr = Cache_Get (&model->cache);
	}
	mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);

	memcpy (vertex_constants, ent->transform, sizeof (ent->transform));
	vertex_constants[16] = R_AliasGetLerpedFrames (ent, hdr);

	if (0/*XXX ent->skin && ent->skin->tex*/) {
		//skin = ent->skin->tex;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (ent->skinnum, hdr);
		skin = (qfv_alias_skin_t *) ((byte *) hdr + skindesc->skin);
	}
	QuatScale (ent->colormod, 255, fragment_constants[0]);
	QuatCopy (skin->colora, fragment_constants[1]);
	QuatCopy (skin->colorb, fragment_constants[2]);

	VkDeviceSize offsets[] = {
		ent->pose1 * hdr->poseverts * sizeof (aliasvrt_t),
		ent->pose2 * hdr->poseverts * sizeof (aliasvrt_t),
		0,
	};
	VkBuffer    buffers[] = {
		mesh->vertex_buffer,
		mesh->vertex_buffer,
		mesh->uv_buffer,
	};
	dfunc->vkCmdBindVertexBuffers (aframe->cmd, 0, 3, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (aframe->cmd, mesh->index_buffer, 0,
								 VK_INDEX_TYPE_UINT32);
	dfunc->vkCmdPushConstants (aframe->cmd, actx->layout,
							   VK_SHADER_STAGE_VERTEX_BIT,
							   0, sizeof (vertex_constants), vertex_constants);
	dfunc->vkCmdPushConstants (aframe->cmd, actx->layout,
							   VK_SHADER_STAGE_FRAGMENT_BIT,
							   68, sizeof (fragment_constants),
							   fragment_constants);
	aframe->imageInfo[0].imageView = skin->view;
	dfunc->vkCmdPushDescriptorSetKHR (aframe->cmd,
									  VK_PIPELINE_BIND_POINT_GRAPHICS,
									  actx->layout,
									  0, ALIAS_IMAGE_INFOS,
									  aframe->descriptors
									  + ALIAS_BUFFER_INFOS);
	dfunc->vkCmdDrawIndexed (aframe->cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);
}

static void
calc_lighting (qfv_light_t *light, entity_t *ent)
{
	vec3_t      ambient_color;
	//FIXME should be ent->position
	float       l = R_LightPoint (&r_worldentity.model->brush, r_origin) / 128.0;

	//XXX l = max (light, max (ent->model->min_light, ent->min_light));
	light->type = 2;
	VectorSet (1, 1, 1, ambient_color);	//FIXME
	// position doubles as ambient light
	VectorScale (ambient_color, l, light->position);
	VectorSet (-1, 0, 0, light->direction);	//FIXME
	VectorCopy (light->position, light->color);
}

void
Vulkan_AliasBegin (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	dlight_t   *lights[ALIAS_LIGHTS];
	//XXX quat_t      fog;

	__auto_type cframe = &ctx->framebuffers.a[ctx->curFrame];
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = aframe->cmd;
	DARRAY_APPEND (cframe->subCommand, cmd);

	//FIXME ambient needs to be per entity
	aframe->lights->light_count = 1;
	calc_lighting (&aframe->lights->lights[0], 0);
	R_FindNearLights (r_origin, ALIAS_LIGHTS - 1, lights);
	for (int i = 0; i < ALIAS_LIGHTS - 1; i++) {
		if (!lights[i]) {
			break;
		}
		aframe->lights->light_count++;
		VectorCopy (lights[i]->color, aframe->lights->lights[i + 1].color);
		VectorCopy (lights[i]->origin, aframe->lights->lights[i + 1].position);
		aframe->lights->lights[i + 1].dist = lights[i]->radius;
		aframe->lights->lights[i + 1].type = 0;
	}

	//FIXME need per frame matrices
	aframe->bufferInfo[0].buffer = ctx->matrices.buffer_3d;
	aframe->bufferInfo[1].buffer = aframe->light_buffer;

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		ctx->renderpass.renderpass, 0,
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  actx->pipeline);
	//VkDescriptorSet sets[] = {
	//	aframe->descriptors[0].dstSet,
	//	aframe->descriptors[1].dstSet,
	//};
	//dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//								actx->layout, 0, 2, sets, 0, 0);
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  actx->layout,
									  0, 2, aframe->descriptors + 0);
	VkViewport  viewport = {0, 0, vid.width, vid.height, 0, 1};
	VkRect2D    scissor = { {0, 0}, {vid.width, vid.height} };
	dfunc->vkCmdSetViewport (cmd, 0, 1, &viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &scissor);

	//dfunc->vkUpdateDescriptorSets (device->dev, 2, aframe->descriptors, 0, 0);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

void
Vulkan_AliasEnd (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	dfunc->vkEndCommandBuffer (aframe->cmd);
}

static VkDescriptorBufferInfo base_buffer_info = {
	0, 0, VK_WHOLE_SIZE
};
static VkDescriptorImageInfo base_image_info = {
	0, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	0, 0, 0
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	0, 0, 0
};

void
Vulkan_Alias_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	aliasctx_t *actx = calloc (1, sizeof (aliasctx_t));
	ctx->alias_context = actx;

	size_t      frames = ctx->framebuffers.size;
	DARRAY_INIT (&actx->frames, frames);
	DARRAY_RESIZE (&actx->frames, frames);
	actx->frames.grow = 0;

	actx->pipeline = Vulkan_CreatePipeline (ctx, "alias");
	actx->layout = Vulkan_CreatePipelineLayout (ctx, "alias_layout");
	actx->sampler = Vulkan_CreateSampler (ctx, "alias_sampler");

	/*__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (2 * frames, alloca);
	for (size_t i = 0; i < layouts->size / 2; i++) {
		__auto_type mats = QFV_GetDescriptorSetLayout (ctx, "alias.matrices");
		__auto_type lights = QFV_GetDescriptorSetLayout (ctx, "alias.lights");
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
							 mats, va (ctx->va_ctx, "set_layout:%s:%d",
									   "alias.matrices", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
							 lights, va (ctx->va_ctx, "set_layout:%s:%d",
										 "alias.lights", i));
		layouts->a[2 * i + 0] = mats;
		layouts->a[2 * i + 1] = lights;
	}*/
	//__auto_type pool = QFV_GetDescriptorPool (ctx, "alias.pool");

	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (frames, alloca);
	QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdBuffers);

	__auto_type lbuffers = QFV_AllocBufferSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		lbuffers->a[i] = QFV_CreateBuffer (device, sizeof (qfv_light_buffer_t),
										   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER,
							 lbuffers->a[i],
							 va (ctx->va_ctx, "buffer:alias:%s:%zd",
								 "lights", i));
	}
	VkMemoryRequirements requirements;
	dfunc->vkGetBufferMemoryRequirements (device->dev, lbuffers->a[0],
										  &requirements);
	actx->light_memory = QFV_AllocBufferMemory (device, lbuffers->a[0],
										VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
										frames * requirements.size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 actx->light_memory, va (ctx->va_ctx,
												 "memory:alias:%s", "lights"));
	byte       *light_data;
	dfunc->vkMapMemory (device->dev, actx->light_memory, 0,
						frames * requirements.size, 0, (void **) &light_data);

	//__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	for (size_t i = 0; i < frames; i++) {
		__auto_type aframe = &actx->frames.a[i];
		aframe->cmd = cmdBuffers->a[i];
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 aframe->cmd,
							 va (ctx->va_ctx, "cmd:alias:%zd", i));
		aframe->light_buffer = lbuffers->a[i];
		aframe->lights = (qfv_light_buffer_t *) (light_data + i * requirements.size);
		QFV_BindBufferMemory (device, lbuffers->a[i], actx->light_memory,
							  i * requirements.size);

		for (int j = 0; j < ALIAS_BUFFER_INFOS; j++) {
			aframe->bufferInfo[j] = base_buffer_info;
			aframe->descriptors[j] = base_buffer_write;
			//aframe->descriptors[j].dstSet = sets->a[ALIAS_BUFFER_INFOS*i + j];
			aframe->descriptors[j].dstBinding = j;
			aframe->descriptors[j].pBufferInfo = &aframe->bufferInfo[j];
		}
		for (int j = 0; j < ALIAS_IMAGE_INFOS; j++) {
			aframe->imageInfo[j] = base_image_info;
			aframe->imageInfo[j].sampler = actx->sampler;
			int k = j + ALIAS_BUFFER_INFOS;
			aframe->descriptors[k] = base_image_write;
			aframe->descriptors[k].dstBinding = k;
			aframe->descriptors[k].pImageInfo = &aframe->imageInfo[j];
		}
	}
	//free (sets);
}

void
Vulkan_Alias_Shutdown (struct vulkan_ctx_s *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	for (size_t i = 0; i < actx->frames.size; i++) {
		__auto_type aframe = &actx->frames.a[i];
		dfunc->vkDestroyBuffer (device->dev, aframe->light_buffer, 0);
	}
	dfunc->vkFreeMemory (device->dev, actx->light_memory, 0);

	dfunc->vkDestroyPipeline (device->dev, actx->pipeline, 0);
	DARRAY_CLEAR (&actx->frames);
	free (actx);
}
