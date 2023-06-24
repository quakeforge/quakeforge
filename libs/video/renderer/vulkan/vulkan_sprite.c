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
#include "QF/Vulkan/qf_sprite.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static void
emit_commands (VkCommandBuffer cmd, qfv_sprite_t *sprite,
			   int numPC, qfv_push_constants_t *constants,
			   qfv_taskctx_t *taskctx, entity_t ent)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	QFV_PushConstants (device, cmd, layout, numPC, constants);
	VkDescriptorSet sets[] = {
		sprite->descriptors,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 1, 1, sets, 0, 0);
	dfunc->vkCmdDraw (cmd, 4, 1, 0, 0);

	QFV_CmdEndLabel (device, cmd);
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

	if (!sctx->dsmanager) {
		sctx->dsmanager = QFV_Render_DSManager (ctx, "sprite_set");
	}
	sprite->descriptors = QFV_DSManager_AllocSet (sctx->dsmanager);

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
Vulkan_Sprite_FreeDescriptors (vulkan_ctx_t *ctx, qfv_sprite_t *sprite)
{
	spritectx_t *sctx = ctx->sprite_context;
	QFV_DSManager_FreeSet (sctx->dsmanager, sprite->descriptors);
}

static void
sprite_draw_ent (qfv_taskctx_t *taskctx, entity_t ent)
{
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	auto model = renderer->model;
	msprite_t *sprite = model->cache.data;

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

	emit_commands (taskctx->cmd,
				   (qfv_sprite_t *) ((byte *) sprite + sprite->data),
				   2, push_constants, taskctx, ent);
}

static void
sprite_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 1, sets, 0, 0);

	auto queue = r_ent_queue;	//FIXME fetch from scene
	for (size_t i = 0; i < queue->ent_queues[mod_sprite].size; i++) {
		entity_t    ent = queue->ent_queues[mod_sprite].a[i];
		sprite_draw_ent (taskctx, ent);
	}
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
	qfvPushDebug (ctx, "sprite init");
	QFV_Render_AddTasks (ctx, sprite_task_syms);

	spritectx_t *sctx = calloc (1, sizeof (spritectx_t));
	ctx->sprite_context = sctx;

	sctx->sampler = Vulkan_CreateSampler (ctx, "sprite_sampler");

	qfvPopDebug (ctx);
}

void
Vulkan_Sprite_Shutdown (vulkan_ctx_t *ctx)
{
	spritectx_t *sctx = ctx->sprite_context;

	free (sctx);
}
