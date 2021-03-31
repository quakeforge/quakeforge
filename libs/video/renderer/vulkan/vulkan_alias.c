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
#include "QF/entity.h"
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

static const char *alias_pass_names[] = {
	"depth",
	"g-buffer",
	"translucent",
};

static QFV_Subpass subpass_map[] = {
	QFV_passDepth,			// QFV_aliasDepth
	QFV_passGBuffer,		// QFV_aliasGBuffer
	QFV_passTranslucent,	// QFV_aliasTranslucent
};

static void
emit_commands (VkCommandBuffer cmd, int pose1, int pose2,
			   qfv_alias_skin_t *skin,
			   void *vert_constants, int vert_size,
			   void *frag_constants, int frag_size,
			   aliashdr_t *hdr, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	__auto_type mesh = (qfv_alias_mesh_t *) ((byte *) hdr + hdr->commands);

	VkDeviceSize offsets[] = {
		pose1 * hdr->poseverts * sizeof (aliasvrt_t),
		pose2 * hdr->poseverts * sizeof (aliasvrt_t),
		0,
	};
	VkBuffer    buffers[] = {
		mesh->vertex_buffer,
		mesh->vertex_buffer,
		mesh->uv_buffer,
	};
	int         bindingCount = skin ? 3 : 2;

	dfunc->vkCmdBindVertexBuffers (cmd, 0, bindingCount, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, mesh->index_buffer, 0,
								 VK_INDEX_TYPE_UINT32);
	dfunc->vkCmdPushConstants (cmd, actx->layout, VK_SHADER_STAGE_VERTEX_BIT,
							   0, vert_size, vert_constants);
	if (skin) {
		dfunc->vkCmdPushConstants (cmd, actx->layout,
								   VK_SHADER_STAGE_FRAGMENT_BIT,
								   68, frag_size, frag_constants);
		aframe->imageInfo[0].imageView = skin->view;
		dfunc->vkCmdPushDescriptorSetKHR (cmd,
										  VK_PIPELINE_BIND_POINT_GRAPHICS,
										  actx->layout,
										  0, ALIAS_IMAGE_INFOS,
										  aframe->descriptors
										  + ALIAS_BUFFER_INFOS);
	}
	dfunc->vkCmdDrawIndexed (cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);
}

void
Vulkan_DrawAlias (entity_t *ent, vulkan_ctx_t *ctx)
{
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	model_t    *model = ent->renderer.model;
	aliashdr_t *hdr;
	qfv_alias_skin_t *skin;
	struct {
		mat4f_t     mat;
		float       blend;
	}           vertex_constants;
	byte        fragment_constants[3][4];

	if (!(hdr = model->aliashdr)) {
		hdr = Cache_Get (&model->cache);
	}

	Transform_GetWorldMatrix (ent->transform, vertex_constants.mat);
	vertex_constants.blend = R_AliasGetLerpedFrames (ent, hdr);

	if (0/*XXX ent->skin && ent->skin->tex*/) {
		//skin = ent->skin->tex;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (ent->renderer.skinnum, hdr);
		skin = (qfv_alias_skin_t *) ((byte *) hdr + skindesc->skin);
	}
	QuatScale (ent->renderer.colormod, 255, fragment_constants[0]);
	QuatCopy (skin->colora, fragment_constants[1]);
	QuatCopy (skin->colorb, fragment_constants[2]);

	emit_commands (aframe->cmdSet.a[QFV_aliasDepth],
				   ent->animation.pose1, ent->animation.pose2,
				   0, &vertex_constants, 17 * sizeof (float),
				   fragment_constants, sizeof (fragment_constants),
				   hdr, ctx);
	emit_commands (aframe->cmdSet.a[QFV_aliasGBuffer],
				   ent->animation.pose1, ent->animation.pose2,
				   skin, &vertex_constants, 17 * sizeof (float),
				   fragment_constants, sizeof (fragment_constants),
				   hdr, ctx);
}

static void
alias_begin_subpass (QFV_AliasSubpass subpass, VkPipeline pipeline,
					 vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = aframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		ctx->renderpass, subpass_map[subpass],
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	//VkDescriptorSet sets[] = {
	//	aframe->descriptors[0].dstSet,
	//	aframe->descriptors[1].dstSet,
	//};
	//dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//								actx->layout, 0, 2, sets, 0, 0);
	dfunc->vkCmdPushDescriptorSetKHR (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									  actx->layout,
									  0, 1, aframe->descriptors + 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &ctx->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &ctx->scissor);

	//dfunc->vkUpdateDescriptorSets (device->dev, 2, aframe->descriptors, 0, 0);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
alias_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_AliasBegin (vulkan_ctx_t *ctx)
{
	aliasctx_t *actx = ctx->alias_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	//XXX quat_t      fog;
	DARRAY_APPEND (&cframe->cmdSets[QFV_passDepth],
				   aframe->cmdSet.a[QFV_aliasDepth]);
	DARRAY_APPEND (&cframe->cmdSets[QFV_passGBuffer],
				   aframe->cmdSet.a[QFV_aliasGBuffer]);

	//FIXME need per frame matrices
	aframe->bufferInfo[0].buffer = ctx->matrices.buffer_3d;

	alias_begin_subpass (QFV_aliasDepth, actx->depth, ctx);
	alias_begin_subpass (QFV_aliasGBuffer, actx->gbuf, ctx);
}

void
Vulkan_AliasEnd (vulkan_ctx_t *ctx)
{
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	alias_end_subpass (aframe->cmdSet.a[QFV_aliasDepth], ctx);
	alias_end_subpass (aframe->cmdSet.a[QFV_aliasGBuffer], ctx);
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

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&actx->frames, frames);
	DARRAY_RESIZE (&actx->frames, frames);
	actx->frames.grow = 0;

	actx->depth = Vulkan_CreatePipeline (ctx, "alias_depth");
	actx->gbuf = Vulkan_CreatePipeline (ctx, "alias_gbuf");
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

	//__auto_type sets = QFV_AllocateDescriptorSet (device, pool, layouts);
	for (size_t i = 0; i < frames; i++) {
		__auto_type aframe = &actx->frames.a[i];

		DARRAY_INIT (&aframe->cmdSet, QFV_aliasNumPasses);
		DARRAY_RESIZE (&aframe->cmdSet, QFV_aliasNumPasses);
		aframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &aframe->cmdSet);

		for (int j = 0; j < QFV_aliasNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 aframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:alias:%zd:%s", i,
									 alias_pass_names[j]));
		}
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
Vulkan_Alias_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	for (size_t i = 0; i < actx->frames.size; i++) {
		__auto_type aframe = &actx->frames.a[i];
		free (aframe->cmdSet.a);
	}

	dfunc->vkDestroyPipeline (device->dev, actx->depth, 0);
	dfunc->vkDestroyPipeline (device->dev, actx->gbuf, 0);
	free (actx->frames.a);
	free (actx);
}
