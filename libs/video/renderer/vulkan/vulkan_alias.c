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
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_palette.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "r_local.h"
#include "vid_vulkan.h"

typedef struct {
	mat4f_t     mat;
	float       blend;
	byte        colors[4];
	vec4f_t     base_color;
	vec4f_t     fog;
} alias_push_constants_t;

typedef struct {
	mat4f_t     mat;
	float       blend;
	byte        colors[4];
	float       ambient;
	float       shadelight;
	vec4f_t     lightvec;
	vec4f_t     base_color;
	vec4f_t     fog;
} fwd_push_constants_t;

typedef struct {
	mat4f_t     mat;
	float       blend;
	uint32_t    matrix_base;
} shadow_push_constants_t;

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
push_alias_constants (const mat4f_t mat, float blend, byte *colors,
					  vec4f_t base_color, int pass, qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	alias_push_constants_t constants = {
		.blend = blend,
		.colors = { VEC4_EXP (colors) },
		.base_color = base_color,
		.fog = Fog_Get (),
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (alias_push_constants_t, mat),
			sizeof (mat4f_t), mat },
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
	QFV_PushConstants (device, cmd, layout, pass ? 5 : 2, push_constants);
}

static void
push_fwd_constants (const mat4f_t mat, float blend, byte *colors,
					vec4f_t base_color, const alight_t *lighting,
					int pass, qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	fwd_push_constants_t constants = {
		.blend = blend,
		.colors = { VEC4_EXP (colors) },
		.ambient = lighting->ambientlight,
		.shadelight = lighting->shadelight,
		.lightvec = { VectorExpand (lighting->lightvec) },
		.base_color = base_color,
		.fog = Fog_Get (),
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (fwd_push_constants_t, mat),
			sizeof (mat4f_t), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (fwd_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, colors),
			sizeof (constants.colors), constants.colors },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, ambient),
			sizeof (constants.ambient), &constants.ambient },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, shadelight),
			sizeof (constants.shadelight), &constants.shadelight },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, lightvec),
			sizeof (constants.lightvec), &constants.lightvec },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, base_color),
			sizeof (constants.base_color), &constants.base_color },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			field_offset (fwd_push_constants_t, fog),
			sizeof (constants.fog), &constants.fog },
	};
	QFV_PushConstants (device, cmd, layout, 8, push_constants);
}

static void
push_shadow_constants (const mat4f_t mat, float blend, uint16_t *matrix_base,
					   qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	shadow_push_constants_t constants = {
		.blend = blend,
		.matrix_base = *matrix_base,
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (shadow_push_constants_t, mat),
			sizeof (mat4f_t), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (shadow_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			field_offset (shadow_push_constants_t, matrix_base),
			sizeof (uint32_t), &constants.matrix_base },
	};
	QFV_PushConstants (device, cmd, layout, 3, push_constants);
}

static void
alias_draw_ent (qfv_taskctx_t *taskctx, entity_t ent, int pass,
				renderer_t *renderer)
{
	auto model = renderer->model;
	mesh_t   *mesh = model->mesh;
	uint16_t *matrix_base = taskctx->data;

	auto animation = Entity_GetAnimation (ent);
	float blend = animation->blend;

	transform_t transform = Entity_Transform (ent);

	qfv_alias_skin_t *skin = nullptr;
	if (renderer->skin) {
		skin_t     *tskin = Skin_Get (renderer->skin);
		if (tskin) {
			skin = (qfv_alias_skin_t *) tskin->tex;
		}
	}
	if (!skin) {
		uint32_t skindesc;
		skindesc = renderer->skindesc;
		skin = (qfv_alias_skin_t *) ((byte *) mesh + skindesc);
	}
	vec4f_t base_color;
	QuatCopy (renderer->colormod, base_color);

	byte colors[4] = {};
	QuatCopy (skin->colors, colors);
	auto colormap = Entity_GetColormap (ent);
	if (colormap) {
		colors[0] = colormap->top * 16 + 8;
		colors[1] = colormap->bottom * 16 + 8;
	}

	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	auto rmesh = (qfv_alias_mesh_t *) ((byte *) mesh + mesh->render_data);

	VkDeviceSize offsets[] = {
		animation->pose1,
		animation->pose2,
		0,
	};
	VkBuffer    buffers[] = {
		rmesh->vertex_buffer,
		rmesh->vertex_buffer,
		rmesh->uv_buffer,
	};
	int         bindingCount = skin ? 3 : 2;

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	dfunc->vkCmdBindVertexBuffers (cmd, 0, bindingCount, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, rmesh->index_buffer, 0,
								 VK_INDEX_TYPE_UINT32);
	if (pass && skin) {
		VkDescriptorSet sets[] = {
			skin->descriptor,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 2, 1, sets, 0, 0);
	}
	if (matrix_base) {
		push_shadow_constants (Transform_GetWorldMatrixPtr (transform),
							   blend, matrix_base, taskctx);
	} else {
		if (pass > 1) {
			alight_t    lighting;
			R_Setup_Lighting (ent, &lighting);
			push_fwd_constants (Transform_GetWorldMatrixPtr (transform),
								blend, colors, base_color, &lighting,
								pass, taskctx);
		} else {
			push_alias_constants (Transform_GetWorldMatrixPtr (transform),
								  blend, colors, base_color, pass, taskctx);
		}
	}
	dfunc->vkCmdDrawIndexed (cmd, 3 * rmesh->numtris, 1, 0, 0, 0);
	QFV_CmdEndLabel (device, cmd);
}

