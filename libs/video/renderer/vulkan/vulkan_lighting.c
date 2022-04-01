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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/script.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/projection.h"
#include "QF/Vulkan/renderpass.h"
#include "QF/Vulkan/staging.h"

#include "compat.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static vec4f_t  ref_direction = { 0, 0, 1, 0 };

static void
expand_pvs (set_t *pvs, model_t *model)
{
	set_t       base_pvs = SET_STATIC_INIT (model->brush.visleafs, alloca);
	set_assign (&base_pvs, pvs);
	for (unsigned i = 0; i < model->brush.visleafs; i++) {
		if (set_is_member (&base_pvs, i)) {
			Mod_LeafPVS_mix (model->brush.leafs + i + 1, model, 0, pvs);
		}
	}
}

static void
find_visible_lights (vulkan_ctx_t *ctx)
{
	//qfv_device_t *device = ctx->device;
	//qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;
	lightingframe_t *lframe = &lctx->frames.a[ctx->curFrame];

	mleaf_t    *leaf = r_refdef.viewleaf;
	model_t    *model = r_refdef.worldmodel;

	if (!leaf || !model) {
		return;
	}

	if (leaf != lframe->leaf) {
		//double start = Sys_DoubleTime ();
		int         flags = 0;

		if (leaf == model->brush.leafs) {
			set_everything (lframe->pvs);
		} else {
			Mod_LeafPVS_set (leaf, model, 0, lframe->pvs);
			expand_pvs (lframe->pvs, model);
		}
		for (unsigned i = 0; i < model->brush.visleafs; i++) {
			if (set_is_member (lframe->pvs, i)) {
				flags |= model->brush.leaf_flags[i + 1];
			}
		}
		lframe->leaf = leaf;

		//double end = Sys_DoubleTime ();
		//Sys_Printf ("find_visible_lights: %.5gus\n", (end - start) * 1e6);

		int visible = 0;
		memset (lframe->lightvis.a, 0, lframe->lightvis.size * sizeof (byte));
		for (size_t i = 0; i < lctx->lightleafs.size; i++) {
			int         l = lctx->lightleafs.a[i];
			if ((l == -1 && (flags & SURF_DRAWSKY))
				|| set_is_member (lframe->pvs, l)) {
				lframe->lightvis.a[i] = 1;
				visible++;
			}
		}
		//Sys_Printf ("find_visible_lights: %d / %zd visible\n", visible,
		//			 lframe->lightvis.size);
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

	dlight_t   *lights[MaxLights];
	qfv_packet_t *packet = QFV_PacketAcquire (ctx->staging);
	qfv_light_buffer_t *light_data = QFV_PacketExtend (packet,
													   sizeof (*light_data));

	for (int i = 0; i < NumStyles; i++) {
		light_data->intensity[i] = d_lightstylevalue[i] / 65536.0;
	}
	// dynamic lights seem a tad faint, so 16x map lights
	light_data->intensity[64] = 1 / 16.0;
	light_data->intensity[65] = 1 / 16.0;
	light_data->intensity[66] = 1 / 16.0;
	light_data->intensity[67] = 1 / 16.0;

	light_data->distFactor1 = 1 / 128.0;
	light_data->distFactor2 = 1 / 16384.0;

	light_data->lightCount = 0;
	R_FindNearLights (r_refdef.frame.position, MaxLights - 1, lights);
	for (int i = 0; i < MaxLights - 1; i++) {
		if (!lights[i]) {
			break;
		}
		light_data->lightCount++;
		VectorCopy (lights[i]->color, light_data->lights[i].color);
		VectorCopy (lights[i]->origin, light_data->lights[i].position);
		light_data->lights[i].light = lights[i]->radius;
		light_data->lights[i].data = 64;	// default dynamic light
		VectorZero (light_data->lights[i].direction);
		light_data->lights[i].cone = 1;
	}
	for (size_t i = 0; (i < lframe->lightvis.size
						&& light_data->lightCount < MaxLights); i++) {
		if (lframe->lightvis.a[i]) {
			light_data->lights[light_data->lightCount++] = lctx->lights.a[i];
		}
	}

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];
	bb.barrier.buffer = lframe->light_buffer;
	bb.barrier.size = sizeof (qfv_light_buffer_t);
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	VkBufferCopy copy_region[] = {
		{ packet->offset, 0, sizeof (qfv_light_buffer_t) },
	};
	dfunc->vkCmdCopyBuffer (packet->cmd, ctx->staging->buffer,
							lframe->light_buffer, 1, &copy_region[0]);
	bb = bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
	bb.barrier.buffer = lframe->light_buffer;
	bb.barrier.size = sizeof (qfv_light_buffer_t);
	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);
	QFV_PacketSubmit (packet);
}

