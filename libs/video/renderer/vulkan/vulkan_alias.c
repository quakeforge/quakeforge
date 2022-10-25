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

#include <string.h>

#include "QF/cvar.h"
#include "QF/va.h"

#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	mat4f_t     mat;
	float       blend;
	byte        colorA[4];
	byte        colorB[4];
	vec4f_t     base_color;
	vec4f_t     fog;
} alias_push_constants_t;

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
			   uint32_t numPC, qfv_push_constants_t *constants,
			   aliashdr_t *hdr, qfv_renderframe_t *rFrame, entity_t ent)
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

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	dfunc->vkCmdBindVertexBuffers (cmd, 0, bindingCount, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, mesh->index_buffer, 0,
								 VK_INDEX_TYPE_UINT32);
	QFV_PushConstants (device, cmd, actx->layout, numPC, constants);
	if (skin) {
		VkDescriptorSet sets[] = {
			skin->descriptor,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										actx->layout, 1, 1, sets, 0, 0);
	}
	dfunc->vkCmdDrawIndexed (cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);

	QFV_CmdEndLabel (device, cmd);
}

void
Vulkan_DrawAlias (entity_t ent, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	aliasctx_t *actx = ctx->alias_context;
	aliasframe_t *aframe = &actx->frames.a[ctx->curFrame];
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	model_t    *model = renderer->model;
	aliashdr_t *hdr;
	qfv_alias_skin_t *skin;
	alias_push_constants_t constants = {};

	if (!(hdr = model->aliashdr)) {
		hdr = Cache_Get (&model->cache);
	}

	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   ent.reg);
	constants.blend = R_AliasGetLerpedFrames (animation, hdr);

	transform_t transform = Entity_Transform (ent);
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (alias_push_constants_t, mat),
			sizeof (mat4f_t), Transform_GetWorldMatrixPtr (transform) },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (alias_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (alias_push_constants_t, colorA),
			sizeof (constants.colorA), constants.colorA },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (alias_push_constants_t, colorB),
			sizeof (constants.colorB), constants.colorB },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (alias_push_constants_t, base_color),
			sizeof (constants.base_color), &constants.base_color },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (alias_push_constants_t, fog),
			sizeof (constants.fog), &constants.fog },
	};

	if (0/*XXX ent->skin && ent->skin->tex*/) {
		//skin = ent->skin->tex;
	} else {
		maliasskindesc_t *skindesc;
		skindesc = R_AliasGetSkindesc (animation, renderer->skinnum, hdr);
		skin = (qfv_alias_skin_t *) ((byte *) hdr + skindesc->skin);
	}
	QuatCopy (renderer->colormod, constants.base_color);
	QuatCopy (skin->colora, constants.colorA);
	QuatCopy (skin->colorb, constants.colorB);
	QuatZero (constants.fog);

	emit_commands (aframe->cmdSet.a[QFV_aliasDepth],
				   animation->pose1, animation->pose2,
				   0, 2, push_constants, hdr, rFrame, ent);
	emit_commands (aframe->cmdSet.a[QFV_aliasGBuffer],
				   animation->pose1, animation->pose2,
				   skin, 6, push_constants, hdr, rFrame, ent);
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
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									actx->layout, 0, 1, sets, 0, 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

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

	VkViewport  viewport = rFrame->renderpass->viewport;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	dfunc->vkCmdSetViewport (aframe->cmdSet.a[QFV_aliasDepth], 0, 1,
							 &viewport);
	dfunc->vkCmdSetViewport (aframe->cmdSet.a[QFV_aliasGBuffer], 0, 1,
							 &viewport);
}

void
Vulkan_AliasAddSkin (vulkan_ctx_t *ctx, qfv_alias_skin_t *skin)
{
	aliasctx_t *actx = ctx->alias_context;
	skin->descriptor = Vulkan_CreateCombinedImageSampler (ctx, skin->view,
														  actx->sampler);
}

void
Vulkan_AliasRemoveSkin (vulkan_ctx_t *ctx, qfv_alias_skin_t *skin)
{
	Vulkan_FreeTexture (ctx, skin->descriptor);
	skin->descriptor = 0;
}

void
Vulkan_Alias_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

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
