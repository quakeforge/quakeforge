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
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"

#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"

static VkImageView
get_view (qfv_tex_t *tex, qfv_tex_t *default_tex)
{
	if (tex) {
		return tex->view;
	}
	if (default_tex) {
		return default_tex->view;
	}
	return 0;
}

void
Vulkan_DrawAlias (struct entity_s *ent, struct vulkan_ctx_s *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	model_t    *model = ent->model;
	aliashdr_t *hdr;
	qfv_alias_mesh_t *mesh;
	float       blend;
	aliasskin_t *skin;

	if (!(hdr = model->aliashdr)) {
		hdr = Cache_Get (&model->cache);
	}
	mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);

	blend = R_AliasGetLerpedFrames (ent, hdr);

	if (0/*XXX ent->skin && ent->skin->tex*/) {
		//skin = ent->skin->tex;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (ent->skinnum, hdr);
		skin = (aliasskin_t *) ((byte *) hdr + skindesc->skin);
	}

	VkDeviceSize offsets[] = {
		ent->pose1 * hdr->poseverts,
		ent->pose2 * hdr->poseverts,
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
							   0, 16 * sizeof (float), ent->transform);
	dfunc->vkCmdPushConstants (aframe->cmd, actx->layout,
							   VK_SHADER_STAGE_VERTEX_BIT,
							   64, sizeof (float), &blend);
	aframe->imageInfo[0].imageView = get_view (skin->tex, ctx->default_white);
	aframe->imageInfo[1].imageView = get_view (skin->glow, ctx->default_black);
	aframe->imageInfo[2].imageView = get_view (skin->colora,
											   ctx->default_black);
	aframe->imageInfo[3].imageView = get_view (skin->colorb,
											   ctx->default_black);
	dfunc->vkCmdPushDescriptorSetKHR (aframe->cmd,
									  VK_PIPELINE_BIND_POINT_GRAPHICS,
									  actx->layout,
									  0, 2, aframe->descriptors
											+ ALIAS_BUFFER_INFOS);
	dfunc->vkCmdDrawIndexed (aframe->cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);
}

void
Vulkan_AliasBegin (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	//XXX quat_t      fog;

	__auto_type cframe = &ctx->framebuffers.a[ctx->curFrame];
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = aframe->cmd;
	DARRAY_APPEND (cframe->subCommand, cmd);

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
	VkViewport  viewport = {0, 0, vid.width, vid.height, 0, 1};
	VkRect2D    scissor = { {0, 0}, {vid.width, vid.height} };
	dfunc->vkCmdSetViewport (cmd, 0, 1, &viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &scissor);

	dfunc->vkUpdateDescriptorSets (device->dev, 2, aframe->descriptors, 0, 0);

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

	aliasctx_t *actx = calloc (1, sizeof (aliasctx_t));
	ctx->alias_context = actx;

	size_t      frames = ctx->framebuffers.size;
	DARRAY_INIT (&actx->frames, frames);
	DARRAY_RESIZE (&actx->frames, frames);
	actx->frames.grow = 0;

	actx->pipeline = Vulkan_CreatePipeline (ctx, "alias");
	actx->layout = QFV_GetPipelineLayout (ctx, "alias.layout");
	actx->sampler = QFV_GetSampler (ctx, "alias.sampler");

	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (2 * frames, alloca);
	for (size_t i = 0; i < layouts->size / 2; i++) {
		__auto_type mats = QFV_GetDescriptorSetLayout (ctx, "alias.matrices");
		__auto_type lights = QFV_GetDescriptorSetLayout (ctx, "alias.lights");
		layouts->a[2 * i + 0] = mats;
		layouts->a[2 * i + 1] = lights;
	}
	__auto_type pool = QFV_GetDescriptorPool (ctx, "alias.pool");

	__auto_type cmdBuffers = QFV_AllocCommandBufferSet (frames, alloca);
	QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, cmdBuffers);

	__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	for (size_t i = 0; i < frames; i++) {
		__auto_type aframe = &actx->frames.a[i];
		aframe->cmd = cmdBuffers->a[i];

		for (int j = 0; j < ALIAS_BUFFER_INFOS; j++) {
			aframe->bufferInfo[j] = base_buffer_info;
			aframe->descriptors[j] = base_buffer_write;
			aframe->descriptors[j].dstSet = sets->a[2 * i + j];
			aframe->descriptors[j].dstBinding = 0;
			aframe->descriptors[j].pBufferInfo = &aframe->bufferInfo[j];
		}
		for (int j = 0; j < ALIAS_IMAGE_INFOS; j++) {
			aframe->imageInfo[j] = base_image_info;
			aframe->imageInfo[j].sampler = actx->sampler;
			int k = j + ALIAS_BUFFER_INFOS;
			aframe->descriptors[k] = base_image_write;
			aframe->descriptors[k].dstBinding = j;
			aframe->descriptors[k].pImageInfo = &aframe->imageInfo[j];
		}
	}
	free (sets);
}

void
Vulkan_Alias_Shutdown (struct vulkan_ctx_s *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	dfunc->vkDestroyPipeline (device->dev, actx->pipeline, 0);
	DARRAY_CLEAR (&actx->frames);
	free (actx);
}