void
Vulkan_Lighting_Draw (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_renderpass_t *renderpass = rFrame->renderpass;

	update_lights (ctx);

	lightingctx_t *lctx = ctx->lighting_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	lightingframe_t *lframe = &lctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = lframe->cmd;

	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passLighting], cmd);

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		renderpass->renderpass, QFV_passLighting,
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, "lighting", { 0.6, 0.5, 0.6, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  lctx->pipeline);

	lframe->bufferInfo[0].buffer = lframe->light_buffer;
	lframe->attachInfo[0].imageView
		= renderpass->attachment_views->a[QFV_attachDepth];
	lframe->attachInfo[1].imageView
		= renderpass->attachment_views->a[QFV_attachColor];
	lframe->attachInfo[2].imageView
		= renderpass->attachment_views->a[QFV_attachEmission];
	lframe->attachInfo[3].imageView
		= renderpass->attachment_views->a[QFV_attachNormal];
	lframe->attachInfo[4].imageView
		= renderpass->attachment_views->a[QFV_attachPosition];
	dfunc->vkUpdateDescriptorSets (device->dev,
								   LIGHTING_DESCRIPTORS,
								   lframe->descriptors, 0, 0);

	VkDescriptorSet sets[] = {
		lframe->attachWrite[0].dstSet,
		lframe->bufferWrite[0].dstSet,
		lframe->shadowWrite.dstSet,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									lctx->layout, 0, 3, sets, 0, 0);

	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);

	QFV_duCmdEndLabel (device, cmd);
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
static VkWriteDescriptorSet base_attachment_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	0, 0, 0
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	0, 0, 0
};

