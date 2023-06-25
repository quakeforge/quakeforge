/*
	vulkan_iqm.c

	Vulkan IQM model pipeline

	Copyright (C) 2022       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/5/3

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
#include "QF/iqm.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_iqm.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	mat4f_t     mat;
	float       blend;
	byte        colorA[4];
	byte        colorB[4];
	vec4f_t     base_color;
	vec4f_t     fog;
} iqm_push_constants_t;

static void
emit_commands (VkCommandBuffer cmd, int pose1, int pose2,
			   qfv_iqm_skin_t *skins,
			   uint32_t numPC, qfv_push_constants_t *constants,
			   iqm_t *iqm, qfv_taskctx_t *taskctx, entity_t ent)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;

	auto mesh = (qfv_iqm_t *) iqm->extra_data;

	VkDeviceSize offsets[] = { 0, 0, };
	VkBuffer    buffers[] = {
		mesh->geom_buffer,
		mesh->rend_buffer,
	};
	int         bindingCount = 2;//skins ? 2 : 1;

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	dfunc->vkCmdBindVertexBuffers (cmd, 0, bindingCount, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, mesh->index_buffer, 0,
								 VK_INDEX_TYPE_UINT16);
	QFV_PushConstants (device, cmd, layout, numPC, constants);
	for (int i = 0; i < iqm->num_meshes; i++) {
		if (skins) {
			VkDescriptorSet sets[] = {
				skins[i].descriptor,
				mesh->bones_descriptors[ctx->curFrame],
			};
			dfunc->vkCmdBindDescriptorSets (cmd,
											VK_PIPELINE_BIND_POINT_GRAPHICS,
											layout, 1, 2, sets, 0, 0);
		} else {
			VkDescriptorSet sets[] = {
				mesh->bones_descriptors[ctx->curFrame],
			};
			dfunc->vkCmdBindDescriptorSets (cmd,
											VK_PIPELINE_BIND_POINT_GRAPHICS,
											layout, 2, 1, sets, 0, 0);
		}
		dfunc->vkCmdDrawIndexed (cmd, 3 * iqm->meshes[i].num_triangles, 1,
								 3 * iqm->meshes[i].first_triangle, 0, 0);
	}

	QFV_CmdEndLabel (device, cmd);
}

static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	0, 0, 0
};

void
Vulkan_IQMAddBones (vulkan_ctx_t *ctx, iqm_t *iqm)
{
	qfvPushDebug (ctx, "Vulkan_IQMAddBones");

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto ictx = ctx->iqm_context;
	auto mesh = (qfv_iqm_t *) iqm->extra_data;
	int  num_sets = ictx->frames.size;

	if (!ictx->dsmanager) {
		ictx->dsmanager = QFV_Render_DSManager (ctx, "bone_set");
	}

	for (int i = 0; i < num_sets; i++) {
		auto set = QFV_DSManager_AllocSet (ictx->dsmanager);
		mesh->bones_descriptors[i] = set;
	}

	VkDescriptorBufferInfo bufferInfo[num_sets];
	size_t      bones_size = sizeof (vec4f_t[iqm->num_joints * 3]);
	for (int i = 0; i < num_sets; i++) {
		bufferInfo[i].buffer = mesh->bones_buffer;
		bufferInfo[i].offset = i * bones_size;
		bufferInfo[i].range = bones_size;
	};
	VkWriteDescriptorSet write[num_sets];
	for (int i = 0; i < num_sets; i++) {
		write[i] = base_buffer_write;
		write[i].dstSet = mesh->bones_descriptors[i];
		write[i].pBufferInfo = &bufferInfo[i];
	}
	dfunc->vkUpdateDescriptorSets (device->dev, num_sets, write, 0, 0);

	qfvPopDebug (ctx);
}

void
Vulkan_IQMRemoveBones (vulkan_ctx_t *ctx, iqm_t *iqm)
{
	auto ictx = ctx->iqm_context;
	auto mesh = (qfv_iqm_t *) iqm->extra_data;
	int  num_sets = ictx->frames.size;

	for (int i = 0; i < num_sets; i++) {
		QFV_DSManager_FreeSet (ictx->dsmanager, mesh->bones_descriptors[i]);
	}
}

void
Vulkan_IQMAddSkin (vulkan_ctx_t *ctx, qfv_iqm_skin_t *skin)
{
	iqmctx_t   *ictx = ctx->iqm_context;
	skin->descriptor = Vulkan_CreateCombinedImageSampler (ctx, skin->view,
														  ictx->sampler);
}

void
Vulkan_IQMRemoveSkin (vulkan_ctx_t *ctx, qfv_iqm_skin_t *skin)
{
	Vulkan_FreeTexture (ctx, skin->descriptor);
	skin->descriptor = 0;
}

static void
iqm_draw_ent (qfv_taskctx_t *taskctx, entity_t ent, bool pass)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	auto model = renderer->model;
	auto iqm = (iqm_t *) model->aliashdr;
	qfv_iqm_t  *mesh = iqm->extra_data;
	auto skins = mesh->skins;
	iqm_push_constants_t constants = {};
	iqmframe_t *frame;

	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   ent.reg);
	constants.blend = R_IQMGetLerpedFrames (animation, iqm);
	frame = R_IQMBlendFrames (iqm, animation->pose1, animation->pose2,
							  constants.blend, 0);

	vec4f_t    *bone_data;
	dfunc->vkMapMemory (device->dev, mesh->bones->memory, 0, VK_WHOLE_SIZE,
						0, (void **)&bone_data);
	for (int i = 0; i < iqm->num_joints; i++) {
		vec4f_t    *b = bone_data + (ctx->curFrame * iqm->num_joints + i) * 3;
		mat4f_t     f;
		// R_IQMBlendFrames sets up the frame as a 4x4 matrix for m * v, but
		// the shader wants a 3x4 (column x row) matrix for v * m, which is
		// just a transpose (and drop of the 4th column) away.
		mat4ftranspose (f, (vec4f_t *) &frame[i]);
		// copy only the first 3 columns
		memcpy (b, f, 3 * sizeof (vec4f_t));
	}
#define a(x) ((x) & ~0x3f)
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		mesh->bones->memory,
		a(3 * ctx->curFrame * iqm->num_joints * sizeof (vec4f_t)),
		a(3 * iqm->num_joints * sizeof (vec4f_t) + 0x3f),
	};
#undef a
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
	dfunc->vkUnmapMemory (device->dev, mesh->bones->memory);

	transform_t transform = Entity_Transform (ent);
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (iqm_push_constants_t, mat),
			sizeof (mat4f_t), Transform_GetWorldMatrixPtr (transform) },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (iqm_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (iqm_push_constants_t, colorA),
			sizeof (constants.colorA), constants.colorA },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (iqm_push_constants_t, colorB),
			sizeof (constants.colorB), constants.colorB },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (iqm_push_constants_t, base_color),
			sizeof (constants.base_color), &constants.base_color },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (iqm_push_constants_t, fog),
			sizeof (constants.fog), &constants.fog },
	};

	QuatCopy (renderer->colormod, constants.base_color);
	QuatCopy (skins[0].colora, constants.colorA);
	QuatCopy (skins[0].colorb, constants.colorB);
	QuatZero (constants.fog);

	emit_commands (taskctx->cmd, animation->pose1, animation->pose2,
				   pass ? skins : 0,
				   pass ? 6 : 2, push_constants,
				   iqm, taskctx, ent);
}

static void
iqm_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	int  pass = *(int *) params[0]->value;

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
	for (size_t i = 0; i < queue->ent_queues[mod_iqm].size; i++) {
		entity_t    ent = queue->ent_queues[mod_iqm].a[i];
		iqm_draw_ent (taskctx, ent, pass);
	}
}

static exprtype_t *iqm_draw_params[] = {
	&cexpr_int,
};
static exprfunc_t iqm_draw_func[] = {
	{ .func = iqm_draw, .num_params = 1, .param_types = iqm_draw_params },
	{}
};
static exprsym_t iqm_task_syms[] = {
	{ "iqm_draw", &cexpr_function, iqm_draw_func },
	{}
};

void
Vulkan_IQM_Init (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "iqm init");
	QFV_Render_AddTasks (ctx, iqm_task_syms);

	iqmctx_t   *ictx = calloc (1, sizeof (iqmctx_t));
	ctx->iqm_context = ictx;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&ictx->frames, frames);
	DARRAY_RESIZE (&ictx->frames, frames);
	ictx->frames.grow = 0;

	qfvPopDebug (ctx);
}

void
Vulkan_IQM_Setup (vulkan_ctx_t *ctx)
{
	auto ictx = ctx->iqm_context;
	ictx->sampler = QFV_Render_Sampler (ctx, "alias_sampler");
}

void
Vulkan_IQM_Shutdown (vulkan_ctx_t *ctx)
{
	iqmctx_t   *ictx = ctx->iqm_context;

	free (ictx->frames.a);
	free (ictx);
}