static void
alias_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto pass = *(int *) params[0]->value;
	auto stage = *(int *) params[1]->value;

	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;
	bool shadow = !!taskctx->data;
	VkDescriptorSet sets[] = {
		shadow ? Vulkan_Lighting_Descriptors (ctx, ctx->curFrame)
			   : Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		Vulkan_Palette_Descriptor (ctx),
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, shadow ? 1 : 2, sets, 0, 0);

	auto queue = r_ent_queue;	//FIXME fetch from scene
	for (size_t i = 0; i < queue->ent_queues[mod_mesh].size; i++) {
		entity_t    ent = queue->ent_queues[mod_mesh].a[i];
		auto renderer = Entity_GetRenderer (ent);
		if ((stage == alias_shadow && renderer->noshadows)
			|| (stage == alias_main && renderer->onlyshadows)) {
			continue;
		}
		// FIXME hack the depth range to prevent view model
		// from poking into walls
		if (stage == alias_main && renderer->depthhack) {
			alias_depth_range (taskctx, 0.7, 1);
		}
		alias_draw_ent (taskctx, ent, pass, renderer);
		// unhack in case the view_model is not the last
		if (stage == alias_main && renderer->depthhack) {
			alias_depth_range (taskctx, 0, 1);
		}
	}
}

static void
alias_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "alias shutdown");
	auto actx = ctx->alias_context;

	free (actx);
	qfvPopDebug (ctx);
}

static void
alias_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto actx = ctx->alias_context;
	actx->sampler = QFV_Render_Sampler (ctx, "qskin_sampler");
}

static void
alias_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "alias init");

	QFV_Render_AddShutdown (ctx, alias_shutdown);
	QFV_Render_AddStartup (ctx, alias_startup);

	aliasctx_t *actx = calloc (1, sizeof (aliasctx_t));
	ctx->alias_context = actx;

	qfvPopDebug (ctx);
}

static exprenum_t alias_stage_enum;
static exprtype_t alias_stage_type = {
	.name = "alias_stage",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &alias_stage_enum,
};
static int alias_stage_values[] = { alias_main, alias_shadow, };
static exprsym_t alias_stage_symbols[] = {
	{"main", &alias_stage_type, alias_stage_values + 0},
	{"shadow", &alias_stage_type, alias_stage_values + 1},
	{}
};
static exprtab_t alias_stage_symtab = { .symbols = alias_stage_symbols };
static exprenum_t alias_stage_enum = {
	&alias_stage_type,
	&alias_stage_symtab,
};

static exprtype_t *alias_draw_params[] = {
	&cexpr_int,
	&alias_stage_type,
};
static exprfunc_t alias_draw_func[] = {
	{ .func = alias_draw, .num_params = 2, .param_types = alias_draw_params },
	{}
};

static exprfunc_t alias_init_func[] = {
	{ .func = alias_init },
	{}
};

static exprsym_t alias_task_syms[] = {
	{ "alias_draw", &cexpr_function, alias_draw_func },
	{ "alias_init", &cexpr_function, alias_init_func },
	{}
};

void
Vulkan_Alias_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	qfvPushDebug (ctx, "alias init");
	QFV_Render_AddTasks (ctx, alias_task_syms);

	qfvPopDebug (ctx);
}