void
Vulkan_Lighting_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "lighting init");

	lightingctx_t *lctx = calloc (1, sizeof (lightingctx_t));
	ctx->lighting_context = lctx;

	DARRAY_INIT (&lctx->lights, 16);
	DARRAY_INIT (&lctx->lightleafs, 16);
	DARRAY_INIT (&lctx->lightmats, 16);
	DARRAY_INIT (&lctx->lightlayers, 16);
	DARRAY_INIT (&lctx->lightimages, 16);
	DARRAY_INIT (&lctx->lightviews, 16);

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&lctx->frames, frames);
	DARRAY_RESIZE (&lctx->frames, frames);
	lctx->frames.grow = 0;

	lctx->pipeline = Vulkan_CreateGraphicsPipeline (ctx, "lighting");
	lctx->layout = Vulkan_CreatePipelineLayout (ctx, "lighting_layout");
	lctx->sampler = Vulkan_CreateSampler (ctx, "shadow_sampler");

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
	size_t      light_size = QFV_NextOffset (requirements.size, &requirements);
	lctx->light_memory = QFV_AllocBufferMemory (device, lbuffers->a[0],
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								frames * light_size, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 lctx->light_memory, "memory:lighting");


	__auto_type cmdSet = QFV_AllocCommandBufferSet (1, alloca);

	__auto_type attach = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	__auto_type lights = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	__auto_type shadow = QFV_AllocDescriptorSetLayoutSet (frames, alloca);
	for (size_t i = 0; i < frames; i++) {
		attach->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "lighting_attach");
		lights->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "lighting_lights");
		shadow->a[i] = Vulkan_CreateDescriptorSetLayout (ctx,
														 "lighting_shadow");
	}
	__auto_type attach_pool = Vulkan_CreateDescriptorPool (ctx,
														"lighting_attach_pool");
	__auto_type lights_pool = Vulkan_CreateDescriptorPool (ctx,
														"lighting_lights_pool");
	__auto_type shadow_pool = Vulkan_CreateDescriptorPool (ctx,
														"lighting_shadow_pool");

	__auto_type attach_set = QFV_AllocateDescriptorSet (device, attach_pool,
														attach);
	__auto_type lights_set = QFV_AllocateDescriptorSet (device, lights_pool,
														lights);
	__auto_type shadow_set = QFV_AllocateDescriptorSet (device, shadow_pool,
														shadow);
	VkDeviceSize light_offset = 0;
	for (size_t i = 0; i < frames; i++) {
		__auto_type lframe = &lctx->frames.a[i];

		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 attach_set->a[i],
							 va (ctx->va_ctx, "lighting:attach_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 lights_set->a[i],
							 va (ctx->va_ctx, "lighting:lights_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 shadow_set->a[i],
							 va (ctx->va_ctx, "lighting:shadow_set:%zd", i));

		DARRAY_INIT (&lframe->lightvis, 16);
		lframe->pvs = 0;
		lframe->leaf = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdSet);
		lframe->cmd = cmdSet->a[0];

		lframe->light_buffer = lbuffers->a[i];
		QFV_BindBufferMemory (device, lbuffers->a[i], lctx->light_memory,
							  light_offset);
		light_offset += light_size;

		QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
							 lframe->cmd, "cmd:lighting");
		for (int j = 0; j < LIGHTING_BUFFER_INFOS; j++) {
			lframe->bufferInfo[j] = base_buffer_info;
			lframe->bufferWrite[j] = base_buffer_write;
			lframe->bufferWrite[j].dstSet = lights_set->a[i];
			lframe->bufferWrite[j].dstBinding = j;
			lframe->bufferWrite[j].pBufferInfo = &lframe->bufferInfo[j];
		}
		for (int j = 0; j < LIGHTING_ATTACH_INFOS; j++) {
			lframe->attachInfo[j] = base_image_info;
			lframe->attachInfo[j].sampler = 0;
			lframe->attachWrite[j] = base_attachment_write;
			lframe->attachWrite[j].dstSet = attach_set->a[i];
			lframe->attachWrite[j].dstBinding = j;
			lframe->attachWrite[j].pImageInfo = &lframe->attachInfo[j];
		}
		for (int j = 0; j < LIGHTING_SHADOW_INFOS; j++) {
			lframe->shadowInfo[j] = base_image_info;
			lframe->shadowInfo[j].sampler = lctx->sampler;
			lframe->shadowInfo[j].imageView = ctx->default_black->view;
		}
		lframe->shadowWrite = base_image_write;
		lframe->shadowWrite.dstSet = shadow_set->a[i];
		lframe->shadowWrite.dstBinding = 0;
		lframe->shadowWrite.descriptorCount
			= min (MaxLights,
			device->physDev->properties.limits.maxPerStageDescriptorSamplers);
		lframe->shadowWrite.pImageInfo = lframe->shadowInfo;
	}
	free (attach_set);
	free (lights_set);
	qfvPopDebug (ctx);
}

static void
clear_shadows (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	if (lctx->shadow_memory) {
		dfunc->vkFreeMemory (device->dev, lctx->shadow_memory, 0);
	}
	for (size_t i = 0; i < lctx->lightviews.size; i++) {
		dfunc->vkDestroyImageView (device->dev, lctx->lightviews.a[i], 0);
	}
	for (size_t i = 0; i < lctx->lightimages.size; i++) {
		dfunc->vkDestroyImage (device->dev, lctx->lightimages.a[i], 0);
	}
	lctx->lightimages.size = 0;
	lctx->lightviews.size = 0;
}

