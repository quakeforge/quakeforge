/*
	vulkan_lighting.c

	Vulkan lighting pass pipeline

	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/2/23

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

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/qfplist.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/staging.h"

#include "compat.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static void
find_visible_lights (vulkan_ctx_t *ctx)
{
	//qfv_device_t *device = ctx->device;
	//qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;
	lightingframe_t *lframe = &lctx->frames.a[ctx->curFrame];

	mleaf_t    *leaf = r_viewleaf;
	model_t    *model = r_worldentity.renderer.model;

	if (!leaf || !model) {
		return;
	}

	if (leaf != lframe->leaf) {
		double start = Sys_DoubleTime ();
		byte        pvs[MAP_PVS_BYTES];

		Mod_LeafPVS_set (leaf, model, 0, pvs);
		memcpy (lframe->pvs, pvs, sizeof (pvs));
		for (int i = 0; i < model->brush.numleafs; i++) {
			if (pvs[i / 8] & (1 << (i % 8))) {
				Mod_LeafPVS_mix (model->brush.leafs + i, model, 0, lframe->pvs);
			}
		}
		lframe->leaf = leaf;

		double end = Sys_DoubleTime ();
		Sys_Printf ("find_visible_lights: %.5gus\n", (end - start) * 1e6);

		int visible = 0;
		memset (lframe->lightvis.a, 0, lframe->lightvis.size * sizeof (byte));
		for (size_t i = 0; i < lctx->lightleafs.size; i++) {
			int         l = lctx->lightleafs.a[i];
			if (lframe->pvs[l / 8] & (1 << (l % 8))) {
				lframe->lightvis.a[i] = 1;
				visible++;
			}
		}
		Sys_Printf ("find_visible_lights: %d / %zd visible\n", visible,
					 lframe->lightvis.size);
	}
}

static void
update_lights (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;
	lightingframe_t *lframe = &lctx->frames.a[ctx->curFrame];

	find_visible_lights (ctx);

	dlight_t   *lights[NUM_LIGHTS];
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	qfv_light_buffer_t *light_data = QFV_PacketExtend (packet,
													   sizeof (*light_data));

	light_data->lightCount = 0;
	R_FindNearLights (r_origin, NUM_LIGHTS - 1, lights);
	for (int i = 0; i < NUM_LIGHTS - 1; i++) {
		if (!lights[i]) {
			break;
		}
		light_data->lightCount++;
		VectorCopy (lights[i]->color, light_data->lights[i].color);
		VectorCopy (lights[i]->origin, light_data->lights[i].position);
		light_data->lights[i].radius = lights[i]->radius;
		light_data->lights[i].intensity = 16;
		VectorZero (light_data->lights[i].direction);
		light_data->lights[i].cone = 1;
	}
	for (size_t i = 0;
		 i < lframe->lightvis.size && light_data->lightCount < NUM_LIGHTS; i++) {
		if (lframe->lightvis.a[i]) {
			light_data->lights[light_data->lightCount++] = lctx->lights.a[i];
		}
	}

	VkBufferMemoryBarrier wr_barriers[] = {
		{   VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			lframe->light_buffer, 0, sizeof (qfv_light_buffer_t) },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0, 0, 0, 1, wr_barriers, 0, 0);
	VkBufferCopy copy_region[] = {
		{ packet->offset, 0, sizeof (qfv_light_buffer_t) },
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, ctx->staging->buffer,
							lframe->light_buffer, 1, &copy_region[0]);
	VkBufferMemoryBarrier rd_barriers[] = {
		{   VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, 0,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			lframe->light_buffer, 0, sizeof (qfv_light_buffer_t) },
	};
	dfunc->vkCmdPipelineBarrier (packet->cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
								 0, 0, 0, 1, rd_barriers, 0, 0);
	QFV_PacketSubmit (packet);
}

void
Vulkan_Lighting_Draw (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	update_lights (ctx);

	lightingctx_t *lctx = ctx->lighting_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	lightingframe_t *lframe = &lctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = lframe->cmd;

	DARRAY_APPEND (&cframe->cmdSets[QFV_passLighting], cmd);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		ctx->renderpass, QFV_passLighting,
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
							  lctx->pipeline);

	lframe->bufferInfo[0].buffer = lframe->light_buffer;
	lframe->imageInfo[0].imageView = ctx->attachment_views->a[QFV_attachDepth];
	lframe->imageInfo[1].imageView = ctx->attachment_views->a[QFV_attachColor];
	lframe->imageInfo[2].imageView
		= ctx->attachment_views->a[QFV_attachNormal];
	lframe->imageInfo[3].imageView
		= ctx->attachment_views->a[QFV_attachPosition];
	dfunc->vkUpdateDescriptorSets (device->dev,
								   LIGHTING_BUFFER_INFOS + LIGHTING_IMAGE_INFOS,
								   lframe->descriptors, 0, 0);

	VkDescriptorSet sets[] = {
		lframe->descriptors[1].dstSet,
		lframe->descriptors[0].dstSet,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									lctx->layout, 0, 2, sets, 0, 0);

	VkViewport  viewport = {0, 0, vid.width, vid.height, 0, 1};
	VkRect2D    scissor = { {0, 0}, {vid.width, vid.height} };
	dfunc->vkCmdSetViewport (cmd, 0, 1, &viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &scissor);

	VkDeviceSize offset = 0;
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 1, &ctx->quad_buffer, &offset);
	dfunc->vkCmdDraw (cmd, 4, 1, 0, 0);

	dfunc->vkEndCommandBuffer (cmd);
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
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	0, 0, 0
};

void
Vulkan_Lighting_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	lightingctx_t *lctx = calloc (1, sizeof (lightingctx_t));
	ctx->lighting_context = lctx;

	DARRAY_INIT (&lctx->lights, 16);
	DARRAY_INIT (&lctx->lightleafs, 16);

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&lctx->frames, frames);
	DARRAY_RESIZE (&lctx->frames, frames);
	lctx->frames.grow = 0;

	lctx->pipeline = Vulkan_CreatePipeline (ctx, "lighting");
	lctx->layout = Vulkan_CreatePipelineLayout (ctx, "lighting_layout");

	__auto_type lbuffers = QFV_AllocBufferSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		lbuffers->a[i] = QFV_CreateBuffer (device, sizeof (qfv_light_buffer_t),
										   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
										   | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_BUFFER,
							 lbuffers->a[i],
							 va (ctx->va_ctx, "buffer:lighting:%zd", i));
	}
	VkMemoryRequirements requirements;
	dfunc->vkGetBufferMemoryRequirements (device->dev, lbuffers->a[0],
										  &requirements);
	lctx->light_memory = QFV_AllocBufferMemory (device, lbuffers->a[0],
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										frames * requirements.size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 lctx->light_memory, "memory:lighting");


	__auto_type cmdSet = QFV_AllocCommandBufferSet (1, alloca);

	__auto_type attach = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	__auto_type lights = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		attach->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "lighting_attach");
		lights->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "lighting_lights");
	}
	__auto_type attach_pool = Vulkan_CreateDescriptorPool (ctx,
														"lighting_attach_pool");
	__auto_type lights_pool = Vulkan_CreateDescriptorPool (ctx,
														"lighting_lights_pool");

	__auto_type attach_set = QFV_AllocateDescriptorSet (device, attach_pool,
														attach);
	__auto_type lights_set = QFV_AllocateDescriptorSet (device, lights_pool,
														lights);
	for (size_t i = 0; i < frames; i++) {
		__auto_type lframe = &lctx->frames.a[i];

		DARRAY_INIT (&lframe->lightvis, 16);

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdSet);
		lframe->cmd = cmdSet->a[0];

		lframe->light_buffer = lbuffers->a[i];
		QFV_BindBufferMemory (device, lbuffers->a[i], lctx->light_memory,
							  i * requirements.size);

		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 lframe->cmd, "cmd:lighting");
		for (int j = 0; j < LIGHTING_BUFFER_INFOS; j++) {
			lframe->bufferInfo[j] = base_buffer_info;
			lframe->descriptors[j] = base_buffer_write;
			lframe->descriptors[j].dstSet = lights_set->a[i];
			lframe->descriptors[j].dstBinding = j;
			lframe->descriptors[j].pBufferInfo = &lframe->bufferInfo[j];
		}
		for (int j = 0; j < LIGHTING_IMAGE_INFOS; j++) {
			lframe->imageInfo[j] = base_image_info;
			lframe->imageInfo[j].sampler = 0;
			int k = j + LIGHTING_BUFFER_INFOS;
			lframe->descriptors[k] = base_image_write;
			lframe->descriptors[k].dstSet = attach_set->a[i];
			lframe->descriptors[k].dstBinding = j;
			lframe->descriptors[k].pImageInfo = &lframe->imageInfo[j];
		}
	}
	free (attach_set);
	free (lights_set);
}

void
Vulkan_Lighting_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	for (size_t i = 0; i < lctx->frames.size; i++) {
		lightingframe_t *lframe = &lctx->frames.a[i];
		dfunc->vkDestroyBuffer (device->dev, lframe->light_buffer, 0);
		DARRAY_CLEAR (&lframe->lightvis);
	}
	dfunc->vkFreeMemory (device->dev, lctx->light_memory, 0);
	dfunc->vkDestroyPipeline (device->dev, lctx->pipeline, 0);
	DARRAY_CLEAR (&lctx->lights);
	DARRAY_CLEAR (&lctx->lightleafs);
	free (lctx->frames.a);
	free (lctx);
}

static void
parse_light (qfv_light_t *light, const plitem_t *entity,
			 const plitem_t *targets)
{
	const char *str;

	/*Sys_Printf ("{\n");
	for (int i = PL_D_NumKeys (entity); i-- > 0; ) {
		const char *field = PL_KeyAtIndex (entity, i);
		const char *value = PL_String (PL_ObjectForKey (entity, field));
		Sys_Printf ("\t%s = %s\n", field, value);
	}
	Sys_Printf ("}\n");*/

	light->cone = 1;
	light->intensity = 1;
	light->radius = 300;
	VectorSet (1, 1, 1, light->color);

	if ((str = PL_String (PL_ObjectForKey (entity, "origin")))) {
		sscanf (str, "%f %f %f", VectorExpandAddr (light->position));
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "target")))) {
		vec3_t      position = {};
		plitem_t   *target = PL_ObjectForKey (targets, str);
		if (target) {
			if ((str = PL_String (PL_ObjectForKey (target, "origin")))) {
				sscanf (str, "%f %f %f", VectorExpandAddr (position));
			}
			VectorSubtract (position, light->position, light->direction);
			VectorNormalize (light->direction);
		}

		float angle = 40;
		if ((str = PL_String (PL_ObjectForKey (entity, "angle")))) {
			angle = atof (str);
		}
		light->cone = -cos (angle * M_PI / 360); // half angle
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "light_lev")))
		|| (str = PL_String (PL_ObjectForKey (entity, "_light")))) {
		light->radius = atof (str);
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "color")))
		|| (str = PL_String (PL_ObjectForKey (entity, "_color")))) {
		sscanf (str, "%f %f %f", VectorExpandAddr (light->color));
	}
}

