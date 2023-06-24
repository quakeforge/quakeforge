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

#include "QF/scene/entity.h"

#include "QF/Vulkan/qf_alias.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_palette.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	mat4f_t     mat;
	float       blend;
	byte        colors[4];
	vec4f_t     base_color;
	vec4f_t     fog;
} alias_push_constants_t;

static void
emit_commands (VkCommandBuffer cmd, int pose1, int pose2,
			   qfv_alias_skin_t *skin,
			   uint32_t numPC, qfv_push_constants_t *constants,
			   aliashdr_t *hdr, qfv_taskctx_t *taskctx, entity_t ent)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;

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
	QFV_PushConstants (device, cmd, layout, numPC, constants);
	if (skin) {
		VkDescriptorSet sets[] = {
			skin->descriptor,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 2, 1, sets, 0, 0);
	}
	dfunc->vkCmdDrawIndexed (cmd, 3 * hdr->mdl.numtris, 1, 0, 0, 0);

	QFV_CmdEndLabel (device, cmd);
}

static void
alias_depth_range (qfv_taskctx_t *taskctx, float minDepth, float maxDepth)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto viewport = taskctx->pipeline->viewport;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	dfunc->vkCmdSetViewport (taskctx->cmd, 0, 1, &viewport);
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

static void
alias_draw_ent (qfv_taskctx_t *taskctx, entity_t ent, bool pass)
{
	renderer_t *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	auto model = renderer->model;
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
			field_offset (alias_push_constants_t, colors),
			sizeof (constants.colors), constants.colors },
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
	QuatCopy (skin->colors, constants.colors);
	if (Ent_HasComponent (ent.id, scene_colormap, ent.reg)) {
		colormap_t *colormap=Ent_GetComponent (ent.id, scene_colormap, ent.reg);
		constants.colors[0] = colormap->top * 16 + 8;
		constants.colors[1] = colormap->bottom * 16 + 8;
	}
	QuatZero (constants.fog);

	emit_commands (taskctx->cmd, animation->pose1, animation->pose2,
				   pass ? skin : 0,
				   pass ? 5 : 2, push_constants,
				   hdr, taskctx, ent);
}

static void
alias_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	bool vmod = Entity_Valid (vr_data.view_model);
	int  pass = *(int *) params[0]->value;

	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;
	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		Vulkan_Palette_Descriptor (ctx),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, sets, 0, 0);

	auto queue = r_ent_queue;	//FIXME fetch from scene
	for (size_t i = 0; i < queue->ent_queues[mod_alias].size; i++) {
		entity_t    ent = queue->ent_queues[mod_alias].a[i];
		// FIXME hack the depth range to prevent view model
		// from poking into walls
		if (vmod && ent.id == vr_data.view_model.id) {
			alias_depth_range (taskctx, 0, 0.3);
		}
		alias_draw_ent (taskctx, ent, pass);
		// unhack in case the view_model is not the last
		if (vmod && ent.id == vr_data.view_model.id) {
			alias_depth_range (taskctx, 0, 1);
		}
	}
}

static exprtype_t *alias_draw_params[] = {
	&cexpr_int,
};
static exprfunc_t alias_draw_func[] = {
	{ .func = alias_draw, .num_params = 1, .param_types = alias_draw_params },
	{}
};
static exprsym_t alias_task_syms[] = {
	{ "alias_draw", &cexpr_function, alias_draw_func },
	{}
};

void
Vulkan_Alias_Init (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "alias init");
	QFV_Render_AddTasks (ctx, alias_task_syms);

	aliasctx_t *actx = calloc (1, sizeof (aliasctx_t));
	ctx->alias_context = actx;

	actx->sampler = Vulkan_CreateSampler (ctx, "alias_sampler");
	qfvPopDebug (ctx);
}

void
Vulkan_Alias_Shutdown (vulkan_ctx_t *ctx)
{
	//qfv_device_t *device = ctx->device;
	//qfv_devfuncs_t *dfunc = device->funcs;
	aliasctx_t *actx = ctx->alias_context;

	free (actx);
}