void
Vulkan_Lighting_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	clear_shadows (ctx);

	for (size_t i = 0; i < lctx->frames.size; i++) {
		lightingframe_t *lframe = &lctx->frames.a[i];
		dfunc->vkDestroyBuffer (device->dev, lframe->light_buffer, 0);
		DARRAY_CLEAR (&lframe->lightvis);
	}
	dfunc->vkFreeMemory (device->dev, lctx->light_memory, 0);
	dfunc->vkDestroyPipeline (device->dev, lctx->pipeline, 0);
	DARRAY_CLEAR (&lctx->lights);
	DARRAY_CLEAR (&lctx->lightleafs);
	DARRAY_CLEAR (&lctx->lightmats);
	DARRAY_CLEAR (&lctx->lightimages);
	DARRAY_CLEAR (&lctx->lightlayers);
	DARRAY_CLEAR (&lctx->lightviews);
	free (lctx->frames.a);
	free (lctx);
}

static void
dump_light (qfv_light_t *light, int leaf, mat4f_t mat)
{
	Sys_MaskPrintf (SYS_vulkan,
					"[%g, %g, %g] %d %d %d, "
					"[%g %g %g] %g, [%g %g %g] %g, %d\n",
					VectorExpand (light->color),
					(light->data & 0x07f),
					(light->data & 0x380) >> 7,
					(light->data & 0xc00) >> 10,
					VectorExpand (light->position), light->light,
					VectorExpand (light->direction), light->cone,
					leaf);
	Sys_MaskPrintf (SYS_vulkan, "    " VEC4F_FMT "\n", MAT4_ROW (mat, 0));
	Sys_MaskPrintf (SYS_vulkan, "    " VEC4F_FMT "\n", MAT4_ROW (mat, 1));
	Sys_MaskPrintf (SYS_vulkan, "    " VEC4F_FMT "\n", MAT4_ROW (mat, 2));
	Sys_MaskPrintf (SYS_vulkan, "    " VEC4F_FMT "\n", MAT4_ROW (mat, 3));
}

static float
parse_float (const char *str, float defval)
{
	float       val = defval;
	if (str) {
		char       *end;
		val = strtof (str, &end);
		if (end == str) {
			val = defval;
		}
	}
	return val;
}

static void
parse_vector (const char *str, vec_t *val)
{
	if (str) {
		int         num = sscanf (str, "%f %f %f", VectorExpandAddr (val));
		while (num < 3) {
			val[num++] = 0;
		}
	}
}

static float
ecos (float ang)
{
	if (ang == 90 || ang == -90) {
		return 0;
	}
	if (ang == 180 || ang == -180) {
		return -1;
	}
	if (ang == 0 || ang == 360) {
		return 1;
	}
	return cos (ang * M_PI / 180);
}

static float
esin (float ang)
{
	if (ang == 90) {
		return 1;
	}
	if (ang == -90) {
		return -1;
	}
	if (ang == 180 || ang == -180) {
		return 0;
	}
	if (ang == 0 || ang == 360) {
		return 0;
	}
	return sin (ang * M_PI / 180);
}

static void
sun_vector (const vec_t *ang, vec_t *vec)
{
	// ang is yaw, pitch (maybe roll, but ignored
	vec[0] = ecos (ang[1]) * ecos (ang[0]);
	vec[1] = ecos (ang[1]) * esin (ang[0]);
	vec[2] = esin (ang[1]);
}

