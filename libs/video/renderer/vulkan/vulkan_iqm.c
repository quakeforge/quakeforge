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

#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "QF/Vulkan/qf_iqm.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"

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

static const char * __attribute__((used)) iqm_pass_names[] = {
	"depth",
	"g-buffer",
	"translucent",
};

static QFV_Subpass subpass_map[] = {
	QFV_passDepth,			// QFV_iqmDepth
	QFV_passGBuffer,		// QFV_iqmGBuffer
	QFV_passTranslucent,	// QFV_iqmTranslucent
};

static void
emit_commands (VkCommandBuffer cmd, int pose1, int pose2,
			   qfv_iqm_skin_t *skins,
			   uint32_t numPC, qfv_push_constants_t *constants,
			   iqm_t *iqm, qfv_renderframe_t *rFrame, entity_t ent)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t *ictx = ctx->iqm_context;

	__auto_type mesh = (qfv_iqm_t *) iqm->extra_data;

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
	QFV_PushConstants (device, cmd, ictx->layout, numPC, constants);
	for (int i = 0; i < iqm->num_meshes; i++) {
		if (skins) {
			VkDescriptorSet sets[] = {
				skins[i].descriptor,
				mesh->bones_descriptors[ctx->curFrame],
			};
			dfunc->vkCmdBindDescriptorSets (cmd,
											VK_PIPELINE_BIND_POINT_GRAPHICS,
											ictx->layout, 1, 2, sets, 0, 0);
		} else {
			VkDescriptorSet sets[] = {
				mesh->bones_descriptors[ctx->curFrame],
			};
			dfunc->vkCmdBindDescriptorSets (cmd,
											VK_PIPELINE_BIND_POINT_GRAPHICS,
											ictx->layout, 2, 1, sets, 0, 0);
		}
		dfunc->vkCmdDrawIndexed (cmd, 3 * iqm->meshes[i].num_triangles, 1,
								 3 * iqm->meshes[i].first_triangle, 0, 0);
	}

	QFV_CmdEndLabel (device, cmd);
}

#define a(x) ((x) & ~0x3f)

void
Vulkan_DrawIQM (entity_t ent, qfv_renderframe_t *rFrame)
{
	transform_t transform = Entity_Transform (ent);
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer,
											 r_refdef.scene->reg);
	animation_t *animation = Ent_GetComponent (ent.id, scene_animation,
											   r_refdef.scene->reg);
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;
	iqm_frame_t *aframe = &ictx->frames.a[ctx->curFrame];
	model_t    *model = renderer->model;
	iqm_t      *iqm = (iqm_t *) model->aliashdr;
	qfv_iqm_t  *mesh = iqm->extra_data;
	qfv_iqm_skin_t *skins = mesh->skins;
	iqm_push_constants_t constants = {};
	iqmframe_t *frame;

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
	VkMappedMemoryRange range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
		mesh->bones->memory,
		a(3 * ctx->curFrame * iqm->num_joints * sizeof (vec4f_t)),
		a(3 * iqm->num_joints * sizeof (vec4f_t) + 0x3f),
	};
	dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
	dfunc->vkUnmapMemory (device->dev, mesh->bones->memory);


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

	emit_commands (aframe->cmdSet.a[QFV_iqmDepth],
				   animation->pose1, animation->pose2,
				   0, 2, push_constants, iqm, rFrame, ent);
	emit_commands (aframe->cmdSet.a[QFV_iqmGBuffer],
				   animation->pose1, animation->pose2,
				   skins, 6, push_constants, iqm, rFrame, ent);
}

static void
iqm_begin_subpass (QFV_IQMSubpass subpass, VkPipeline pipeline,
				   qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;
	__auto_type cframe = &ctx->frames.a[ctx->curFrame];
	iqm_frame_t *aframe = &ictx->frames.a[ctx->curFrame];
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

	QFV_duCmdBeginLabel (device, cmd, va (ctx->va_ctx, "iqm:%s",
										  iqm_pass_names[subpass]),
						 { 0.6, 0.5, 0, 1});

	dfunc->vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									ictx->layout, 0, 1, sets, 0, 0);
	dfunc->vkCmdSetViewport (cmd, 0, 1, &rFrame->renderpass->viewport);
	dfunc->vkCmdSetScissor (cmd, 0, 1, &rFrame->renderpass->scissor);

	//XXX glsl_Fog_GetColor (fog);
	//XXX fog[3] = glsl_Fog_GetDensity () / 64.0;
}

