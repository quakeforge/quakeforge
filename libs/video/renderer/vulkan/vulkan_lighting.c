/*
	vulkan_lighting.c

	Vulkan lighting pass pipeline

	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/2/23

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
#include "QF/dstring.h"
#include "QF/heapsort.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/script.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/scene.h"
#include "QF/ui/imui.h"
#define IMUI_context imui_ctx

#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_draw.h"
#include "QF/Vulkan/qf_lighting.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/descriptor.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/projection.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "compat.h"

#include "r_internal.h"
#include "vid_vulkan.h"
#include "vkparse.h"

#define ico_verts 12
#define cone_verts 7
static int ico_inds[] = {
	0,  4,  6,  9, 2,  8, 4, -1,
	3,  1, 10, 5,  7, 11, 1, -1,
	1, 11,  6, 4, 10, -1,
	9,  6, 11, 7,  2, -1,
	5, 10,  8, 2,  7, -1,
	4,  8, 10,
};
#define num_ico_inds (sizeof (ico_inds) / sizeof (ico_inds[0]))
static int cone_inds[] = {
	0, 1, 2, 3, 4, 5, 6, 1, -1,
	1, 6, 5, 4, 3, 2,
};
#define num_cone_inds (sizeof (cone_inds) / sizeof (cone_inds[0]))

#define dynlight_max 32
static int dynlight_size;
static cvar_t dynlight_size_cvar = {
	.name = "dynlight_size",
	.description =
		"Effective radius of dynamic light shadow maps. Needs map reload to "
		"take effect",
	.default_value = "250",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &dynlight_size },
};
#if 0
static const light_t *
get_light (entity_t ent)
{
	return Ent_GetComponent (ent.id, scene_light, ent.reg);
}
#endif
static const dlight_t *
get_dynlight (entity_t ent)
{
	return Ent_GetComponent (ent.id, scene_dynlight, ent.reg);
}

static bool
has_dynlight (entity_t ent)
{
	return Ent_HasComponent (ent.id, scene_dynlight, ent.reg);
}

static uint32_t
get_lightstyle (entity_t ent)
{
	return *(uint32_t *) Ent_GetComponent (ent.id, scene_lightstyle, ent.reg);
}

static uint32_t
get_lightleaf (entity_t ent)
{
	return *(uint32_t *) Ent_GetComponent (ent.id, scene_lightleaf, ent.reg);
}

static uint32_t
get_lightid (entity_t ent)
{
	return *(uint32_t *) Ent_GetComponent (ent.id, scene_lightid, ent.reg);
}

static void
set_lightid (uint32_t ent, ecs_registry_t *reg, uint32_t id)
{
	Ent_SetComponent (ent, scene_lightid, reg, &id);
}

static void
lighting_setup_shadow (const exprval_t **params, exprval_t *result,
					   exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;

	if (!lctx->ldata) {
		return;
	}
	auto pass = Vulkan_Bsp_GetPass (ctx, QFV_bspShadow);
	auto brush = pass->brush;
	set_t leafs = SET_STATIC_INIT (brush->modleafs, alloca);
	set_empty (&leafs);

	auto entqueue = r_ent_queue;   //FIXME fetch from scene
	for (size_t i = 0; i < entqueue->ent_queues[mod_light].size; i++) {
		entity_t    ent = entqueue->ent_queues[mod_light].a[i];
		if (!has_dynlight (ent)) {
			auto ls = get_lightstyle (ent);
			if (!d_lightstylevalue[ls]) {
				continue;
			}
		}
		auto leafnum = get_lightleaf (ent);
		if (leafnum != ~0u) {
			set_add (&leafs, leafnum);
		}
	}

	set_t pvs = SET_STATIC_INIT (brush->visleafs, alloca);
	auto iter = set_first (&leafs);
	if (!iter) {
		return;
	}
	if (iter->element == 0) {
		set_assign (&pvs, lctx->ldata->sun_pvs);
	}  else {
		Mod_LeafPVS_set (brush->leafs + iter->element, brush, 0, &pvs);
	}
	for (iter = set_next (iter); iter; iter = set_next (iter)) {
		Mod_LeafPVS_mix (brush->leafs + iter->element, brush, 0, &pvs);
	}

	visstate_t visstate = {
		.node_visframes = pass->node_frames,
		.leaf_visframes = pass->leaf_frames,
		.face_visframes = pass->face_frames,
		.visframecount = pass->vis_frame,
		.brush = pass->brush,
	};
	R_MarkLeavesPVS (&visstate, &pvs);
	pass->vis_frame = visstate.visframecount;
}

static VkImageView
create_view (vulkan_ctx_t *ctx, light_control_t *renderer)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	VkImageViewCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = lctx->map_images[renderer->map_index],
		.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		.format = VK_FORMAT_X8_D24_UNORM_PACK32,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.levelCount = 1,
			.baseArrayLayer = renderer->layer,
			.layerCount = renderer->numLayers,
		},
	};
	VkImageView view;
	dfunc->vkCreateImageView (device->dev, &cInfo, 0, &view);
	return view;
}

static VkFramebuffer
create_framebuffer (vulkan_ctx_t *ctx, light_control_t *renderer,
					VkImageView view, VkRenderPass renderpass)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkFramebuffer framebuffer;
	dfunc->vkCreateFramebuffer (device->dev,
		&(VkFramebufferCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderpass,
			.attachmentCount = 1,
			.pAttachments = &view,
			.width = renderer->size,
			.height = renderer->size,
			.layers = 1,
		}, 0, &framebuffer);
	return framebuffer;
}

static void
clear_frame_buffers_views (vulkan_ctx_t *ctx, lightingframe_t *lframe)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	for (size_t i = 0; i < lframe->framebuffers.size; i++) {
		auto framebuffer = lframe->framebuffers.a[i];
		dfunc->vkDestroyFramebuffer (device->dev, framebuffer, 0);
	}
	lframe->framebuffers.size = 0;
	for (size_t i = 0; i < lframe->views.size; i++) {
		auto view = lframe->views.a[i];
		dfunc->vkDestroyImageView (device->dev, view, 0);
	}
	lframe->views.size = 0;
}

static void
lighting_draw_shadow_maps (const exprval_t **params, exprval_t *result,
						   exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;
	auto shadow = QFV_GetStep (params[0], ctx->render_context->job);
	auto render = shadow->render;
	auto lframe = &lctx->frames.a[ctx->curFrame];

	if (!lctx->num_maps) {
		return;
	}

	clear_frame_buffers_views (ctx, lframe);

	auto entqueue = r_ent_queue;   //FIXME fetch from scene
	for (size_t i = 0; i < entqueue->ent_queues[mod_light].size; i++) {
		entity_t    ent = entqueue->ent_queues[mod_light].a[i];
		uint32_t    id = get_lightid (ent);
		if (id >= lctx->light_control.size) {
			continue;
		}
		auto r = &lctx->light_control.a[id];
		if (!r->numLayers) {
			continue;
		}
		if (!has_dynlight (ent)) {
			auto ls = get_lightstyle (ent);
			if (!d_lightstylevalue[ls]) {
				continue;
			}
		}
		auto renderpass = &render->renderpasses[r->renderpass_index];
		auto view = create_view (ctx, r);
		auto bi = &renderpass->beginInfo;
		auto fbuffer = create_framebuffer (ctx, r, view, bi->renderPass);
		bi->framebuffer = fbuffer;
		QFV_RunRenderPass (ctx, renderpass, r->size, r->size, &r->matrix_id);
		DARRAY_APPEND (&lframe->views, view);
		DARRAY_APPEND (&lframe->framebuffers, fbuffer);
		bi->framebuffer = 0;
	}
}

static uint32_t
make_id (uint32_t matrix_index, uint32_t map_index, uint32_t layer,
		 uint32_t type)
{
	if (type == ST_CUBE) {
		layer /= 6;
	}
	return ((matrix_index & 0x1fff) << 0)
		 | ((map_index & 0x1f) << 13)
		 | ((layer & 0x7ff) << 18)
		 | ((type & 3) << 29);
}

static void
cube_mat (mat4f_t *mat, vec4f_t position)
{
	mat4f_t     view;
	mat4fidentity (view);
	view[3] = -position;
	view[3][3] = 1;

	mat4f_t     proj;
	QFV_PerspectiveTan (proj, 1, 1);
	for (int j = 0; j < 6; j++) {
		mat4f_t side_view;
		mat4f_t rotinv;
		mat4ftranspose (rotinv, qfv_box_rotations[j]);
		mmulf (side_view, rotinv, view);
		mmulf (side_view, qfv_z_up, side_view);
		mmulf (mat[j], proj, side_view);
	}
}

static void
lighting_update_lights (const exprval_t **params, exprval_t *result,
						exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;

	auto lframe = &lctx->frames.a[ctx->curFrame];

	memset (lframe->light_queue, 0, sizeof (lframe->light_queue));
	if (!lctx->scene || !lctx->scene->lights) {
		return;
	}

	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];

	auto packet = QFV_PacketAcquire (ctx->staging);
	vec4f_t *styles = QFV_PacketExtend (packet, sizeof (vec4f_t[NumStyles]));
	for (int i = 0; i < NumStyles; i++) {
		styles[i] = (vec4f_t) { 1, 1, 1, d_lightstylevalue[i] / 65536.0};
	}
	QFV_PacketCopyBuffer (packet, lframe->style_buffer, 0, bb);
	QFV_PacketSubmit (packet);

	uint32_t light_ids[4][MaxLights];
	uint32_t entids[4][MaxLights];

	uint32_t light_count = 0;
	auto queue = lframe->light_queue;

	uint32_t dynamic_light_entities[MaxLights];
	const dlight_t *dynamic_lights[MaxLights];
	int ndlight = 0;

	auto entqueue = r_ent_queue;   //FIXME fetch from scene
	for (size_t i = 0; i < entqueue->ent_queues[mod_light].size; i++) {
		entity_t    ent = entqueue->ent_queues[mod_light].a[i];
		if (has_dynlight (ent)) {
			dynamic_light_entities[ndlight] = ent.id;
			dynamic_lights[ndlight] = get_dynlight (ent);
			ndlight++;
			continue;
		}
		auto ls = get_lightstyle (ent);
		if (!d_lightstylevalue[ls]) {
			continue;
		}

		light_count++;
		uint32_t id = lctx->light_control.a[get_lightid (ent)].light_id;
		int mode =  lctx->light_control.a[get_lightid (ent)].mode;
		light_ids[mode][queue[mode].count] = id;
		entids[mode][queue[mode].count] = ent.id;
		queue[mode].count++;
	}

	if (ndlight) {
		light_count += ndlight;
		packet = QFV_PacketAcquire (ctx->staging);
		light_t *lights = QFV_PacketExtend (packet, sizeof (light_t[ndlight]));
		for (int i = 0; i < ndlight; i++) {
			uint32_t id = lctx->dynamic_base + i;
			set_lightid (dynamic_light_entities[i], lctx->scene->reg, id);
			light_ids[ST_CUBE][queue[ST_CUBE].count] = id;
			entids[ST_CUBE][queue[ST_CUBE].count] = dynamic_light_entities[i];
			queue[ST_CUBE].count++;

			VectorCopy (dynamic_lights[i]->color, lights[i].color);
			// dynamic lights seem a tad faint, so 16x map lights
			lights[i].color[3] = dynamic_lights[i]->radius / 16;
			VectorCopy (dynamic_lights[i]->origin, lights[i].position);
			// dlights are local point sources
			lights[i].position[3] = 1;
			lights[i].attenuation =
				(vec4f_t) { 0, 0, 1, 1/dynamic_lights[i]->radius };
			// full sphere, normal light (not ambient)
			lights[i].direction = (vec4f_t) { 0, 0, 1, 1 };
		}
		VkDeviceSize dlight_offset = sizeof (light_t[lctx->dynamic_base]);
		QFV_PacketCopyBuffer (packet, lframe->light_buffer, dlight_offset, bb);
		QFV_PacketSubmit (packet);

		packet = QFV_PacketAcquire (ctx->staging);
		uint32_t r_size = sizeof (qfv_light_render_t[ndlight]);
		qfv_light_render_t *render = QFV_PacketExtend (packet, r_size);
		for (int i = 0; i < ndlight; i++) {
			auto r = &lctx->light_control.a[lctx->dynamic_base + i];
			render[i] = (qfv_light_render_t) {
				.id_data = make_id(r->matrix_id, r->map_index, r->layer,
								   r->mode),
			};
			render[i].id_data |= 0x80000000;	// no style
		}
		dlight_offset = sizeof (qfv_light_render_t[lctx->dynamic_base]);
		QFV_PacketCopyBuffer (packet, lframe->render_buffer, dlight_offset, bb);
		QFV_PacketSubmit (packet);

		packet = QFV_PacketAcquire (ctx->staging);
		uint32_t msize = sizeof (mat4f_t[ndlight * 6]);
		mat4f_t *mats = QFV_PacketExtend (packet, msize);
		for (int i = 0; i < ndlight; i++) {
			cube_mat (&mats[i * 6], dynamic_lights[i]->origin);
		}
		VkDeviceSize mat_offset = sizeof (mat4f_t[lctx->dynamic_matrix_base]);
		QFV_PacketCopyBuffer (packet, lframe->shadowmat_buffer, mat_offset, bb);
		QFV_PacketSubmit (packet);
	}
	if (developer & SYS_lighting) {
		Vulkan_Draw_String (vid.width - 32, 8,
							va (ctx->va_ctx, "%3d", light_count),
							ctx);
	}

	if (light_count) {
		for (int i = 1; i < 4; i++) {
			queue[i].start = queue[i - 1].start + queue[i - 1].count;
		}
		packet = QFV_PacketAcquire (ctx->staging);
		uint32_t *lids = QFV_PacketExtend (packet,
										   sizeof (uint32_t[light_count]));
		for (int i = 0; i < 4; i++) {
			memcpy (lids + queue[i].start, light_ids[i],
					sizeof (uint32_t[queue[i].count]));
		}
		QFV_PacketCopyBuffer (packet, lframe->id_buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
		QFV_PacketSubmit (packet);

		packet = QFV_PacketAcquire (ctx->staging);
		uint32_t *eids = QFV_PacketExtend (packet,
										   sizeof (uint32_t[light_count]));
		for (int i = 0; i < 4; i++) {
			memcpy (eids + queue[i].start, entids[i],
					sizeof (uint32_t[queue[i].count]));
		}
		QFV_PacketCopyBuffer (packet, lframe->entid_buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
		QFV_PacketSubmit (packet);
	}
}

static void
lighting_update_descriptors (const exprval_t **params, exprval_t *result,
							 exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	auto lframe = &lctx->frames.a[ctx->curFrame];

	auto job = ctx->render_context->job;
	auto step = QFV_GetStep (params[0], job);
	auto render = step->render;
	auto fb = &render->active->framebuffer;
	VkDescriptorImageInfo attachInfo[] = {
		{	.imageView = fb->views[QFV_attachColor],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{	.imageView = fb->views[QFV_attachEmission],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{	.imageView = fb->views[QFV_attachNormal],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{	.imageView = fb->views[QFV_attachPosition],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};
	VkWriteDescriptorSet attachWrite[] = {
		{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lframe->attach_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &attachInfo[0], },
		{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lframe->attach_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &attachInfo[1], },
		{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lframe->attach_set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &attachInfo[2], },
		{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lframe->attach_set,
			.dstBinding = 3,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pImageInfo = &attachInfo[3], },
	};
	dfunc->vkUpdateDescriptorSets (device->dev,
								   LIGHTING_ATTACH_INFOS, attachWrite,
								   0, 0);
}

static void
lighting_bind_descriptors (const exprval_t **params, exprval_t *result,
						   exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;
	if (!lctx->num_maps) {
		return;
	}
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	auto shadow_type = *(int *) params[0]->value;
	auto stage = *(int *) params[1]->value;

	if (stage == lighting_debug) {
		VkDescriptorSet sets[] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			lframe->lights_set,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2, sets, 0, 0);

		VkBuffer buffers[] = {
			lframe->id_buffer,
			lctx->splat_verts,
		};
		VkDeviceSize offsets[] = { 0, 0 };
		dfunc->vkCmdBindVertexBuffers (cmd, 0, 2, buffers, offsets);
		dfunc->vkCmdBindIndexBuffer (cmd, lctx->splat_inds, 0,
									 VK_INDEX_TYPE_UINT32);
	} else {
		VkDescriptorSet sets[] = {
			lframe->shadowmat_set,
			lframe->lights_set,
			lframe->attach_set,
			(VkDescriptorSet[]) {
				lctx->shadow_2d_set,
				lctx->shadow_2d_set,
				lctx->shadow_2d_set,
				lctx->shadow_cube_set
			}[shadow_type],
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 4, sets, 0, 0);
	}
}

static void
lighting_draw_splats (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;
	auto cmd = taskctx->cmd;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	if (lframe->light_queue[ST_CUBE].count) {
		auto q = lframe->light_queue[ST_CUBE];
		dfunc->vkCmdDrawIndexed (cmd, num_ico_inds, q.count, 0, 0, q.start);
	}
	if (lframe->light_queue[ST_PLANE].count) {
		auto q = lframe->light_queue[ST_PLANE];
		dfunc->vkCmdDrawIndexed (cmd, num_cone_inds, q.count,
								 num_ico_inds, 12, q.start);
	}
}

static void
lighting_draw_lights (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	auto shadow_type = *(int *) params[0]->value;
	auto queue = lframe->light_queue[shadow_type];

	if (!queue.count) {
		return;
	}

	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (queue), &queue },
	};
	QFV_PushConstants (device, cmd, layout, 1, push_constants);

	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
}

static exprenum_t lighting_stage_enum;
static exprtype_t lighting_stage_type = {
	.name = "lighting_stage",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &lighting_stage_enum,
};
static int lighting_stage_values[] = {
	lighting_main,
	lighting_shadow,
	lighting_debug,
};
static exprsym_t lighting_stage_symbols[] = {
	{"main", &lighting_stage_type, lighting_stage_values + 0},
	{"shadow", &lighting_stage_type, lighting_stage_values + 1},
	{"debug", &lighting_stage_type, lighting_stage_values + 2},
	{}
};
static exprtab_t lighting_stage_symtab = { .symbols = lighting_stage_symbols };
static exprenum_t lighting_stage_enum = {
	&lighting_stage_type,
	&lighting_stage_symtab,
};

static exprenum_t shadow_type_enum;
static exprtype_t shadow_type_type = {
	.name = "shadow_type",
	.size = sizeof (int),
	.get_string = cexpr_enum_get_string,
	.data = &shadow_type_enum,
};
static int shadow_type_values[] = { ST_NONE, ST_PLANE, ST_CASCADE, ST_CUBE };
static exprsym_t shadow_type_symbols[] = {
	{"none", &shadow_type_type, shadow_type_values + 0},
	{"plane", &shadow_type_type, shadow_type_values + 1},
	{"cascade", &shadow_type_type, shadow_type_values + 2},
	{"cube", &shadow_type_type, shadow_type_values + 3},
	{}
};
static exprtab_t shadow_type_symtab = { .symbols = shadow_type_symbols };
static exprenum_t shadow_type_enum = {
	&shadow_type_type,
	&shadow_type_symtab,
};

static exprtype_t *shadow_type_param[] = {
	&shadow_type_type,
	&lighting_stage_type,
};

static exprtype_t *stepref_param[] = {
	&cexpr_string,
};

static exprfunc_t lighting_update_lights_func[] = {
	{ .func = lighting_update_lights },
	{}
};
static exprfunc_t lighting_update_descriptors_func[] = {
	{ .func = lighting_update_descriptors, .num_params = 1,
		.param_types = stepref_param },
	{}
};
static exprfunc_t lighting_bind_descriptors_func[] = {
	{ .func = lighting_bind_descriptors, .num_params = 2,
		.param_types = shadow_type_param },
	{}
};
static exprfunc_t lighting_draw_splats_func[] = {
	{ .func = lighting_draw_splats },
	{}
};
static exprfunc_t lighting_draw_lights_func[] = {
	{ .func = lighting_draw_lights, .num_params = 2,
		.param_types = shadow_type_param },
	{}
};
static exprfunc_t lighting_setup_shadow_func[] = {
	{ .func = lighting_setup_shadow },
	{}
};
static exprfunc_t lighting_draw_shadow_maps_func[] = {
	{ .func = lighting_draw_shadow_maps, .num_params = 1,
		.param_types = stepref_param },
	{}
};
static exprsym_t lighting_task_syms[] = {
	{ "lighting_update_lights", &cexpr_function, lighting_update_lights_func },
	{ "lighting_update_descriptors", &cexpr_function,
		lighting_update_descriptors_func },
	{ "lighting_bind_descriptors", &cexpr_function,
		lighting_bind_descriptors_func },
	{ "lighting_draw_splats", &cexpr_function, lighting_draw_splats_func },
	{ "lighting_draw_lights", &cexpr_function, lighting_draw_lights_func },
	{ "lighting_setup_shadow", &cexpr_function, lighting_setup_shadow_func },
	{ "lighting_draw_shadow_maps", &cexpr_function,
		lighting_draw_shadow_maps_func },
	{}
};

void
Vulkan_Lighting_Init (vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = calloc (1, sizeof (lightingctx_t));
	ctx->lighting_context = lctx;

	Cvar_Register (&dynlight_size_cvar, 0, 0);

	QFV_Render_AddTasks (ctx, lighting_task_syms);

	lctx->shadow_info = (qfv_attachmentinfo_t) {
		.name = "$shadow",
		.format = VK_FORMAT_X8_D24_UNORM_PACK32,
		.samples = 1,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	qfv_attachmentinfo_t *attachments[] = {
		&lctx->shadow_info,
	};
	QFV_Render_AddAttachments (ctx, 1, attachments);
}

static void
make_default_map (int size, VkImage default_map, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto packet = QFV_PacketAcquire (ctx->staging);
	size_t imgsize = size * size * sizeof (uint32_t);
	uint32_t *img = QFV_PacketExtend (packet, imgsize);
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 64; j++) {
			img[i * 64 + j] = ((j ^ i) & 1) ? 0x00ffffff : 0;
		}
	}

	auto ib = imageBarriers[qfv_LT_Undefined_to_TransferDst];
	ib.barrier.image = default_map;
	ib.barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, 1, &ib.barrier);

	VkBufferImageCopy copy_region[6];
	for (int i = 0; i < 6; i++) {
		copy_region[i] = (VkBufferImageCopy) {
			.bufferOffset = packet->offset,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, i, 1},
			{0, 0, 0}, {size, size, 1},
		};
	}
	dfunc->vkCmdCopyBufferToImage (packet->cmd, packet->stage->buffer,
								   default_map,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								   6, copy_region);

	ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.image = default_map;
	ib.barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	dfunc->vkCmdPipelineBarrier (packet->cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, 1, &ib.barrier);
	QFV_PacketSubmit (packet);
}

static void
make_ico (qfv_packet_t *packet)
{
	vec3_t *verts = QFV_PacketExtend (packet, sizeof (vec3_t[ico_verts]));
	float p = (sqrt(5) + 1) / 2;
	float a = sqrt (3) / p;
	float b = a / p;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 4; j++) {
			float my = j & 1 ? a : -a;
			float mz = j & 2 ? b : -b;
			int   vind = i * 4 + j;
			int   ix = i;
			int   iy = (i + 1) % 3;
			int   iz = (i + 2) % 3;
			verts[vind][ix] = 0;
			verts[vind][iy] = my;
			verts[vind][iz] = mz;
		}
	}
}

static void
make_cone (qfv_packet_t *packet)
{
	vec3_t *verts = QFV_PacketExtend (packet, sizeof (vec3_t[cone_verts]));
	float a = 2 / sqrt (3);
	float b = 1 / sqrt (3);
	VectorSet ( 0,  0,  0, verts[0]);
	VectorSet ( a,  0, -1, verts[1]);
	VectorSet ( b,  1, -1, verts[2]);
	VectorSet (-b,  1, -1, verts[3]);
	VectorSet (-a,  0, -1, verts[4]);
	VectorSet (-b, -1, -1, verts[5]);
	VectorSet ( b, -1, -1, verts[6]);
}

static void
write_inds (qfv_packet_t *packet)
{
	uint32_t *inds = QFV_PacketExtend (packet, sizeof (ico_inds)
												+ sizeof (cone_inds));
	memcpy (inds, ico_inds, sizeof (ico_inds));
	inds += num_ico_inds;
	memcpy (inds, cone_inds, sizeof (cone_inds));
}

void
Vulkan_Lighting_Setup (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "lighting init");

	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	lctx->sampler = QFV_Render_Sampler (ctx, "shadow_sampler");

	Vulkan_Script_SetOutput (ctx,
			&(qfv_output_t) { .format = VK_FORMAT_X8_D24_UNORM_PACK32 });

	DARRAY_INIT (&lctx->light_mats, 16);
	DARRAY_INIT (&lctx->light_control, 16);

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&lctx->frames, frames);
	DARRAY_RESIZE (&lctx->frames, frames);
	lctx->frames.grow = 0;

	lctx->light_resources = malloc (sizeof (qfv_resource_t)
									// splat vertices
									+ sizeof (qfv_resobj_t)
									// splat indices
									+ sizeof (qfv_resobj_t)
									// default shadow map and views
									+ 3 * sizeof (qfv_resobj_t)
									// light entids
									+ sizeof (qfv_resobj_t[frames])
									// light ids
									+ sizeof (qfv_resobj_t[frames])
									// light data
									+ sizeof (qfv_resobj_t[frames])
									// light render
									+ sizeof (qfv_resobj_t[frames])
									// light styles
									+ sizeof (qfv_resobj_t[frames])
									// light matrices
									+ sizeof (qfv_resobj_t[frames]));
	lctx->light_resources[0] = (qfv_resource_t) {
		.name = "lights",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 + 3 + 6 * frames,
		.objects = (qfv_resobj_t *) &lctx->light_resources[1],
	};
	auto splat_verts = lctx->light_resources->objects;
	auto splat_inds = &splat_verts[1];
	auto default_map = &splat_inds[1];
	auto default_view_cube = &default_map[1];
	auto default_view_2d = &default_view_cube[1];
	auto light_entids = &default_view_2d[1];
	auto light_ids = &light_entids[frames];
	auto light_data = &light_ids[frames];
	auto light_render = &light_data[frames];
	auto light_styles = &light_render[frames];
	auto light_mats = &light_styles[frames];
	splat_verts[0] = (qfv_resobj_t) {
		.name = "splat:vertices",
		.type = qfv_res_buffer,
		.buffer = {
			.size = (20 + 7) * sizeof (vec3_t),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		},
	};
	splat_inds[0] = (qfv_resobj_t) {
		.name = "splat:indices",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (ico_inds) + sizeof (cone_inds),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		},
	};
	default_map[0] = (qfv_resobj_t) {
		.name = "default_map",
		.type = qfv_res_image,
		.image = {
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.type = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_X8_D24_UNORM_PACK32,
			.extent = { 64, 64, 1 },
			.num_mipmaps = 1,
			.num_layers = 6,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
					| VK_IMAGE_USAGE_SAMPLED_BIT,
		},
	};
	default_view_cube[0] = (qfv_resobj_t) {
		.name = "default_map:view_cube",
		.type = qfv_res_image_view,
		.image_view = {
			.image = default_map - lctx->light_resources->objects,
			.type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
			.format = VK_FORMAT_X8_D24_UNORM_PACK32,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
		},
	};
	default_view_2d[0] = (qfv_resobj_t) {
		.name = "default_map:view_2d",
		.type = qfv_res_image_view,
		.image_view = {
			.image = default_map - lctx->light_resources->objects,
			.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = VK_FORMAT_X8_D24_UNORM_PACK32,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
		},
	};
	for (size_t i = 0; i < frames; i++) {
		light_entids[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "entids:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = MaxLights * sizeof (uint32_t),
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_ids[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "ids:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = MaxLights * sizeof (uint32_t),
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_data[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "lights:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (light_t[MaxLights]),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_render[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "render:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_light_render_t[MaxLights]),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_styles[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "styles:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (vec4f_t[NumStyles]),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_mats[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "matrices:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				// never need more than 6 matrices per light
				.size = sizeof (mat4f_t[MaxLights * 6]),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
	}

	QFV_CreateResource (device, lctx->light_resources);

	lctx->splat_verts = splat_verts[0].buffer.buffer;
	lctx->splat_inds = splat_inds[0].buffer.buffer;
	lctx->default_map = default_map[0].image.image;
	lctx->default_view_cube = default_view_cube[0].image_view.view;
	lctx->default_view_2d = default_view_2d[0].image_view.view;

	auto shadow_mgr = QFV_Render_DSManager (ctx, "lighting_shadow");
	lctx->shadow_cube_set = QFV_DSManager_AllocSet (shadow_mgr);
	lctx->shadow_2d_set = QFV_DSManager_AllocSet (shadow_mgr);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
						 lctx->shadow_cube_set, "lighting:shadow_cube_set");
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
						 lctx->shadow_2d_set, "lighting:shadow_2d_set");
	lctx->shadow_sampler = QFV_Render_Sampler (ctx, "shadow_sampler");

	auto attach_mgr = QFV_Render_DSManager (ctx, "lighting_attach");
	auto lights_mgr = QFV_Render_DSManager (ctx, "lighting_lights");
	auto shadowmat_mgr = QFV_Render_DSManager (ctx, "shadowmat_set");
	for (size_t i = 0; i < frames; i++) {
		auto lframe = &lctx->frames.a[i];
		*lframe = (lightingframe_t) {
			.shadowmat_set = QFV_DSManager_AllocSet (shadowmat_mgr),
			.lights_set = QFV_DSManager_AllocSet (lights_mgr),
			.attach_set = QFV_DSManager_AllocSet (attach_mgr),
			.shadowmat_buffer = light_mats[i].buffer.buffer,
			.light_buffer = light_data[i].buffer.buffer,
			.render_buffer = light_render[i].buffer.buffer,
			.style_buffer = light_styles[i].buffer.buffer,
			.id_buffer = light_ids[i].buffer.buffer,
			.entid_buffer = light_entids[i].buffer.buffer,
		};

		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 lframe->attach_set,
							 va (ctx->va_ctx, "lighting:attach_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 lframe->lights_set,
							 va (ctx->va_ctx, "lighting:lights_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET,
							 lframe->shadowmat_set,
							 va (ctx->va_ctx, "lighting:shadowmat_set:%zd", i));

		lframe->views = (qfv_imageviewset_t) DARRAY_STATIC_INIT (16);
		lframe->framebuffers = (qfv_framebufferset_t) DARRAY_STATIC_INIT (16);

		VkDescriptorBufferInfo bufferInfo[] = {
			{	.buffer = lframe->shadowmat_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->id_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->light_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->render_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->style_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->entid_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
		};
		VkWriteDescriptorSet bufferWrite[] = {
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->shadowmat_set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[0], },

			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[1], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[2], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[3], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[4], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 4,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[5], },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 6, bufferWrite, 0, 0);
	}

	make_default_map (64, lctx->default_map, ctx);

	auto packet = QFV_PacketAcquire (ctx->staging);
	make_ico (packet);
	make_cone (packet);
	QFV_PacketCopyBuffer (packet, splat_verts[0].buffer.buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);
	packet = QFV_PacketAcquire (ctx->staging);
	write_inds (packet);
	QFV_PacketCopyBuffer (packet, splat_inds[0].buffer.buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);

	qfvPopDebug (ctx);
}

static void
clear_shadows (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	lightingctx_t *lctx = ctx->lighting_context;

	if (lctx->shadow_resources) {
		QFV_DestroyResource (device, lctx->shadow_resources);
		free (lctx->shadow_resources);
		lctx->shadow_resources = 0;
	}
	free (lctx->map_images);
	free (lctx->map_views);
	free (lctx->map_cube);
	lctx->map_images = 0;
	lctx->map_views = 0;
	lctx->map_cube = 0;
	lctx->num_maps = 0;
	lctx->light_control.size = 0;
}

void
Vulkan_Lighting_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	lightingctx_t *lctx = ctx->lighting_context;

	clear_shadows (ctx);

	QFV_DestroyResource (device, lctx->light_resources);
	free (lctx->light_resources);

	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		clear_frame_buffers_views (ctx, lframe);
		DARRAY_CLEAR (&lframe->views);
		DARRAY_CLEAR (&lframe->framebuffers);
	}

	DARRAY_CLEAR (&lctx->light_mats);
	DARRAY_CLEAR (&lctx->light_control);
	free (lctx->map_images);
	free (lctx->map_views);
	free (lctx->map_cube);
	free (lctx->frames.a);
	free (lctx);
}

static vec4f_t  ref_direction = { 1, 0, 0, 0 };

static void
create_light_matrices (lightingctx_t *lctx)
{
	auto reg = lctx->scene->reg;
	auto light_pool = &reg->comp_pools[scene_light];
	auto light_data = (light_t *) light_pool->data;
	uint16_t mat_count = 0;
	for (uint32_t i = 0; i < light_pool->count; i++) {
		entity_t    ent = { .reg = reg, .id = light_pool->dense[i] };
		uint32_t    id = get_lightid (ent);
		auto        r = &lctx->light_control.a[id];
		r->matrix_id = mat_count;
		mat_count += r->numLayers;
	}
	DARRAY_RESIZE (&lctx->light_mats, mat_count);
	lctx->dynamic_matrix_base = mat_count;
	for (uint32_t i = 0; i < dynlight_max; i++) {
		auto r = &lctx->light_control.a[lctx->dynamic_base + i];
		r->matrix_id = lctx->dynamic_matrix_base + i * 6;
	}
	for (uint32_t i = 0; i < light_pool->count; i++) {
		light_t    *light = &light_data[i];
		entity_t    ent = { .reg = reg, .id = light_pool->dense[i] };
		uint32_t    id = get_lightid (ent);
		auto        r = &lctx->light_control.a[id];
		auto        lm = &lctx->light_mats.a[r->matrix_id];
		mat4f_t     view;
		mat4f_t     proj;

		switch (r->mode) {
			default:
			case ST_NONE:
				continue;
			case ST_CUBE:
				mat4fidentity (view);
				break;
			case ST_CASCADE:
			case ST_PLANE:
				//FIXME will fail for -ref_direction
				vec4f_t     dir = light->direction;
				dir[3] = 0;
				mat4fquat (view, qrotf (dir, ref_direction));
				break;
		}
		vec4f_t pos = -light->position;
		pos[3] = 1;
		view[3] = mvmulf (view, pos);

		switch (r->mode) {
			case ST_NONE:
				continue;
			case ST_CUBE:
				QFV_PerspectiveTan (proj, 1, 1);
				for (int j = 0; j < 6; j++) {
					mat4f_t side_view;
					mat4f_t rotinv;
					mat4ftranspose (rotinv, qfv_box_rotations[j]);
					mmulf (side_view, rotinv, view);
					mmulf (side_view, qfv_z_up, side_view);
					mmulf (lm[j], proj, side_view);
				}
				break;
			case ST_CASCADE:
				// dependent on view fustrum and cascade level
				mat4fidentity (proj);
				mmulf (view, qfv_z_up, view);
				for (int j = 0; j < 4; j++) {
					mmulf (lm[j], proj, view);
				}
				break;
			case ST_PLANE:
				QFV_PerspectiveCos (proj, -light->direction[3]);
				mmulf (view, qfv_z_up, view);
				mmulf (lm[0], proj, view);
				break;
		}
	}
}

static void
upload_light_matrices (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	auto packet = QFV_PacketAcquire (ctx->staging);
	size_t mat_size = sizeof (mat4f_t[lctx->light_mats.size]);
	void *mat_data = QFV_PacketExtend (packet, mat_size);
	memcpy (mat_data, lctx->light_mats.a, mat_size);
	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		QFV_PacketCopyBuffer (packet, lframe->shadowmat_buffer, 0, bb);
	}
	QFV_PacketSubmit (packet);
}

static void
upload_light_data (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	auto reg = lctx->scene->reg;
	auto light_pool = &reg->comp_pools[scene_light];
	auto lights = (light_t *) light_pool->data;
	uint32_t count = light_pool->count;

	auto packet = QFV_PacketAcquire (ctx->staging);
	auto light_data = QFV_PacketExtend (packet, sizeof (light_t[count]));
	memcpy (light_data, lights, sizeof (light_t[count]));
	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		QFV_PacketCopyBuffer (packet, lframe->light_buffer, 0, bb);
	}
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (ctx->staging);
	uint32_t r_size = sizeof (qfv_light_render_t[count]);
	qfv_light_render_t *render = QFV_PacketExtend (packet, r_size);
	for (uint32_t i = 0; i < count; i++) {
		entity_t    ent = { .reg = reg, .id = light_pool->dense[i] };
		uint32_t    id = get_lightid (ent);
		if (id >= lctx->light_control.size) {
			continue;
		}
		auto        r = &lctx->light_control.a[id];
		render[i] = (qfv_light_render_t) {
			.id_data = make_id(r->matrix_id, r->map_index, r->layer, r->mode),
			.style = get_lightstyle (ent),
		};
	}
	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		QFV_PacketCopyBuffer (packet, lframe->render_buffer, 0, bb);
	}
	QFV_PacketSubmit (packet);
}

static int
light_shadow_type (const light_t *light)
{
	if (!light->position[3]) {
		if (!VectorIsZero (light->direction)) {
			return ST_CASCADE;
		}
	} else {
		if (light->direction[3] > -0.5) {
			return ST_CUBE;
		} else {
			return ST_PLANE;
		}
	}
	return ST_NONE;
}

static int
light_compare (const void *_li2, const void *_li1, void *_lights)
{
	const int *li1 = _li1;
	const int *li2 = _li2;
	const light_t *lights = _lights;
	const light_t *l1 = &lights[*li1];
	const light_t *l2 = &lights[*li2];
	int         s1 = abs ((int) l1->color[3]);
	int         s2 = abs ((int) l2->color[3]);

	if (s1 == s2) {
		if (l1->position[3] == l2->position[3]) {
			return (l2->direction[3] > -0.5) - (l1->direction[3] > -0.5);
		}
		return l2->position[3] - l1->position[3];
	}
	return s1 - s2;
}

typedef struct {
	int         size;
	int         layers;
	int         cube;
} mapdesc_t;

typedef struct {
	mapdesc_t  *maps;
	int         numMaps;
	int         numLights;
	const light_t *lights;
	int        *imageMap;
	const int  *lightMap;
	light_control_t *control;
	int         maxLayers;
} mapctx_t;

static int
allocate_map (mapctx_t *mctx, int type, int (*getsize) (const light_t *light))
{
	int         size = -1;
	int         numLayers = 0;
	int         totalLayers = 0;
	int         layers = ((int[4]) { 0, 1, 4, 6 })[type];
	int         cube = type == ST_CUBE;

	for (int i = 0; i < mctx->numLights; i++) {
		auto li = mctx->lightMap[i];
		auto lr = &mctx->control[li];

		if (lr->mode != type) {
			continue;
		}
		int light_size = getsize (&mctx->lights[li]);
		if (size != light_size || numLayers + layers > mctx->maxLayers) {
			if (numLayers) {
				mctx->maps[mctx->numMaps++] = (mapdesc_t) {
					.size = size,
					.layers = numLayers,
					.cube = cube,
				};
				numLayers = 0;
			}
			size = light_size;
		}
		mctx->imageMap[li] = mctx->numMaps;
		lr->size = size;
		lr->layer = numLayers;
		lr->numLayers = layers;
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		mctx->maps[mctx->numMaps++] = (mapdesc_t) {
			.size = size,
			.layers = numLayers,
			.cube = cube,
		};
	}
	return totalLayers;
}

static int
allocate_dynlight_map (mapctx_t *mctx)
{
	int         size = -1;
	int         numLayers = 0;
	int         totalLayers = 0;
	int         layers = 6;
	int         cube = 1;

	for (int i = 0; i < dynlight_max; i++) {
		if (size != dynlight_size || numLayers + layers > mctx->maxLayers) {
			if (numLayers) {
				mctx->maps[mctx->numMaps++] = (mapdesc_t) {
					.size = size,
					.layers = numLayers,
					.cube = cube,
				};
				numLayers = 0;
			}
			size = dynlight_size;
		}

		auto li = mctx->numLights + i;
		auto lr = &mctx->control[li];

		*lr = (light_control_t) {
			.renderpass_index = 2,
			.map_index = mctx->numMaps,
			.size = size,
			.layer = numLayers,
			.numLayers = layers,
			.mode = ST_CUBE,
			.light_id = li,
		};
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		mctx->maps[mctx->numMaps++] = (mapdesc_t) {
			.size = size,
			.layers = numLayers,
			.cube = cube,
		};
	}
	return totalLayers;
}

static int
get_point_size (const light_t *light)
{
	return abs ((int) light->color[3]);
}

static int
get_spot_size (const light_t *light)
{
	float c = light->direction[3];
	float s = sqrt (1 - c * c);
	return abs ((int) (s * light->color[3]));
}

static int
get_direct_size (const light_t *light)
{
	return 1024;
}

static void
build_shadow_maps (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{

	qfv_device_t *device = ctx->device;
	qfv_physdev_t *physDev = device->physDev;
	int         maxLayers = physDev->properties->limits.maxImageArrayLayers;
	if (maxLayers > 2048) {
		maxLayers = 2048;
	}
	auto reg = lctx->scene->reg;
	auto light_pool = &reg->comp_pools[scene_light];
	auto lights = (light_t *) light_pool->data;
	int         numLights = light_pool->count;
	int         totalLayers = 0;
	int         imageMap[numLights];
	int         lightMap[numLights];
	mapdesc_t   maps[numLights];

	for (int i = 0; i < numLights; i++) {
		lightMap[i] = i;
	}
	heapsort_r (lightMap, numLights, sizeof (int), light_compare, lights);

	DARRAY_RESIZE (&lctx->light_control, numLights + dynlight_max);
	for (int i = 0; i < numLights; i++) {
		auto li = lightMap[i];
		auto lr = &lctx->light_control.a[li];
		*lr = (light_control_t) {
			.mode = light_shadow_type (&lights[li]),
			.light_id = li,
		};
		set_lightid (light_pool->dense[li], reg, li);
		// assume all lights have no shadows
		imageMap[li] = -1;
	}

	mapctx_t mctx = {
		.maps = maps,
		.numLights = numLights,
		.lights = lights,
		.imageMap = imageMap,
		.lightMap = lightMap,
		.control = lctx->light_control.a,
		.maxLayers = maxLayers,
	};
	totalLayers += allocate_map (&mctx, ST_CUBE, get_point_size);
	totalLayers += allocate_map (&mctx, ST_PLANE, get_spot_size);
	totalLayers += allocate_map (&mctx, ST_CASCADE, get_direct_size);

	totalLayers += allocate_dynlight_map (&mctx);

	lctx->num_maps = mctx.numMaps;
	if (mctx.numMaps) {
		qfv_resource_t *shad = calloc (1, sizeof (qfv_resource_t)
									   + sizeof (qfv_resobj_t[mctx.numMaps])
									   + sizeof (qfv_resobj_t[mctx.numMaps]));
		lctx->shadow_resources = shad;
		*shad = (qfv_resource_t) {
			.name = "shadow",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = 2 * mctx.numMaps,
			.objects = (qfv_resobj_t *) &shad[1],
		};
		lctx->map_images = malloc (sizeof (VkImage[mctx.numMaps]));
		lctx->map_views = malloc (sizeof (VkImageView[mctx.numMaps]));
		lctx->map_cube = malloc (sizeof (bool[mctx.numMaps]));
		auto images = shad->objects;
		auto views = &images[mctx.numMaps];
		for (int i = 0; i < mctx.numMaps; i++) {
			int         cube = maps[i].cube;
			lctx->map_cube[i] = cube;
			images[i] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "map:image:%d:%d", i, maps[i].size),
				.type = qfv_res_image,
				.image = {
					.flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
					.type = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_X8_D24_UNORM_PACK32,
					.extent = { maps[i].size, maps[i].size, 1 },
					.num_mipmaps = 1,
					.num_layers = maps[i].layers,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
							| VK_IMAGE_USAGE_SAMPLED_BIT,
				},
			};
			views[i] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "map:view:%d:%d", i, maps[i].size),
				.type = qfv_res_image_view,
				.image_view = {
					.image = i,
					.type = cube ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
								 : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
					.format = VK_FORMAT_X8_D24_UNORM_PACK32,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					},
				},
			};
		}
		QFV_CreateResource (device, shad);
		for (int i = 0; i < mctx.numMaps; i++) {
			lctx->map_images[i] = images[i].image.image;
			lctx->map_views[i] = views[i].image_view.view;
		}
	}

	for (int i = 0; i < numLights; i++) {
		int  li = lightMap[i];
		auto lr = &lctx->light_control.a[li];

		if (imageMap[li] == -1) {
			continue;
		}

		switch (lr->numLayers) {
			case 6:
				lr->renderpass_index = 2;
				break;
			case 4:
				lr->renderpass_index = 1;
				break;
			case 1:
				lr->renderpass_index = 0;
				break;
			default:
				Sys_Error ("build_shadow_maps: invalid light layer count: %u",
						   lr->numLayers);
		}
		lr->map_index = imageMap[li];
	}
	Sys_MaskPrintf (SYS_lighting,
					"shadow maps: %d layers in %d images: %"PRId64"\n",
					totalLayers, lctx->num_maps,
					lctx->shadow_resources ? lctx->shadow_resources->size
										   : (VkDeviceSize) 0);
}

static void
transition_shadow_maps (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkCommandBufferAllocateInfo aInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ctx->cmdpool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer cmd;
	dfunc->vkAllocateCommandBuffers (device->dev, &aInfo, &cmd);
	VkCommandBufferBeginInfo bInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	dfunc->vkBeginCommandBuffer (cmd, &bInfo);

	auto ib = imageBarriers[qfv_LT_Undefined_to_ShaderReadOnly];
	ib.barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	VkImageMemoryBarrier barriers[lctx->num_maps];
	for (int i = 0; i < lctx->num_maps; i++) {
		barriers[i] = ib.barrier;
		barriers[i].image = lctx->map_images[i];
	}
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, lctx->num_maps, barriers);
	dfunc->vkEndCommandBuffer (cmd);

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};
	dfunc->vkQueueSubmit (device->queue.queue, 1, &submitInfo, 0);
}

static void
update_shadow_descriptors (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;

	VkDescriptorImageInfo imageInfoCube[32];
	VkDescriptorImageInfo imageInfo2d[32];
	for (int i = 0; i < 32; i++) {
		VkImageView viewCube = lctx->default_view_cube;
		VkImageView view2d = lctx->default_view_2d;
		if (i < lctx->num_maps) {
			if (lctx->map_cube[i]) {
				viewCube = lctx->map_views[i];
			} else {
				view2d = lctx->map_views[i];
			}
		}
		imageInfoCube[i] = (VkDescriptorImageInfo) {
			.sampler = lctx->shadow_sampler,
			.imageView = viewCube,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		imageInfo2d[i] = (VkDescriptorImageInfo) {
			.sampler = lctx->shadow_sampler,
			.imageView = view2d,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
	}
	VkWriteDescriptorSet imageWrite[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lctx->shadow_cube_set,
			.dstBinding = 0,
			.descriptorCount = 32,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = imageInfoCube,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = lctx->shadow_2d_set,
			.dstBinding = 0,
			.descriptorCount = 32,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = imageInfo2d,
		},
	};
	dfunc->vkUpdateDescriptorSets (device->dev, 2, imageWrite, 0, 0);
}

static void
mark_leaves (bsp_pass_t *pass, set_t *pvs)
{
	visstate_t visstate = {
		.node_visframes = pass->node_frames,
		.leaf_visframes = pass->leaf_frames,
		.face_visframes = pass->face_frames,
		.visframecount = pass->vis_frame,
		.brush = pass->brush,
	};
	R_MarkLeavesPVS (&visstate, pvs);
	pass->vis_frame = visstate.visframecount;
}

static void
show_leaves (vulkan_ctx_t *ctx, uint32_t leafnum, efrag_t *efrags)
{
	auto pass = Vulkan_Bsp_GetPass (ctx, QFV_bspDebug);
	auto brush = pass->brush;

	set_t pvs = SET_STATIC_INIT (brush->visleafs, alloca);
	set_empty (&pvs);
	if (leafnum) {
		set_add (&pvs, leafnum - 1);
	} else {
		for (auto e = efrags; e; e = e->entnext) {
			set_add (&pvs, e->leaf - brush->leafs - 1);
		}
	}
	mark_leaves (pass, &pvs);
}

static void
scene_efrags_ui (void *comp, imui_ctx_t *imui_ctx,
				 ecs_registry_t *reg, uint32_t ent, void *data)
{
	vulkan_ctx_t *ctx = data;
	auto efrags = *(efrag_t **) comp;
	uint32_t len = 0;
	bool valid = true;
	for (auto e = efrags; e; e = e->entnext, len++) {
		valid &= e->entity.id == ent;
	}
	UI_Horizontal {
		if (UI_Button (va (ctx->va_ctx, "Show##lightefrags_ui.%08x", ent))) {
			show_leaves (ctx, 0, efrags);
		}
		UI_FlexibleSpace ();
		UI_Labelf ("%4s %5u", valid ? "good" : "bad", len);
	}
}

static void
scene_lightleaf_ui (void *comp, imui_ctx_t *imui_ctx,
					ecs_registry_t *reg, uint32_t ent, void *data)
{
	vulkan_ctx_t *ctx = data;
	auto leaf = *(uint32_t *) comp;
	UI_Horizontal {
		if (UI_Button (va (ctx->va_ctx, "Show##lightleaf_ui.%08x", ent))) {
			show_leaves (ctx, leaf, 0);
		}
		UI_FlexibleSpace ();
		UI_Labelf ("%5u", leaf);

		auto pass = Vulkan_Bsp_GetPass (ctx, QFV_bspDebug);
		auto brush = pass->brush;
		set_t pvs = SET_STATIC_INIT (brush->visleafs, alloca);
		Mod_LeafPVS_set (brush->leafs + leaf, brush, 0, &pvs);

		UI_FlexibleSpace ();
		if (UI_Button (va (ctx->va_ctx, "Vis##lightleaf_ui.%08x", ent))) {
			mark_leaves (pass, &pvs);
		}
		UI_FlexibleSpace ();
		UI_Labelf ("%5u", set_count (&pvs));
	}
}

void
Vulkan_LoadLights (scene_t *scene, vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = ctx->lighting_context;

	lctx->scene = scene;

	clear_shadows (ctx);

	lctx->ldata = 0;
	if (lctx->scene) {
		auto reg = lctx->scene->reg;
		reg->components.a[scene_efrags].ui = scene_efrags_ui;
		reg->components.a[scene_lightleaf].ui = scene_lightleaf_ui;

		auto light_pool = &reg->comp_pools[scene_light];
		if (light_pool->count) {
			lctx->dynamic_base = light_pool->count;
			lctx->dynamic_count = 0;
			build_shadow_maps (lctx, ctx);
			transition_shadow_maps (lctx, ctx);
			update_shadow_descriptors (lctx, ctx);
			create_light_matrices (lctx);
			upload_light_matrices (lctx, ctx);
			upload_light_data (lctx, ctx);
		}
		lctx->ldata = scene->lights;
	}
}

VkDescriptorSet
Vulkan_Lighting_Descriptors (vulkan_ctx_t *ctx, int frame)
{
	auto lctx = ctx->lighting_context;
	return lctx->frames.a[frame].shadowmat_set;
}