static void
parse_sun (lightingctx_t *lctx, plitem_t *entity, model_t *model)
{
	qfv_light_t light = {};
	float       sunlight;
	//float       sunlight2;
	vec3_t      sunangle = { 0, -90, 0 };

	set_expand (lctx->sun_pvs, model->brush.visleafs);
	set_empty (lctx->sun_pvs);
	sunlight = parse_float (PL_String (PL_ObjectForKey (entity,
													    "_sunlight")), 0);
	//sunlight2 = parse_float (PL_String (PL_ObjectForKey (entity,
	//													 "_sunlight2")), 0);
	parse_vector (PL_String (PL_ObjectForKey (entity, "_sun_mangle")),
				  sunangle);
	if (sunlight <= 0) {
		return;
	}
	VectorSet (1, 1, 1, light.color);
	light.data = LM_INFINITE | ST_CASCADE;
	light.light = sunlight;
	sun_vector (sunangle, light.direction);
	light.cone = 1;
	DARRAY_APPEND (&lctx->lights, light);
	DARRAY_APPEND (&lctx->lightleafs, -1);

	// Any leaf with sky surfaces can potentially see the sun, thus put
	// the sun "in" every leaf with a sky surface
	// however, skip leaf 0 as it is the exterior solid leaf
	for (unsigned l = 1; l < model->brush.modleafs; l++) {
		if (model->brush.leaf_flags[l] & SURF_DRAWSKY) {
			set_add (lctx->sun_pvs, l - 1); //pvs is 1-based
		}
	}
	// any leaf visible from a leaf with a sky surface (and thus the sun)
	// can receive shadows from the sun
	expand_pvs (lctx->sun_pvs, model);
}

static void
parse_light (qfv_light_t *light, const plitem_t *entity,
			 const plitem_t *targets)
{
	const char *str;
	int         model = 0;

	/*Sys_Printf ("{\n");
	for (int i = PL_D_NumKeys (entity); i-- > 0; ) {
		const char *field = PL_KeyAtIndex (entity, i);
		const char *value = PL_String (PL_ObjectForKey (entity, field));
		Sys_Printf ("\t%s = %s\n", field, value);
	}
	Sys_Printf ("}\n");*/

	light->cone = 1;
	light->data = 0;
	light->light = 300;
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
		light->light = atof (str);
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "style")))) {
		light->data = atoi (str) & 0x3f;
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "delay")))) {
		model = (atoi (str) & 0x7) << 7;
		if (model == LM_INVERSE2) {
			model = LM_INVERSE3;	//FIXME for marcher (need a map)
		}
		light->data |= model;
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "color")))
		|| (str = PL_String (PL_ObjectForKey (entity, "_color")))) {
		sscanf (str, "%f %f %f", VectorExpandAddr (light->color));
		VectorScale (light->color, 1/255.0, light->color);
	}

	if (model == LM_INFINITE) {
		light->data |= ST_CASCADE;
	} else if (model != LM_AMBIENT) {
		if (light->cone > -0.5) {
			light->data |= ST_CUBE;
		} else {
			light->data |= ST_PLANE;
		}
	}
}

static void
create_light_matrices (lightingctx_t *lctx)
{
	DARRAY_RESIZE (&lctx->lightmats, lctx->lights.size);
	for (size_t i = 0; i < lctx->lights.size; i++) {
		qfv_light_t *light = &lctx->lights.a[i];
		mat4f_t     view;
		mat4f_t     proj;

		switch (light->data & ShadowMask) {
			default:
			case ST_NONE:
			case ST_CUBE:
				mat4fidentity (view);
				break;
			case ST_CASCADE:
			case ST_PLANE:
				//FIXME will fail for -ref_direction
				mat4fquat (view, qrotf (loadvec3f (light->direction),
										ref_direction));
				break;
		}
		VectorNegate (light->position, view[3]);

		switch (light->data & ShadowMask) {
			case ST_NONE:
				mat4fidentity (proj);
				break;
			case ST_CUBE:
				QFV_PerspectiveTan (proj, 1, 1);
				break;
			case ST_CASCADE:
				// dependent on view fustrum and cascade level
				mat4fidentity (proj);
				break;
			case ST_PLANE:
				QFV_PerspectiveCos (proj, light->cone);
				break;
		}
		mmulf (lctx->lightmats.a[i], proj, view);
	}
}