void
Vulkan_LoadLights (model_t *model, const char *entity_data, vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = ctx->lighting_context;
	plitem_t   *entities = 0;

	lctx->lights.size = 0;
	lctx->lightleafs.size = 0;

	script_t   *script = Script_New ();
	Script_Start (script, "ent data", entity_data);

	if (Script_GetToken (script, 1)) {
		if (strequal (script->token->str, "(")) {
			// new style (plist) entity data
			entities = PL_GetPropertyList (entity_data, &ctx->hashlinks);
		} else {
			// old style entity data
			Script_UngetToken (script);
			// FIXME ED_ConvertToPlist aborts if an error is encountered.
			entities = ED_ConvertToPlist (script, 0, &ctx->hashlinks);
		}
	}
	Script_Delete (script);

	if (entities) {
		plitem_t   *targets = PL_NewDictionary (&ctx->hashlinks);

		// find all the targets so spotlights can be aimed
		for (int i = 1; i < PL_A_NumObjects (entities); i++) {
			plitem_t   *entity = PL_ObjectAtIndex (entities, i);
			const char *targetname = PL_String (PL_ObjectForKey (entity,
																 "targetname"));
			if (targetname && !PL_ObjectForKey (targets, targetname)) {
				PL_D_AddObject (targets, targetname, entity);
			}
		}

		for (int i = 1; i < PL_A_NumObjects (entities); i++) {
			plitem_t   *entity = PL_ObjectAtIndex (entities, i);
			const char *classname = PL_String (PL_ObjectForKey (entity,
																"classname"));
			if (classname && strnequal (classname, "light", 5)) {
				qfv_light_t light = {};

				parse_light (&light, entity, targets);
				DARRAY_APPEND (&lctx->lights, light);
				mleaf_t    *leaf = Mod_PointInLeaf (&light.position[0],
													model);
				DARRAY_APPEND (&lctx->lightleafs, leaf - model->brush.leafs);
				printf ("[%g, %g, %g] %g, [%g %g %g] %g, [%g %g %g] %g, %zd\n",
						VectorExpand (light.color), light.intensity,
						VectorExpand (light.position), light.radius,
						VectorExpand (light.direction), light.cone,
						leaf - model->brush.leafs);
			}
		}
		printf ("%zd frames\n", ctx->frames.size);
		for (size_t i = 0; i < ctx->frames.size; i++) {
			lightingframe_t *lframe = &lctx->frames.a[i];
			DARRAY_RESIZE (&lframe->lightvis, lctx->lights.size);
		}
		// targets does not own the objects, so need to remove them before
		// freeing targets
		for (int i = PL_D_NumKeys (targets); i-- > 0; ) {
			PL_RemoveObjectForKey (targets, PL_KeyAtIndex (targets, i));
		}
		PL_Free (targets);
		PL_Free (entities);
	}
	printf ("loaded %zd lights\n", lctx->lights.size);
}