static void
iqm_end_subpass (VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	QFV_duCmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);
}

void
Vulkan_IQMBegin (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	iqmctx_t   *ictx = ctx->iqm_context;
	iqm_frame_t *aframe = &ictx->frames.a[ctx->curFrame];

	//XXX quat_t      fog;
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passDepth],
				   aframe->cmdSet.a[QFV_iqmDepth]);
	DARRAY_APPEND (&rFrame->subpassCmdSets[QFV_passGBuffer],
				   aframe->cmdSet.a[QFV_iqmGBuffer]);

	iqm_begin_subpass (QFV_iqmDepth, ictx->depth, rFrame);
	iqm_begin_subpass (QFV_iqmGBuffer, ictx->gbuf, rFrame);
}

void
Vulkan_IQMEnd (qfv_renderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	iqmctx_t   *ictx = ctx->iqm_context;
	iqm_frame_t *aframe = &ictx->frames.a[ctx->curFrame];

	iqm_end_subpass (aframe->cmdSet.a[QFV_iqmDepth], ctx);
	iqm_end_subpass (aframe->cmdSet.a[QFV_iqmGBuffer], ctx);
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

	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;
	__auto_type mesh = (qfv_iqm_t *) iqm->extra_data;
	int         num_sets = ictx->frames.size;

	//FIXME kinda dumb
	__auto_type layouts = QFV_AllocDescriptorSetLayoutSet (num_sets, alloca);
	for (size_t i = 0; i < layouts->size; i++) {
		layouts->a[i] = ictx->bones_setLayout;
	}
	__auto_type sets = QFV_AllocateDescriptorSet (device, ictx->bones_pool,
												  layouts);
	for (size_t i = 0; i < sets->size; i++) {
		mesh->bones_descriptors[i] = sets->a[i];
	}
	free (sets);

	VkDescriptorBufferInfo bufferInfo[num_sets];
	size_t      bones_size = iqm->num_joints * 3 * sizeof (vec4f_t);
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
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;
	__auto_type mesh = (qfv_iqm_t *) iqm->extra_data;
	int         num_sets = ictx->frames.size;

	dfunc->vkFreeDescriptorSets (device->dev, ictx->bones_pool, num_sets,
								 mesh->bones_descriptors);
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

void
Vulkan_IQM_Init (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;

	qfvPushDebug (ctx, "iqm init");

	iqmctx_t   *ictx = calloc (1, sizeof (iqmctx_t));
	ctx->iqm_context = ictx;

	size_t      frames = ctx->frames.size;
	DARRAY_INIT (&ictx->frames, frames);
	DARRAY_RESIZE (&ictx->frames, frames);
	ictx->frames.grow = 0;

	ictx->depth = Vulkan_CreateGraphicsPipeline (ctx, "iqm_depth");
	ictx->gbuf = Vulkan_CreateGraphicsPipeline (ctx, "iqm_gbuf");
	ictx->layout = Vulkan_CreatePipelineLayout (ctx, "iqm_layout");
	ictx->sampler = Vulkan_CreateSampler (ctx, "alias_sampler");

	ictx->bones_pool = Vulkan_CreateDescriptorPool (ctx, "bone_pool");
	ictx->bones_setLayout = Vulkan_CreateDescriptorSetLayout (ctx, "bone_set");

	for (size_t i = 0; i < frames; i++) {
		__auto_type aframe = &ictx->frames.a[i];

		DARRAY_INIT (&aframe->cmdSet, QFV_iqmNumPasses);
		DARRAY_RESIZE (&aframe->cmdSet, QFV_iqmNumPasses);
		aframe->cmdSet.grow = 0;

		QFV_AllocateCommandBuffers (device, ctx->cmdpool, 1, &aframe->cmdSet);

		for (int j = 0; j < QFV_iqmNumPasses; j++) {
			QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
								 aframe->cmdSet.a[j],
								 va (ctx->va_ctx, "cmd:iqm:%zd:%s", i,
									 iqm_pass_names[j]));
		}
	}
	qfvPopDebug (ctx);
}

void
Vulkan_IQM_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	iqmctx_t   *ictx = ctx->iqm_context;

	for (size_t i = 0; i < ictx->frames.size; i++) {
		__auto_type aframe = &ictx->frames.a[i];
		free (aframe->cmdSet.a);
	}

	dfunc->vkDestroyPipeline (device->dev, ictx->depth, 0);
	dfunc->vkDestroyPipeline (device->dev, ictx->gbuf, 0);
	free (ictx->frames.a);
	free (ictx);
}