static int
light_compare (const void *_l2, const void *_l1)
{
	const qfv_light_t *l1 = _l1;
	const qfv_light_t *l2 = _l2;

	if (l1->light == l2->light) {
		return (l1->data & ShadowMask) - (l2->data & ShadowMask);
	}
	return l1->light - l2->light;
}

static VkImage
create_map (int size, int layers, int cube, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	if (layers < 6) {
		cube = 0;
	}

	VkImageCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0,
		cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0, VK_IMAGE_TYPE_2D,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		{ size, size, 1 }, 1, layers,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE,
		0, 0,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VkImage     image;
	dfunc->vkCreateImage (device->dev, &createInfo, 0, &image);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE, image,
						 va (ctx->va_ctx, "image:shadowmap:%d:%d",
							 size, layers));
	return image;
}

static VkImageView
create_view (VkImage image, int baseLayer, int data, int id, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	int         layers = 0;
	VkImageViewType type = 0;
	const char *viewtype = 0;

	switch (data & ShadowMask) {
		case ST_NONE:
			return 0;
		case ST_PLANE:
			layers = 1;
			type = VK_IMAGE_VIEW_TYPE_2D;
			viewtype = "plane";
			break;
		case ST_CASCADE:
			layers = 4;
			type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewtype = "cascade";
			break;
		case ST_CUBE:
			layers = 6;
			type = VK_IMAGE_VIEW_TYPE_CUBE;
			viewtype = "cube";
			break;
	}

	VkImageViewCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0,
		0,
		image, type, VK_FORMAT_X8_D24_UNORM_PACK32,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, baseLayer, layers }
	};

	VkImageView view;
	dfunc->vkCreateImageView (device->dev, &createInfo, 0, &view);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, view,
						 va (ctx->va_ctx, "iview:shadowmap:%s:%d",
							 viewtype, id));
	(void) viewtype;//silence unused warning when vulkan debug disabled
	return view;
}

static void
build_shadow_maps (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	qfv_physdev_t *physDev = device->physDev;
	int         maxLayers = physDev->properties.limits.maxImageArrayLayers;
	qfv_light_t *lights = lctx->lights.a;
	int         numLights = lctx->lights.size;
	int         size = -1;
	int         numLayers = 0;
	int         totalLayers = 0;
	int        *imageMap = alloca (numLights * sizeof (int));
	size_t      memsize = 0;

	DARRAY_RESIZE (&lctx->lightlayers, numLights);
	qsort (lights, numLights, sizeof (qfv_light_t), light_compare);
	for (int i = 0; i < numLights; i++) {
		int         shadow = lights[i].data & ShadowMask;
		int         layers = 1;
		if (shadow == ST_CASCADE || shadow == ST_NONE) {
			// cascade shadows will be handled separately, and "none" has no
			// shadow map at all
			imageMap[i] = -1;
			continue;
		}
		if (shadow == ST_CUBE) {
			layers = 6;
		}
		if (size != (int) lights[i].light || numLayers + layers > maxLayers) {
			if (numLayers) {
				VkImage     shadow_map = create_map (size, numLayers, 1, ctx);
				DARRAY_APPEND (&lctx->lightimages, shadow_map);
				numLayers = 0;
			}
			size = lights[i].light;
		}
		imageMap[i] = lctx->lightimages.size;
		lctx->lightlayers.a[i] = numLayers;
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		VkImage     shadow_map = create_map (size, numLayers, 1, ctx);
		DARRAY_APPEND (&lctx->lightimages, shadow_map);
	}

	numLayers = 0;
	size = 1024;
	for (int i = 0; i < numLights; i++) {
		int         shadow = lights[i].data & ShadowMask;
		int         layers = 4;

		if (shadow != ST_CASCADE) {
			continue;
		}
		if (numLayers + layers > maxLayers) {
			VkImage     shadow_map = create_map (size, numLayers, 0, ctx);
			DARRAY_APPEND (&lctx->lightimages, shadow_map);
			numLayers = 0;
		}
		imageMap[i] = lctx->lightimages.size;
		lctx->lightlayers.a[i] = numLayers;
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		VkImage     shadow_map = create_map (size, numLayers, 0, ctx);
		DARRAY_APPEND (&lctx->lightimages, shadow_map);
	}

	for (size_t i = 0; i < lctx->lightimages.size; i++) {
		memsize += QFV_GetImageSize (device, lctx->lightimages.a[i]);
	}
	lctx->shadow_memory = QFV_AllocImageMemory (device, lctx->lightimages.a[0],
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
												memsize, 0);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DEVICE_MEMORY,
						 lctx->shadow_memory, "memory:shadowmap");

	size_t      offset = 0;
	for (size_t i = 0; i < lctx->lightimages.size; i++) {
		dfunc->vkBindImageMemory (device->dev, lctx->lightimages.a[i],
								  lctx->shadow_memory, offset);
		offset += QFV_GetImageSize (device, lctx->lightimages.a[i]);
	}

	DARRAY_RESIZE (&lctx->lightviews, numLights);
	for (int i = 0; i < numLights; i++) {
		if (imageMap[i] == -1) {
			lctx->lightviews.a[i] = 0;
			continue;
		}
		lctx->lightviews.a[i] = create_view (lctx->lightimages.a[imageMap[i]],
											 lctx->lightlayers.a[i],
											 lctx->lights.a[i].data, i, ctx);
	}
	Sys_MaskPrintf (SYS_vulkan, "shadow maps: %d layers in %zd images: %zd\n",
					totalLayers, lctx->lightimages.size, memsize);
}

