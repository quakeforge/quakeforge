/*
	vulkan_mesh.c

	Vulkan mesh model pipeline

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

#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_mesh.h"
#include "QF/Vulkan/qf_palette.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "r_local.h"
#include "vid_vulkan.h"

typedef struct {
	mat4f_t     mat;
	uint32_t    enabled_mask;
	float       blend;
	uint32_t    debug_bone;
	byte        colors[4];
	vec4f_t     base_color;
	vec4f_t     fog;
} mesh_push_constants_t;

typedef struct {
	mat4f_t     mat;
	uint32_t    enabled_mask;
	float       blend;
	uint32_t    debug_bone;
	byte        colors[4];
	vec4f_t     base_color;
	vec4f_t     fog;
	vec4f_t     lightvec;
	float       ambient;
	float       shadelight;
} fwd_push_constants_t;

typedef struct {
	mat4f_t     mat;
	uint32_t    enabled_mask;
	float       blend;
	uint32_t    matrix_base;
} shadow_push_constants_t;

static void
mesh_depth_range (qfv_taskctx_t *taskctx, float minDepth, float maxDepth)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto viewport = taskctx->pipeline->viewport;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	dfunc->vkCmdSetViewport (taskctx->cmd, 0, 1, &viewport);
}

static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	0, 0, 0
};

void
Vulkan_MeshAddBones (vulkan_ctx_t *ctx, qf_model_t *model)
{
	qfvPushDebug (ctx, "Vulkan_MeshAddBones");

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto rctx = ctx->render_context;
	auto mctx = ctx->mesh_context;
	auto rmesh = (qfv_mesh_t *) ((byte *) model + model->render_data);
	auto bone_descs = (VkDescriptorSet *) ((byte *) rmesh
										   + rmesh->bone_descriptors);
	int  num_sets = rctx->frames.size;

	if (!mctx->dsmanager) {
		mctx->dsmanager = QFV_Render_DSManager (ctx, "bone_set");
	}

	for (int i = 0; i < num_sets; i++) {
		auto set = QFV_DSManager_AllocSet (mctx->dsmanager);
		bone_descs[i] = set;
	}

	VkDescriptorBufferInfo bufferInfo[num_sets];
	size_t      bones_size = sizeof (vec4f_t[model->joints.count * 3]);
	if (!bones_size) {
		bones_size = sizeof (vec4f_t[3]);
	}
	auto bones_buffer = rmesh->bones_buffer;
	if (!bones_buffer) {
		bones_buffer = mctx->null_bone;
	}
	for (int i = 0; i < num_sets; i++) {
		bufferInfo[i].buffer = bones_buffer;
		bufferInfo[i].offset = i * bones_size;
		bufferInfo[i].range = bones_size;
	};
	VkWriteDescriptorSet write[num_sets];
	for (int i = 0; i < num_sets; i++) {
		write[i] = base_buffer_write;
		write[i].dstSet = bone_descs[i];
		write[i].pBufferInfo = &bufferInfo[i];
	}
	dfunc->vkUpdateDescriptorSets (device->dev, num_sets, write, 0, 0);

	qfvPopDebug (ctx);
}

void
Vulkan_MeshRemoveBones (vulkan_ctx_t *ctx, qf_model_t *model)
{
	auto rctx = ctx->render_context;
	auto mctx = ctx->mesh_context;
	auto rmesh = (qfv_mesh_t *) ((byte *) model + model->render_data);
	auto bone_descs = (VkDescriptorSet *) ((byte *) rmesh
										   + rmesh->bone_descriptors);
	int  num_sets = rctx->frames.size;

	for (int i = 0; i < num_sets; i++) {
		QFV_DSManager_FreeSet (mctx->dsmanager, bone_descs[i]);
	}
}

void
Vulkan_MeshAddSkin (vulkan_ctx_t *ctx, qfv_skin_t *skin)
{
	meshctx_t *mctx = ctx->mesh_context;
	skin->descriptor = Vulkan_CreateCombinedImageSampler (ctx, skin->view,
														  mctx->sampler);
}

void
Vulkan_MeshRemoveSkin (vulkan_ctx_t *ctx, qfv_skin_t *skin)
{
	Vulkan_FreeTexture (ctx, skin->descriptor);
	skin->descriptor = 0;
}

static void
push_mesh_constants (const mat4f_t mat, uint32_t enabled_mask, float blend,
					 int debug_bone, byte *colors, vec4f_t base_color,
					 int pass, qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	mesh_push_constants_t constants = {
		.enabled_mask = enabled_mask,
		.blend = blend,
		.debug_bone = debug_bone,
		.colors = { VEC4_EXP (colors) },
		.base_color = base_color,
		.fog = Fog_Get (),
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, mat),
			sizeof (mat4f_t), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, enabled_mask),
			sizeof (uint32_t), &constants.enabled_mask },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, debug_bone),
			sizeof (float), &constants.debug_bone },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (mesh_push_constants_t, colors),
			sizeof (constants.colors), constants.colors },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (mesh_push_constants_t, base_color),
			sizeof (constants.base_color), &constants.base_color },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (mesh_push_constants_t, fog),
			sizeof (constants.fog), &constants.fog },
	};
	QFV_PushConstants (device, cmd, layout, pass ? 7 : 4, push_constants);
}

static void
push_fwd_constants (const mat4f_t mat, uint32_t enabled_mask, float blend,
					int debug_bone, byte *colors, vec4f_t base_color,
					const alight_t *lighting, int pass, qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	fwd_push_constants_t constants = {
		.blend = blend,
		.enabled_mask = enabled_mask,
		.debug_bone = debug_bone,
		.colors = { VEC4_EXP (colors) },
		.base_color = base_color,
		.fog = Fog_Get (),
		.lightvec = { VectorExpand (lighting->lightvec) },
		.ambient = lighting->ambientlight,
		.shadelight = lighting->shadelight,
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (fwd_push_constants_t, mat),
			sizeof (mat4f_t), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, enabled_mask),
			sizeof (uint32_t), &constants.enabled_mask },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (fwd_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (mesh_push_constants_t, debug_bone),
			sizeof (uint32_t), &constants.debug_bone },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, colors),
			sizeof (constants.colors), constants.colors },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, base_color),
			sizeof (constants.base_color), &constants.base_color },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, fog),
			sizeof (constants.fog), &constants.fog },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, lightvec),
			sizeof (constants.lightvec), &constants.lightvec },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, ambient),
			sizeof (constants.ambient), &constants.ambient },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,
			offsetof (fwd_push_constants_t, shadelight),
			sizeof (constants.shadelight), &constants.shadelight },
	};
	QFV_PushConstants (device, cmd, layout, 9, push_constants);
}

static void
push_shadow_constants (const mat4f_t mat, uint32_t enabled_mask, float blend,
					   uint16_t *matrix_base, qfv_taskctx_t *taskctx)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	shadow_push_constants_t constants = {
		.enabled_mask = enabled_mask,
		.blend = blend,
		.matrix_base = *matrix_base,
	};

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (shadow_push_constants_t, mat),
			sizeof (mat4f_t), mat },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (shadow_push_constants_t, enabled_mask),
			sizeof (uint32_t), &constants.enabled_mask },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (shadow_push_constants_t, blend),
			sizeof (float), &constants.blend },
		{ VK_SHADER_STAGE_VERTEX_BIT,
			offsetof (shadow_push_constants_t, matrix_base),
			sizeof (uint32_t), &constants.matrix_base },
	};
	QFV_PushConstants (device, cmd, layout, 4, push_constants);
}

static struct {
	VkFormat    format[4];
	VkIndexType index_type;
} qfv_mesh_type_map[] = {
	[qfm_s8]   = {
		.format = {
			VK_FORMAT_R8_SINT,
			VK_FORMAT_R8G8_SINT,
			VK_FORMAT_R8G8B8_SINT,
			VK_FORMAT_R8G8B8A8_SINT,
		},
	},
	[qfm_s16]  = {
		.format = {
			VK_FORMAT_R16_SINT,
			VK_FORMAT_R16G16_SINT,
			VK_FORMAT_R16G16B16_SINT,
			VK_FORMAT_R16G16B16A16_SINT,
		},
	},
	[qfm_s32]  = {
		.format = {
			VK_FORMAT_R32_SINT,
			VK_FORMAT_R32G32_SINT,
			VK_FORMAT_R32G32B32_SINT,
			VK_FORMAT_R32G32B32A32_SINT,
		},
	},
	[qfm_s64]  = {
		.format = {
			VK_FORMAT_R64_SINT,
			VK_FORMAT_R64G64_SINT,
			VK_FORMAT_R64G64B64_SINT,
			VK_FORMAT_R64G64B64A64_SINT,
		},
	},

	[qfm_u8]   = {
		.format = {
			VK_FORMAT_R8_UINT,
			VK_FORMAT_R8G8_UINT,
			VK_FORMAT_R8G8B8_UINT,
			VK_FORMAT_R8G8B8A8_UINT,
		},
		.index_type = VK_INDEX_TYPE_UINT8,
	},
	[qfm_u16]  = {
		.format = {
			VK_FORMAT_R16_UINT,
			VK_FORMAT_R16G16_UINT,
			VK_FORMAT_R16G16B16_UINT,
			VK_FORMAT_R16G16B16A16_UINT,
		},
		.index_type = VK_INDEX_TYPE_UINT16,
	},
	[qfm_u32]  = {
		.format = {
			VK_FORMAT_R32_UINT,
			VK_FORMAT_R32G32_UINT,
			VK_FORMAT_R32G32B32_UINT,
			VK_FORMAT_R32G32B32A32_UINT,
		},
		.index_type = VK_INDEX_TYPE_UINT32,
	},
	[qfm_u64]  = {
		.format = {
			VK_FORMAT_R64_UINT,
			VK_FORMAT_R64G64_UINT,
			VK_FORMAT_R64G64B64_UINT,
			VK_FORMAT_R64G64B64A64_UINT,
		},
	},

	[qfm_s8n]  = {
		.format = {
			VK_FORMAT_R8_SNORM,
			VK_FORMAT_R8G8_SNORM,
			VK_FORMAT_R8G8B8_SNORM,
			VK_FORMAT_R8G8B8A8_SNORM,
		},
	},
	[qfm_s16n] = {
		.format = {
			VK_FORMAT_R16_SNORM,
			VK_FORMAT_R16G16_SNORM,
			VK_FORMAT_R16G16B16_SNORM,
			VK_FORMAT_R16G16B16A16_SNORM,
		},
	},
	[qfm_u8n]  = {
		.format = {
			VK_FORMAT_R8_UNORM,
			VK_FORMAT_R8G8_UNORM,
			VK_FORMAT_R8G8B8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
		},
	},
	[qfm_u16n] = {
		.format = {
			VK_FORMAT_R16_UNORM,
			VK_FORMAT_R16G16_UNORM,
			VK_FORMAT_R16G16B16_UNORM,
			VK_FORMAT_R16G16B16A16_UNORM,
		},
	},

	[qfm_f16]  = {
		.format = {
			VK_FORMAT_R16_SFLOAT,
			VK_FORMAT_R16G16_SFLOAT,
			VK_FORMAT_R16G16B16_SFLOAT,
			VK_FORMAT_R16G16B16A16_SFLOAT,
		},
	},
	[qfm_f32]  = {
		.format = {
			VK_FORMAT_R32_SFLOAT,
			VK_FORMAT_R32G32_SFLOAT,
			VK_FORMAT_R32G32B32_SFLOAT,
			VK_FORMAT_R32G32B32A32_SFLOAT,
		},
	},
	[qfm_f64]  = {
		.format = {
			VK_FORMAT_R64_SFLOAT,
			VK_FORMAT_R64G64_SFLOAT,
			VK_FORMAT_R64G64B64_SFLOAT,
			VK_FORMAT_R64G64B64A64_SFLOAT,
		},
	},
};

static qfm_attrdesc_t default_attributes[] = {
	{ .type = qfm_s8n, .components = 3 },
	{ .type = qfm_s8n, .components = 3 },
	{ .type = qfm_s8n, .components = 4 },
	{ .type = qfm_u8n, .components = 2 },
	{ .type = qfm_u8n, .components = 4 },
	{ .type = qfm_u8,  .components = 4 },
	{ .type = qfm_u8n, .components = 4 },
	{ .type = qfm_s8n, .components = 3 },
	{ .type = qfm_s8n, .components = 3 },
	{ .type = qfm_s8n, .components = 4 },
	{ .type = qfm_u8n, .components = 2 },
	{ .type = qfm_u8n, .components = 4 },
};

static uint32_t
set_vertex_attributes (qf_mesh_t *mesh, VkCommandBuffer cmd, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto attribs = (qfm_attrdesc_t *) ((byte *) mesh + mesh->attributes.offset);
	uint32_t num_bindings = 0;
	for (uint32_t i = 0; i < mesh->attributes.count; i++) {
		if (num_bindings < attribs[i].set + 1u) {
			num_bindings = attribs[i].set + 1u;
		}
	}
	//FIXME want VK_EXT_vertex_attribute_robustness, but renderdoc doesn't
	//like it
	uint32_t num_attributes = qfm_attr_count + qfm_color + 1;
	VkVertexInputBindingDescription2EXT bindings[num_bindings];
	VkVertexInputAttributeDescription2EXT attributes[num_attributes];

	for (uint32_t i = 0; i < num_bindings; i++) {
		bindings[i] = (VkVertexInputBindingDescription2EXT) {
			.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
			.binding = i,
			.stride = 0,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			.divisor = 1,
		};
	}
	for (uint32_t i = 0; i < num_attributes; i++) {
		uint32_t attr = i < qfm_attr_count ? i : i - qfm_attr_count;
		auto a = default_attributes[attr];
		attributes[i] = (VkVertexInputAttributeDescription2EXT) {
			.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
			.location = i,
			.format = qfv_mesh_type_map[a.type].format[a.components - 1],
		};
	}

	uint32_t enabled_mask = 0;

	for (uint32_t i = 0; i < mesh->attributes.count; i++) {
		auto a = attribs[i];
		uint32_t location = a.attr + a.morph * qfm_attr_count;
		attributes[location] = (VkVertexInputAttributeDescription2EXT) {
			.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
			.location = location,
			.binding = a.set,
			.format = qfv_mesh_type_map[a.type].format[a.components - 1],
			.offset = a.offset,
		};
		enabled_mask |= 1 << location;

		bindings[a.set] = (VkVertexInputBindingDescription2EXT) {
			.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
			.binding = a.set,
			.stride = a.stride,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			.divisor = 1,
		};
	}
	dfunc->vkCmdSetVertexInputEXT (cmd, num_bindings, bindings,
								   num_attributes, attributes);
	return enabled_mask;
}

static qfv_skin_t *
get_skin (renderer_t *renderer, qf_mesh_t *mesh)
{
	qfv_skin_t *skin = nullptr;
	if (renderer->skin) {
		skin_t     *tskin = Skin_Get (renderer->skin);
		if (tskin) {
			skin = (qfv_skin_t *) tskin->tex;
		}
	}
	if (!skin) {
		uint32_t skindesc;
		if (renderer->skindesc) {
			skindesc = renderer->skindesc;
		} else {
			if (!mesh->skin.numclips) {
				return nullptr;
			}
			uint32_t keyframe = mesh->skin.keyframes;
			auto skinframe = (keyframe_t *) ((byte *) mesh + keyframe);
			skindesc = skinframe->data;
		}
		skin = (qfv_skin_t *) ((byte *) mesh + skindesc);
	}
	return skin;
}

static void
mesh_draw_ent (qfv_taskctx_t *taskctx, entity_t ent, int pass,
				renderer_t *renderer)
{
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;
	auto mctx = ctx->mesh_context;

	auto model = renderer->model->model;
	auto meshes = (qf_mesh_t *) ((byte *) model + model->meshes.offset);
	uint16_t *matrix_base = taskctx->data;

	transform_t transform = Entity_Transform (ent);

	auto skin = get_skin (renderer, &meshes[0]);
	if (!skin) {
		skin = &mctx->default_skin;
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

	auto rmesh = (qfv_mesh_t *) ((byte *) model + model->render_data);
	auto bone_descs = (VkDescriptorSet *) ((byte *) rmesh
										   + rmesh->bone_descriptors);

	auto anim = Entity_GetAnimation (ent);
	float blend = anim ? anim->blend : 0;

	uint32_t enabled_mask = set_vertex_attributes (meshes, cmd, ctx);
	uint32_t c_matrix_palette = ent.base + scene_matrix_palette;
	if (Ent_HasComponent (ent.id, c_matrix_palette, ent.reg)) {
		auto mp = *(qfm_motor_t **) Ent_GetComponent (ent.id, c_matrix_palette,
													  ent.reg);
		vec4f_t    *bone_data;
		dfunc->vkMapMemory (device->dev, rmesh->bones_memory, 0, VK_WHOLE_SIZE,
							0, (void **)&bone_data);
		uint32_t num_joints = model->joints.count;
		for (uint32_t i = 0; i < num_joints; i++) {
			auto    b = bone_data + (ctx->curFrame * num_joints + i) * 3;
			mat4f_t f;
			qfm_motor_to_mat (f, mp[i]);
			// R_IQMBlendFrames sets up the frame as a 4x4 matrix for m * v,
			// but the shader wants a 3x4 (column x row) matrix for v * m,
			// which is just a transpose (and drop of the 4th column) away.
			mat4ftranspose (f, f);
			// copy only the first 3 columns
			memcpy (b, f, 3 * sizeof (vec4f_t));
		}
#define a(x) ((x) & ~0x3f)
		VkMappedMemoryRange range = {
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 0,
			rmesh->bones_memory,
			a(3 * ctx->curFrame * num_joints * sizeof (vec4f_t)),
			a(3 * num_joints * sizeof (vec4f_t) + 0x3f),
		};
#undef a
		dfunc->vkFlushMappedMemoryRanges (device->dev, 1, &range);
		dfunc->vkUnmapMemory (device->dev, rmesh->bones_memory);
	} else {
		enabled_mask &= ~((1 << qfm_joints) | (1 << qfm_weights));
	}

	VkDeviceSize offsets[] = { 0, 0, 0 };
	//FIXME per mesh, also allow mixing with bones
	if (meshes[0].morph.numclips) {
		offsets[0] = anim->pose1;
		offsets[1] = anim->pose2;
	}
	VkBuffer    buffers[] = {
		rmesh->geom_buffer,
		rmesh->geom_buffer,
		rmesh->rend_buffer,
	};
	int         bindingCount = skin ? 3 : 2;

	Vulkan_BeginEntityLabel (ctx, cmd, ent);

	auto index_type = qfv_mesh_type_map[meshes[0].index_type].index_type;
	dfunc->vkCmdBindVertexBuffers (cmd, 0, bindingCount, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, rmesh->index_buffer, 0, index_type);
	VkDescriptorSet sets[] = {
		bone_descs[ctx->curFrame],
		skin ? skin->descriptor : nullptr,
	};
	auto bound_skin = skin;
	bool shadow = !!taskctx->data;
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout,
									shadow ? 1 : 2,
									pass && skin ? 2 : 1, sets, 0, 0);
	if (matrix_base) {
		push_shadow_constants (Transform_GetWorldMatrixPtr (transform),
							   enabled_mask, blend, matrix_base, taskctx);
	} else {
		if (pass > 1) {
			alight_t    lighting;
			R_Setup_Lighting (ent, &lighting);
			push_fwd_constants (Transform_GetWorldMatrixPtr (transform),
								enabled_mask, blend, anim->debug_bone,
								colors, base_color, &lighting, pass, taskctx);
		} else {
			push_mesh_constants (Transform_GetWorldMatrixPtr (transform),
								 enabled_mask, blend, anim->debug_bone,
								 colors, base_color, pass, taskctx);
		}
	}
	for (uint32_t i = 0; i < model->meshes.count; i++) {
		if (!shadow && pass && i) {
			skin = get_skin (renderer, &meshes[i]);
			if (skin && skin != bound_skin) {
				VkDescriptorSet sets[] = { skin->descriptor };
				bound_skin = skin;
				dfunc->vkCmdBindDescriptorSets (cmd,
												VK_PIPELINE_BIND_POINT_GRAPHICS,
												layout, 3, 1, sets, 0, 0);
			}
		}
		uint32_t num_tris = meshes[i].triangle_count;
		uint32_t indices = meshes[i].indices;
		dfunc->vkCmdDrawIndexed (cmd, 3 * num_tris, 1, indices, 0, 0);
	}
	QFV_CmdEndLabel (device, cmd);
}

static void
mesh_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
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
		if ((stage == mesh_shadow && renderer->noshadows)
			|| (stage == mesh_main && renderer->onlyshadows)) {
			continue;
		}
		// FIXME hack the depth range to prevent view model
		// from poking into walls
		if (stage == mesh_main && renderer->depthhack) {
			mesh_depth_range (taskctx, 0.7, 1);
		}
		mesh_draw_ent (taskctx, ent, pass, renderer);
		// unhack in case the view_model is not the last
		if (stage == mesh_main && renderer->depthhack) {
			mesh_depth_range (taskctx, 0, 1);
		}
	}
}

static void
mesh_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	qfvPushDebug (ctx, "mesh shutdown");
	auto mctx = ctx->mesh_context;

	QFV_DestroyResource (device, mctx->resource);
	free (mctx->resource);

	free (mctx);
	qfvPopDebug (ctx);
}

static void
mesh_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto rctx = ctx->render_context;
	auto mctx = ctx->mesh_context;
	mctx->sampler = QFV_Render_Sampler (ctx, "qskin_sampler");

	mctx->resource = malloc (sizeof (qfv_resource_t) + sizeof (qfv_resobj_t));
	mctx->resource[0] = (qfv_resource_t) {
		.name = "mesh",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 1,
		.objects = (qfv_resobj_t *) &mctx->resource[1],
	};
	mctx->resource[0].objects[0] = (qfv_resobj_t) {
		.name = "null_bone",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (vec4f_t[3]) * rctx->frames.size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		},
	};
	QFV_CreateResource (device, mctx->resource);
	mctx->null_bone = mctx->resource[0].objects[0].buffer.buffer;
	mctx->default_skin = (qfv_skin_t) {
		.view = ctx->default_skin,
	};
	Vulkan_MeshAddSkin (ctx, &mctx->default_skin);
}

static void
mesh_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "mesh init");

	QFV_Render_AddShutdown (ctx, mesh_shutdown);
	QFV_Render_AddStartup (ctx, mesh_startup);

	meshctx_t *mctx = calloc (1, sizeof (meshctx_t));
	ctx->mesh_context = mctx;

	qfvPopDebug (ctx);
}

static exprenum_t mesh_stage_enum;
static exprtype_t mesh_stage_type = {
	.name = "mesh_stage",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &mesh_stage_enum,
};
static int mesh_stage_values[] = { mesh_main, mesh_shadow, };
static exprsym_t mesh_stage_symbols[] = {
	{"main", &mesh_stage_type, mesh_stage_values + 0},
	{"shadow", &mesh_stage_type, mesh_stage_values + 1},
	{}
};
static exprtab_t mesh_stage_symtab = { .symbols = mesh_stage_symbols };
static exprenum_t mesh_stage_enum = {
	&mesh_stage_type,
	&mesh_stage_symtab,
};

static exprtype_t *mesh_draw_params[] = {
	&cexpr_int,
	&mesh_stage_type,
};
static exprfunc_t mesh_draw_func[] = {
	{ .func = mesh_draw, .num_params = 2, .param_types = mesh_draw_params },
	{}
};

static exprfunc_t mesh_init_func[] = {
	{ .func = mesh_init },
	{}
};

static exprsym_t mesh_task_syms[] = {
	{ "mesh_draw", &cexpr_function, mesh_draw_func },
	{ "mesh_init", &cexpr_function, mesh_init_func },
	{}
};

void
Vulkan_Mesh_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	qfvPushDebug (ctx, "mesh init");
	QFV_Render_AddTasks (ctx, mesh_task_syms);

	qfvPopDebug (ctx);
}
