/*
	vulkan_sprite.c

	Vulkan sprite model pipeline

	Copyright (C) 2012       Bill Currie <bill@taniwha.org>
	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
	Date: 2021/12/14

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

#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_sprite.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static const char * __attribute__((used)) sprite_pass_names[] = {
	"depth",
	"g-buffer",
	"translucent",
};

static QFV_Subpass subpass_map[] = {
	QFV_passDepth,			// QFV_spriteDepth
	QFV_passGBuffer,		// QFV_spriteGBuffer
	QFV_passTranslucentFrag,// QFV_spriteTranslucent
};

static void
emit_commands (VkCommandBuffer cmd, qfv_sprite_t *sprite,
			   int numPC, qfv_push_constants_t *constants,
			   qfv_renderframe_t *rFrame, entity_t ent)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	spritectx_t *sctx = ctx->sprite_context;

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	QFV_PushConstants (device, cmd, sctx->layout, numPC, constants);
	VkDescriptorSet sets[] = {
		sprite->descriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									sctx->layout, 1, 1, sets, 0, 0);
	dfunc->vkCmdDraw (cmd, 4, 1, 0, 0);

	QFV_CmdEndLabel (device, cmd);
}

void
Vulkan_DrawSprite (entity_t ent, qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	spritectx_t *sctx = ctx->sprite_context;
	spriteframe_t *sframe = &sctx->frames.a[ctx->curFrame];
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	model_t    *model = renderer->model;
	msprite_t  *sprite = model->cache.data;

	mat4f_t     mat = {};
	uint32_t    frame;
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (mat), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			64, sizeof (frame), &frame },
	};

	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   ent.reg);
	frame = (ptrdiff_t) R_GetSpriteFrame (sprite, animation);

	transform_t transform = Entity_Transform (ent);
	mat[3] = Transform_GetWorldPosition (transform);
	vec4f_t     cameravec = r_refdef.frame.position - mat[3];
	R_BillboardFrame (transform, sprite->type, cameravec,
					  &mat[2], &mat[1], &mat[0]);
	mat[0] = -mat[0];

	emit_commands (sframe->cmdSet.a[QFV_spriteDepth],
				   (qfv_sprite_t *) ((byte *) sprite + sprite->data),
				   2, push_constants, rFrame, ent);
	emit_commands (sframe->cmdSet.a[QFV_spriteGBuffer],
				   (qfv_sprite_t *) ((byte *) sprite + sprite->data),
				   2, push_constants, rFrame, ent);
}

static void
sprite_begin_subpass (QFV_SpriteSubpass subpass, VkPipeline pipeline,
					 qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	spritectx_t *sctx = ctx->sprite_context;
	spriteframe_t *sframe = &sctx->frames.a[ctx->curFrame];
	VkCommandBuffer cmd = sframe->cmdSet.a[subpass];

	dfunc->vkResetCommandBuffer (cmd, 0);
	VkCommandBufferInheritanceInfo inherit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, 0,
		rFrame->renderpass->renderpass, subpass_map[subpass],
		rFrame->framebuffer,
		0, 0, 0,
	};
	VkCommandBufferBeginInfo beginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "sprite:%s",
										  sprite_pass_names[subpass]),
						 { 0.6, 0.5, 0, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									sctx->layout, 0, 1, sets, 0, 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
sprite_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_SpriteBegin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	spritectx_t *sctx = ctx->sprite_context;
	spriteframe_t *sframe = &sctx->frames.a[ctx->curFrame];

	//XXX quat_t      fog;
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passDepth],
				   sframe->cmdSet.a[QFV_spriteDepth]);
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passGBuffer],
				   sframe->cmdSet.a[QFV_spriteGBuffer]);

	sprite_begin_subpass (QFV_spriteDepth, sctx->depth, rFrame);
	sprite_begin_subpass (QFV_spriteGBuffer, sctx->gbuf, rFrame);
}

void
Vulkan_SpriteEnd (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	spritectx_t *sctx = ctx->sprite_context;
	spriteframe_t *sframe = &sctx->frames.a[ctx->curFrame];

	sprite_end_subpass (sframe->cmdSet.a[QFV_spriteDepth], ctx);
	sprite_end_subpass (sframe->cmdSet.a[QFV_spriteGBuffer], ctx);
}

static VkDescriptorBufferInfo base_buffer_info = {
	.range = VK_WHOLE_SIZE,
};
static VkDescriptorImageInfo base_image_info = {
	.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
static VkWriteDescriptorSet base_buffer_write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstBinding = 0,
	.descriptorCount = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
};
static VkWriteDescriptorSet base_image_write = {
	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	.dstBinding = 1,
	.descriptorCount = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
};

void
Vulkan_Sprite_DescriptorSet (vulkan_ctx_t *ctx, qfv_sprite_t *sprite)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	spritectx_t *sctx = ctx->sprite_context;

	//FIXME kinda dumb
	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (1, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = sctx->setLayout;
	}
	__auto_type sets = QFV_AllocateDescriptorSet (device, sctx->pool, layouts);
	sprite->descriptors = sets->a[0];
	free (sets);

	VkDescriptorBufferInfo bufferInfo[1];
	bufferInfo[0] = base_buffer_info;
	bufferInfo[0].buffer = sprite->verts;

	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0] = base_image_info;
	imageInfo[0].sampler = sctx->sampler;
	imageInfo[0].imageView = sprite->view;

	VkWriteDescriptorSet write[2];
	write[0] = base_buffer_write;
	write[0].dstSet = sprite->descriptors;
	write[0].pBufferInfo = bufferInfo;
	write[1] = base_image_write;
	write[1].dstSet = sprite->descriptors;
	write[1].pImageInfo = imageInfo;
	dfunc->vkUpdateDescriptorSets (device->dev, 2, write, 0, 0);
}

void
Vulkan_Sprint_FreeDescriptors (vulkan_ctx_t *ctx, qfv_sprite_t *sprite)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	spritectx_t *sctx = ctx->sprite_context;

	dfunc->vkFreeDescriptorSets (device->dev, sctx->pool, 1,
								 &sprite->descriptors);
}

static void
sprite_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
}

static exprfunc_t sprite_draw_func[] = {
	{ .func = sprite_draw },
	{}
};
static exprsym_t sprite_task_syms[] = {
	{ "sprite_draw", &cexpr_function, sprite_draw_func },
	{}
};

void
Vulkan_Sprite_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "sprite init");
	QFV_Render_AddTasks (ctx, sprite_task_syms);

	spritectx_t *sctx = calloc (1, sizeof (spritectx_t));
	ctx->sprite_context = sctx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&sctx->frames, frames);
	DARRAY_RESIZE (&sctx->frames, frames);
	sctx->frames.grow = 0;

	sctx->depth = Vulkan_CreateGraphicsPipeline (ctx, "sprite_depth");
	sctx->gbuf = Vulkan_CreateGraphicsPipeline (ctx, "sprite_gbuf");
	sctx->layout = Vulkan_CreatePipelineLayout (ctx, "sprite_layout");
	sctx->sampler = Vulkan_CreateSampler (ctx, "sprite_sampler");

	sctx->pool = Vulkan_CreateDescriptorPool (ctx, "sprite_pool");
	sctx->setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "sprite_set");

	for (size_t i = 0; i < frames; i++) {
		__auto_type sframe = &sctx->frames.a[i];

		DARRAY_INIT (&sframe->cmdSet, QFV_spriteNumPasses);
		DARRAY_RESIZE (&sframe->cmdSet, QFV_spriteNumPasses);
		sframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &sframe->cmdSet);

		for (int j = 0; j < QFV_spriteNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 sframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:sprite:%zd:%s", i,
									 sprite_pass_names[j]));
		}
	}
	qfvPopDebug (ctx);
}

void
Vulkan_Sprite_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	spritectx_t *sctx = ctx->sprite_context;

	for (size_t i = 0; i < sctx->frames.size; i++) {
		__auto_type sframe = &sctx->frames.a[i];
		free (sframe->cmdSet.a);
	}

	dfunc->vkDestroyPipeline (device->dev, sctx->depth, 0);
	dfunc->vkDestroyPipeline (device->dev, sctx->gbuf, 0);
	free (sctx->frames.a);
	free (sctx);
}