static void
locate_lights (model_t *model, lightingctx_t *lctx)
{
	qfv_light_t *lights = lctx->lights.a;
	DARRAY_RESIZE (&lctx->lightleafs, lctx->lights.size);
	for (size_t i = 0; i < lctx->lights.size; i++) {
		mleaf_t    *leaf = Mod_PointInLeaf (&lights[i].position[0], model);
		lctx->lightleafs.a[i] = leaf - model->brush.leafs - 1;
	}
}

void
Vulkan_LoadLights (model_t *model, const char *entity_data, vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = ctx->lighting_context;
	plitem_t   *entities = 0;

	lctx->lights.size = 0;
	lctx->lightleafs.size = 0;
	lctx->lightmats.size = 0;
	if (lctx->sun_pvs) {
		set_delete (lctx->sun_pvs);
	}
	lctx->sun_pvs = set_new_size (model->brush.visleafs);
	for (size_t i = 0; i < ctx->frames.size; i++) {
		__auto_type lframe = &lctx->frames.a[i];
		if (lframe->pvs) {
			set_delete (lframe->pvs);
		}
		lframe->pvs = set_new_size (model->brush.visleafs);
	}

	clear_shadows (ctx);

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

		for (int i = 0; i < PL_A_NumObjects (entities); i++) {
			plitem_t   *entity = PL_ObjectAtIndex (entities, i);
			const char *classname = PL_String (PL_ObjectForKey (entity,
																"classname"));
			if (!classname) {
				continue;
			}
			if (strequal (classname, "worldspawn")) {
				// parse_sun can add many lights
				parse_sun (lctx, entity, model);
			} else if (strnequal (classname, "light", 5)) {
				qfv_light_t light = {};

				parse_light (&light, entity, targets);
				// some lights have 0 output, so drop them
				if (light.light) {
					DARRAY_APPEND (&lctx->lights, light);
				}
			}
		}
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
	Sys_MaskPrintf (SYS_vulkan, "loaded %zd lights\n", lctx->lights.size);
	build_shadow_maps (lctx, ctx);
	create_light_matrices (lctx);
	locate_lights (model, lctx);
	for (size_t i = 0; i < lctx->lights.size; i++) {
		dump_light (&lctx->lights.a[i], lctx->lightleafs.a[i],
					lctx->lightmats.a[i]);
	}
}
