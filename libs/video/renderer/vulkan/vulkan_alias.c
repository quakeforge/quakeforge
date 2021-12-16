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
#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/renderpass.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static const char * __attribute__((used)) alias_pass_names[] = {
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
			   aliashdr_t *hdr, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

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
	}
	dfunc->vkCmdDrawIndexed (cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);
}

void
Vulkan_DrawAlias (entity_t *ent, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	model_t    *model = ent->renderer.model;
	aliashdr_t *hdr;
	qfv_alias_skin_t *skin;
	animation_t *animation = &ent->animation;
	struct {
		mat4f_t     mat;
		float       blend;
	}           vertex_constants;
	struct {
		uint32_t    texind;
		byte        colorA[4];
		byte        colorB[4];
		float       base_color[4];
		float       fog[4];
	}           fragment_constants;

	if (!(hdr = model->aliashdr)) {
		hdr = Cache_Get (&model->cache);
	}

	Transform_GetWorldMatrix (ent->transform, vertex_constants.mat);
	vertex_constants.blend = R_AliasGetLerpedFrames (animation, hdr);

	if (0/*XXX ent->skin && ent->skin->tex*/) {
		//skin = ent->skin->tex;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (animation, ent->renderer.skinnum, hdr);
		skin = (qfv_alias_skin_t *) ((byte *) hdr + skindesc->skin);
	}
	fragment_constants.texind = skin->texind;
	QuatCopy (ent->renderer.colormod, fragment_constants.base_color);
	QuatCopy (skin->colora, fragment_constants.colorA);
	QuatCopy (skin->colorb, fragment_constants.colorB);
	QuatZero (fragment_constants.fog);

	emit_commands (aframe->cmdSet.a[QFV_aliasDepth],
				   ent->animation.pose1, ent->animation.pose2,
				   0, &vertex_constants, 17 * sizeof (float),
				   &fragment_constants, sizeof (fragment_constants),
				   hdr, rFrame);
	emit_commands (aframe->cmdSet.a[QFV_aliasGBuffer],
				   ent->animation.pose1, ent->animation.pose2,
				   skin, &vertex_constants, 17 * sizeof (float),
				   &fragment_constants, sizeof (fragment_constants),
				   hdr, rFrame);
}

static void
alias_begin_subpass (QFV_AliasSubpass subpass, VkPipeline pipeline,
					 qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = aframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, subpass_map[subpass],
		cframe->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "alias:%s",
										  alias_pass_names[subpass]),
						 { 0.6, 0.5, 0, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		actx->descriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									actx->layout, 0, 2, sets, 0, 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &ctx->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &ctx->scissor);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
alias_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_AliasBegin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	//XXX quat_t      fog;
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passDepth],
				   aframe->cmdSet.a[QFV_aliasDepth]);
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passGBuffer],
				   aframe->cmdSet.a[QFV_aliasGBuffer]);

	alias_begin_subpass (QFV_aliasDepth, actx->depth, rFrame);
	alias_begin_subpass (QFV_aliasGBuffer, actx->gbuf, rFrame);
}

void
Vulkan_AliasEnd (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	alias_end_subpass (aframe->cmdSet.a[QFV_aliasDepth], ctx);
	alias_end_subpass (aframe->cmdSet.a[QFV_aliasGBuffer], ctx);
}

void
Vulkan_AliasDepthRange (qfv_renderframe_t *rFrame,
						float minDepth, float maxDepth)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];

	VkViewport  viewport = ctx->viewport;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	dfunc->vkCmdSetViewport (aframe->cmdSet.a[QFV_aliasDepth], 0, 1,
							 &viewport);
	dfunc->vkCmdSetViewport (aframe->cmdSet.a[QFV_aliasGBuffer], 0, 1,
							 &viewport);
}

static VkDescriptorImageInfo base_sampler_info = { };
static VkDescriptorImageInfo base_image_info = {
	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_sampler_write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstBinding = 0,
	.descriptorCount = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
};
static VkWriteDescriptorSet base_image_write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstBinding = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
};

void
Vulkan_AliasAddSkin (vulkan_ctx_t *ctx, qfv_alias_skin_t *skin)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	if (!actx->texindices.size) {
		Sys_Error ("ran out of skins (smart texture handling not implemented)");
	}
	skin->texind = DARRAY_REMOVE (&actx->texindices);

	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0] = base_image_info;
	imageInfo[0].imageView = skin->view;

	VkWriteDescriptorSet write[2];
	write[0] = base_image_write;
	write[0].dstSet = actx->descriptors;
	write[0].dstArrayElement = skin->texind;
	write[0].descriptorCount = 1;
	write[0].pImageInfo = imageInfo;
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
}

void
Vulkan_AliasRemoveSkin (vulkan_ctx_t *ctx, qfv_alias_skin_t *skin)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	DARRAY_APPEND (&actx->texindices, skin->texind);

	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0] = base_image_info;
	imageInfo[0].imageView = ctx->default_magenta_array->view;

	VkWriteDescriptorSet write[2];
	write[0] = base_image_write;
	write[0].dstSet = actx->descriptors;
	write[0].dstArrayElement = skin->texind;
	write[0].descriptorCount = 1;
	write[0].pImageInfo = imageInfo;
	dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
}

void
Vulkan_Alias_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	qfvPushDebug (ctx, "alias init");

	aliasctx_t *actx = calloc (1, sizeof (aliasctx_t));
	ctx->alias_context = actx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&actx->frames, frames);
	DARRAY_RESIZE (&actx->frames, frames);
	actx->frames.grow = 0;

	actx->depth = Vulkan_CreateGraphicsPipeline (ctx, "alias_depth");
	actx->gbuf = Vulkan_CreateGraphicsPipeline (ctx, "alias_gbuf");
	actx->layout = Vulkan_CreatePipelineLayout (ctx, "alias_layout");
	actx->sampler = Vulkan_CreateSampler (ctx, "alias_sampler");

	//FIXME too many places
	__auto_type limits = device->physDev->properties.limits;
	actx->maxImages = min (256, limits.maxPerStageDescriptorSampledImages - 8);
	actx->pool = Vulkan_CreateDescriptorPool (ctx, "alias_pool");
	actx->setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "alias_set");
	//FIXME kinda dumb
	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (1, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = actx->setLayout;
	}
	__auto_type sets = QFV_AllocateDescriptorSet (device, actx->pool, layouts);
	actx->descriptors = sets->a[0];
	free (sets);

	DARRAY_INIT (&actx->texindices, actx->maxImages);
	DARRAY_RESIZE (&actx->texindices, actx->maxImages);
	actx->texindices.grow = 0;
	actx->texindices.maxSize = actx->maxImages;
	for (unsigned i = 0; i < actx->maxImages; i++) {
		actx->texindices.a[i] = i;
	}

	VkDescriptorImageInfo samplerInfo[1];
	samplerInfo[0] = base_sampler_info;
	samplerInfo[0].sampler = actx->sampler;

	VkDescriptorImageInfo imageInfo[actx->maxImages];
	for (unsigned i = 0; i < actx->maxImages; i++) {
		imageInfo[i] = base_image_info;
		imageInfo[i].imageView = ctx->default_magenta_array->view;
	}
	VkWriteDescriptorSet write[2];
	write[0] = base_sampler_write;
	write[0].dstSet = actx->descriptors;
	write[0].pImageInfo = samplerInfo;
	write[1] = base_image_write;
	write[1].dstSet = actx->descriptors;
	write[1].descriptorCount = actx->maxImages;
	write[1].pImageInfo = imageInfo;
	dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);



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
	}
	qfvPopDebug (ctx);
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
