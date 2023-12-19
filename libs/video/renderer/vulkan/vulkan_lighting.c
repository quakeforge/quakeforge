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
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
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

#define shadow_quanta 128
#define lnearclip 4
#define num_cascade 4
#define max_views 29	// FIXME should be 32 (or really, maxMultiviewViewCount,
						// but there are other problems there), but nvidia's
						// drivers segfault for > 29

static vec4f_t  ref_direction = { 1, 0, 0, 0 };

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
	.default_value = "256",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &dynlight_size },
};

static const light_t *
get_light (uint32_t ent, ecs_registry_t *reg)
{
	return Ent_GetComponent (ent, scene_light, reg);
}

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
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;
	auto lframe = &lctx->frames.a[ctx->curFrame];

	if (!lctx->ldata) {
		return;
	}
	auto pass = Vulkan_Bsp_GetPass (ctx, QFV_bspShadow);
	auto brush = pass->brush;

	if (brush->submodels) {
		lctx->world_mins = loadvec3f (brush->submodels[0].mins);
		lctx->world_maxs = loadvec3f (brush->submodels[0].maxs);
	} else {
		// FIXME better bounds
		lctx->world_mins = (vec4f_t) { -512, -512, -512, 0 };
		lctx->world_maxs = (vec4f_t) {  512,  512,  512, 0 };
	}
	set_t leafs = SET_STATIC_INIT (brush->modleafs, alloca);
	set_empty (&leafs);

	for (int i = 0; i < ST_COUNT; i++) {
		auto q = lframe->light_queue[i];
		for (uint32_t j = 0; j < q.count; j++) {
			uint32_t leafnum = lframe->id_radius[q.start + j].leafnum;
			if (leafnum != ~0u) {
				set_add (&leafs, leafnum);
			}
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

static VkFramebuffer
create_framebuffer (vulkan_ctx_t *ctx, int size,
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
			.width = size,
			.height = size,
			.layers = 1,
		}, 0, &framebuffer);
	return framebuffer;
}

static void
copy_maps (uint32_t start, uint32_t count, int stage_id,
		   lightingframe_t *lframe, lightingctx_t *lctx, vulkan_ctx_t *ctx,
		   qfv_taskctx_t *taskctx)
{
	qfZoneScoped (true);
	auto device = ctx->device;
	auto dfunc = device->funcs;

	int num_regions = 0;
	int num_copies = 0;
	uint32_t last = ~0u;
	for (uint32_t j = 0; j < count; j++) {
		auto tgt = lframe->stage_targets[start + j];
		// if the map id is different or if the layers aren't sequential
		if (tgt != last + 0x20) {
			num_regions++;
			if ((tgt & 0x1f) != (last & 0x1f)) {
				num_copies++;
			}
		}
		last = tgt;
	}

	auto ib = imageBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	ib.barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = 0;

	VkImageMemoryBarrier barriers[num_regions];
	VkImageCopy2 regions[num_regions];
	VkCopyImageInfo2 copies[num_copies];
	num_regions = 0;
	num_copies = 0;
	last = ~0u;
	for (uint32_t j = 0; j < count; j++) {
		auto tgt = lframe->stage_targets[start + j];
		int ind = num_regions - 1;
		// if the map id is different or if the layers aren't sequential
		if (tgt != last + 0x20) {
			int cpind = num_copies - 1;
			if ((tgt & 0x1f) != (last & 0x1f)) {
				cpind = num_copies++;
				copies[cpind] = (VkCopyImageInfo2) {
					.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
					.srcImage = lctx->stage_images[stage_id],
					.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.dstImage = lctx->map_images[tgt & 0x1f],
					.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.pRegions = &regions[num_regions],
				};
			}
			copies[cpind].regionCount++;

			ind = num_regions++;

			barriers[ind] = ib.barrier;
			barriers[ind].subresourceRange.baseArrayLayer = tgt >> 5;
			barriers[ind].image = lctx->map_images[tgt & 0x1f];

			regions[ind] = (VkImageCopy2) {
				.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
				.srcSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseArrayLayer = j,
				},
				.dstSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseArrayLayer = tgt >> 5,
				},
				.extent = {
					.width = (stage_id + 1) * shadow_quanta,
					.height = (stage_id + 1) * shadow_quanta,
					.depth = 1,
				},
			};
		}
		barriers[ind].subresourceRange.layerCount++;
		regions[ind].srcSubresource.layerCount++;
		regions[ind].dstSubresource.layerCount++;
		last = tgt;
	}

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	dfunc->vkBeginCommandBuffer (cmd, &(VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});
	{
		qftVkScopedZoneC (taskctx->frame->qftVkCtx, cmd, "copy stage",
						  0xa0c060);
		for (int i = 0; i < num_copies; i++) {
			dfunc->vkCmdCopyImage2 (cmd, &copies[i]);
		}
		dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
									 0, 0, 0, 0, 0, num_regions, barriers);
	}
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);
}

static void
lighting_draw_shadow_maps (const exprval_t **params, exprval_t *result,
						   exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;
	auto shadow = QFV_GetStep (params[0], ctx->render_context->job);
	auto render = shadow->render;
	auto lframe = &lctx->frames.a[ctx->curFrame];

	if (!lctx->num_maps) {
		return;
	}

	uint32_t id_base = 0;
	for (int i = 0; i < LIGHTING_STAGES; i++) {
		auto queue = &lframe->stage_queue[i];
		int  count;
		for (int remaining = queue->count; remaining > 0; remaining -= count) {
			count = min (remaining, max_views);
			int  rpind = count - 1;
			auto renderpass = &render->renderpasses[rpind];
			auto fbuffer = lctx->stage_framebuffers[rpind][i];
			uint32_t size = (i + 1) * shadow_quanta;
			auto bi = &renderpass->beginInfo;
			if (!fbuffer) {
				auto view = lctx->stage_views[i];
				fbuffer = create_framebuffer (ctx, size, view, bi->renderPass);
				lctx->stage_framebuffers[rpind][i] = fbuffer;
			}
			bi->framebuffer = fbuffer;
			QFV_RunRenderPass (ctx, renderpass, size, size, &id_base);
			bi->framebuffer = 0;

			copy_maps (id_base, count, i, lframe, lctx, ctx, taskctx);
			id_base += count;
		}
	}
}

typedef enum : uint32_t {
	style_enable,
	style_disable = 0x80000000,
} style_e;

static uint32_t
make_id (const light_control_t *cont, style_e style)
{
	uint32_t matrix_index = cont->matrix_id;
	uint32_t map_index = cont->map_index;
	uint32_t layer = cont->layer;
	uint32_t type = cont->mode;

	if (type == ST_CUBE) {
		// on the GPU, layer is the cube layer, and one cube layer is 6
		// flat image layers
		layer /= 6;
	}
	return ((matrix_index & 0x3fff) << 0)
		 | ((map_index & 0x1f) << 14)
		 | ((layer & 0x7ff) << 19)
		 | style;
}

static void
cube_mats (mat4f_t *mat, vec4f_t position)
{
	mat4f_t     view;
	mat4fidentity (view);
	view[3] = -position;
	view[3][3] = 1;

	mat4f_t     proj;
	QFV_PerspectiveTan (proj, 1, 1, lnearclip);
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
frustum_corners (vec4f_t corners[8], float minz, float maxz,
				  const mat4f_t invproj)
{
	for (int i = 0; i < 8; i++) {
		vec4f_t p = {
			(i & 1) ? 1 : -1,
			(i & 2) ? 1 : -1,
			(i & 4) ? maxz : minz,
			1
		};
		p = mvmulf (invproj, p);
		corners[i] = p / p[3];
	}
}

static vec4f_t
vec_select (vec4i_t c, vec4f_t a, vec4f_t b)
{
	return (vec4f_t) ((c & (vec4i_t) a) | (~c & (vec4i_t) b));
}

static void
transform_corners (vec4f_t *mins, vec4f_t *maxs, const vec4f_t corners[8],
				   const mat4f_t lightview)
{
	*mins = (vec4f_t) {  INFINITY,  INFINITY,  INFINITY,  INFINITY };
	*maxs = (vec4f_t) { -INFINITY, -INFINITY, -INFINITY, -INFINITY };
	for (int i = 0; i < 8; i++) {
		vec4f_t p = mvmulf (lightview, corners[i]);
		vec4i_t tmin = p <= *mins;
		vec4i_t tmax = p >= *maxs;
		*mins = vec_select (tmin, p, *mins);
		*maxs = vec_select (tmax, p, *maxs);
	}
}

static void
cascade_mats (mat4f_t *mat, vec4f_t position, vulkan_ctx_t *ctx)
{
	auto mctx = ctx->matrix_context;
	auto lctx = ctx->lighting_context;
	mat4f_t invproj;
	QFV_InversePerspectiveTanFar (invproj, mctx->fov_x, mctx->fov_y,
								  r_nearclip, r_farclip);
	mat4f_t inv_z_up;
	mat4ftranspose (inv_z_up, qfv_z_up);
	mmulf (invproj, inv_z_up, invproj);
	mmulf (invproj, r_refdef.camera, invproj);

	mat4f_t lightview;
	// position points towards the light, but the view needs to be from the
	// light.
	mat4fquat (lightview, qrotf (-position, ref_direction));

	// Find the world corner closest to the light in order to ensure the
	// entire world between the focal point and the light is included in the
	// depth range
	vec4i_t d = position >= (vec4f_t) {0, 0, 0, 0};
	vec4f_t lightcorner = vec_select (d, lctx->world_maxs, lctx->world_mins);
	// assumes position is a unit vector (which it will be for directional
	// lights loaded from quake maps)
	float corner_dist = dotf (lightcorner-r_refdef.camera[3], position)[0];

	// Pre-swizzle the light view so it's done only once (and so the
	// frustum bounds get swizzled correctly for creating the orthographic
	// projection matrix).
	// Also, no need (actually, undesirable) to include any translation in
	// the light view matrix as the orthographic matrix setup includes
	// translation based on the bounds.
	mmulf (lightview, qfv_z_up, lightview);

	vec2f_t z_range[] = {
		{ r_nearclip / 32,   1 },
		{ r_nearclip / 256,  r_nearclip / 32 },
		{ r_nearclip / 1024, r_nearclip / 256 },
		{ 0,                 r_nearclip / 1024 },
	};
	for (int i = 0; i < num_cascade; i++) {
		vec4f_t     corners[8];
		vec4f_t     fmin, fmax;
		frustum_corners (corners, z_range[i][0], z_range[i][1], invproj);
		transform_corners (&fmin, &fmax, corners, lightview);
		// ensure evertything between the light and the far frustum corner
		// is in the depth range
		fmin[2] = min(fmin[2], -corner_dist);
		QFV_OrthographicV (mat[i], fmin, fmax);
		mmulf (mat[i], mat[i], lightview);
	}
}

static uint16_t
make_target (uint16_t map_index, uint16_t layer)
{
	return (map_index & 0x1f) | ((layer & 0x7ff) << 5);
}

static void
enqueue_map (uint32_t *ids, lightingframe_t *lframe, light_control_t *r)
{
	auto q = &lframe->stage_queue[r->stage_index];
	auto tgt = lframe->stage_targets;
	for (uint32_t i = 0; i < r->numLayers; i++) {
		ids[q->start + q->count + i] = r->matrix_id + i;
		tgt[q->start + q->count + i] = make_target (r->map_index, r->layer + i);
	}
	q->count += r->numLayers;
}

static void
transition_shadow_targets (lightingframe_t *lframe, vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	int num_barriers = 0;
	uint32_t last = ~0u;
	for (int i = 0; i < LIGHTING_STAGES; i++) {
		auto q = lframe->stage_queue[i];
		for (uint32_t j = 0; j < q.count; j++) {
			auto tgt = lframe->stage_targets[q.start + j];
			// if the map id is different or if the layers aren't sequential
			if (tgt != last + 0x20) {
				num_barriers++;
			}
			last = tgt;
		}
	}

	auto ib = imageBarriers[qfv_LT_ShaderReadOnly_to_TransferDst];
	ib.barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	ib.barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ib.barrier.subresourceRange.layerCount = 0;

	VkImageMemoryBarrier barriers[num_barriers];
	num_barriers = 0;
	last = ~0u;
	for (int i = 0; i < LIGHTING_STAGES; i++) {
		auto q = lframe->stage_queue[i];
		for (uint32_t j = 0; j < q.count; j++) {
			auto tgt = lframe->stage_targets[q.start + j];
			int ind = num_barriers - 1;
			// if the map id is different or if the layers aren't sequential
			if (tgt != last + 0x20) {
				barriers[num_barriers++] = ib.barrier;
				ind = num_barriers - 1;
				barriers[ind].subresourceRange.baseArrayLayer = tgt >> 5;
				barriers[ind].image = lctx->map_images[tgt & 0x1f];
			}
			barriers[ind].subresourceRange.layerCount++;
			last = tgt;
		}
	}

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	dfunc->vkBeginCommandBuffer (cmd, &(VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});
	dfunc->vkCmdPipelineBarrier (cmd, ib.srcStages, ib.dstStages,
								 0, 0, 0, 0, 0, num_barriers, barriers);
	dfunc->vkEndCommandBuffer (cmd);
	QFV_AppendCmdBuffer (ctx, cmd);
}

static float
light_radius (const light_t *l)
{
	return l->attenuation[3] > 0 ? 1 / l->attenuation[3]
		 : l->attenuation[0] > 0 ? sqrt(abs(l->color[3]/l->attenuation[0]))
		 : l->attenuation[1] > 0 ? abs(l->color[3]/l->attenuation[1])
		 // FIXME ambient lights. not right, but at least it will render
		 : sqrt(abs(l->color[3]));
}

static void
lighting_update_lights (const exprval_t **params, exprval_t *result,
						exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;

	auto lframe = &lctx->frames.a[ctx->curFrame];

	memset (lframe->light_queue, 0, sizeof (lframe->light_queue));
	if (!lctx->scene || !lctx->scene->lights) {
		return;
	}

	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];

	uint32_t light_ids[ST_COUNT][MaxLights];
	float    light_radii[ST_COUNT][MaxLights];
	uint32_t light_leafs[ST_COUNT][MaxLights];
	vec4f_t  light_positions[ST_COUNT][MaxLights];
	uint32_t entids[ST_COUNT][MaxLights];

	uint32_t light_count = 0;
	auto queue = lframe->light_queue;

	uint32_t dynamic_light_entities[MaxLights];
	uint32_t dynamic_light_leafs[MaxLights];
	const dlight_t *dynamic_lights[MaxLights];
	int ndlight = 0;

	auto entqueue = r_ent_queue;   //FIXME fetch from scene
	for (size_t i = 0; i < entqueue->ent_queues[mod_light].size; i++) {
		entity_t    ent = entqueue->ent_queues[mod_light].a[i];
		if (has_dynlight (ent)) {
			dynamic_light_entities[ndlight] = ent.id;
			dynamic_light_leafs[ndlight] = get_lightleaf (ent);
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
		auto r = &lctx->light_control.a[id];

		int mode =  r->mode;
		auto light = get_light (ent.id, ent.reg);
		uint32_t ind = queue[mode].count++;
		light_ids[mode][ind] = id;
		light_radii[mode][ind] = light_radius (light);
		light_leafs[mode][ind] = get_lightleaf (ent);
		light_positions[mode][ind] = light->position;
		entids[mode][ind] = ent.id;
	}

	size_t      packet_size = 0;
	packet_size += sizeof (vec4f_t[NumStyles]);
	if (queue[ST_CASCADE].count) {
		uint32_t mat_count = queue[ST_CASCADE].count * num_cascade;
		packet_size += sizeof (mat4f_t[mat_count]);
	}
	if (ndlight) {
		packet_size += sizeof (mat4f_t[ndlight * 6]);
		packet_size += sizeof (light_t[ndlight]);
		packet_size += sizeof (qfv_light_render_t[ndlight]);
	}
	if (light_count) {
		// light ids
		packet_size += sizeof (uint32_t[light_count]);
		// light radii
		packet_size += sizeof (float[light_count]);
		// ent ids
		packet_size += sizeof (uint32_t[light_count]);
	}

	auto packet = QFV_PacketAcquire (ctx->staging);
	byte *packet_start = QFV_PacketExtend (packet, packet_size);
	byte *packet_data = packet_start;

	qfv_scatter_t style_scatter = {
		.srcOffset = 0,
		.dstOffset = 0,
		.length = sizeof (vec4f_t[NumStyles]),
	};
	auto styles = (vec4f_t *) packet_data;
	packet_data += style_scatter.length;
	for (int i = 0; i < NumStyles; i++) {
		styles[i] = (vec4f_t) { 1, 1, 1, d_lightstylevalue[i] / 65536.0};
	}
	QFV_PacketScatterBuffer (packet, lframe->style_buffer,
							 1, &style_scatter, bb);

	if (queue[ST_CASCADE].count) {
		uint32_t mat_count = queue[ST_CASCADE].count * num_cascade;
		auto mats = (mat4f_t *) packet_data;
		auto base = packet_data - packet_start;
		packet_data += sizeof (mat4f_t[mat_count]);
		qfv_scatter_t scatter[queue[ST_CASCADE].count];
		for (uint32_t i = 0; i < queue[ST_CASCADE].count; i++) {
			auto r = &lctx->light_control.a[light_ids[ST_CASCADE][i]];
			auto light = get_light (entids[ST_CASCADE][i], lctx->scene->reg);
			cascade_mats (&mats[i * num_cascade], light->position, ctx);
			scatter[i] = (qfv_scatter_t) {
				.srcOffset = base + sizeof (mat4f_t[i * num_cascade]),
				.dstOffset = sizeof (mat4f_t[r->matrix_id]),
				.length = sizeof (mat4f_t[num_cascade]),
			};
		}
		QFV_PacketScatterBuffer (packet, lframe->shadowmat_buffer,
								 queue[ST_CASCADE].count, scatter, bb);
	}

	if (ndlight) {
		light_count += ndlight;

		auto mats = (mat4f_t *) packet_data;
		qfv_scatter_t mat_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = sizeof (mat4f_t[lctx->dynamic_matrix_base]),
			.length = sizeof (mat4f_t[ndlight * 6]),
		};
		packet_data += mat_scatter.length;
		for (int i = 0; i < ndlight; i++) {
			cube_mats (&mats[i * 6], dynamic_lights[i]->origin);
		}
		QFV_PacketScatterBuffer (packet, lframe->shadowmat_buffer,
								 1, &mat_scatter, bb);

		auto lights = (light_t *) packet_data;
		qfv_scatter_t light_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = sizeof (light_t[lctx->dynamic_base]),
			.length = sizeof (light_t[ndlight]),
		};
		packet_data += light_scatter.length;
		for (int i = 0; i < ndlight; i++) {
			light_t light = {
				.color = {
					VectorExpand (dynamic_lights[i]->color),
					// dynamic lights seem a tad faint, so 16x map lights
					dynamic_lights[i]->radius / 16,
				},
				// dlights are local point sources
				.position = { VectorExpand (dynamic_lights[i]->origin), 1 },
				// full sphere, normal light (not ambient)
				.direction = { 0, 0, 1, 1 },
				.attenuation = { 0, 0, 1, 1/dynamic_lights[i]->radius },
			};
			uint32_t id = lctx->dynamic_base + i;
			set_lightid (dynamic_light_entities[i], lctx->scene->reg, id);
			uint32_t ind = queue[ST_CUBE].count++;
			light_ids[ST_CUBE][ind] = id;
			light_radii[ST_CUBE][ind] = light_radius (&light);
			light_leafs[ST_CUBE][ind] = dynamic_light_leafs[i];
			light_positions[ST_CUBE][ind] = light.position;
			entids[ST_CUBE][ind] = dynamic_light_entities[i];

			lights[i] = light;
		}
		QFV_PacketScatterBuffer (packet, lframe->light_buffer,
								 1, &light_scatter, bb);

		auto render = (qfv_light_render_t *) packet_data;
		qfv_scatter_t render_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = sizeof (qfv_light_render_t[lctx->dynamic_base]),
			.length = sizeof (qfv_light_render_t[ndlight]),
		};
		packet_data += render_scatter.length;
		for (int i = 0; i < ndlight; i++) {
			auto r = &lctx->light_control.a[lctx->dynamic_base + i];
			render[i] = (qfv_light_render_t) {
				.id_data = make_id(r, style_disable),
			};
		}
		QFV_PacketScatterBuffer (packet, lframe->render_buffer,
								 1, &render_scatter, bb);
	}
	if (developer & SYS_lighting) {
		Vulkan_Draw_String (vid.width - 32, 8,
							va (ctx->va_ctx, "%3d", light_count),
							ctx);
	}

	if (light_count) {
		for (int i = 1; i < ST_COUNT; i++) {
			queue[i].start = queue[i - 1].start + queue[i - 1].count;
		}

		auto lids = (uint32_t *) packet_data;
		qfv_scatter_t lid_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = 0,
			.length = sizeof (uint32_t[light_count]),
		};
		packet_data += lid_scatter.length;
		for (int i = 0; i < ST_COUNT; i++) {
			memcpy (lids + queue[i].start, light_ids[i],
					sizeof (uint32_t[queue[i].count]));
		}
		QFV_PacketScatterBuffer (packet, lframe->id_buffer,
								 1, &lid_scatter,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);

		auto lradii = (float *) packet_data;
		qfv_scatter_t lradius_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = 0,
			.length = sizeof (float[light_count]),
		};
		packet_data += lradius_scatter.length;
		for (int i = 0; i < ST_COUNT; i++) {
			memcpy (lradii + queue[i].start, light_radii[i],
					sizeof (float[queue[i].count]));
		}
		QFV_PacketScatterBuffer (packet, lframe->radius_buffer,
								 1, &lradius_scatter,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);

		auto eids = (uint32_t *) packet_data;
		qfv_scatter_t eid_scatter = {
			.srcOffset = packet_data - packet_start,
			.dstOffset = 0,
			.length = sizeof (uint32_t[light_count]),
		};
		packet_data += eid_scatter.length;
		for (int i = 0; i < ST_COUNT; i++) {
			memcpy (eids + queue[i].start, entids[i],
					sizeof (uint32_t[queue[i].count]));
		}
		auto ir_barrier = &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead];
		QFV_PacketScatterBuffer (packet, lframe->entid_buffer, 1, &eid_scatter,
								 ir_barrier);

		memset (lframe->id_radius, -1, MaxLights * sizeof (light_idrad_t));
		for (int i = 0; i < ST_COUNT; i++) {
			auto q = queue[i];
			auto idr = &lframe->id_radius[q.start];
			auto pos = &lframe->positions[q.start];
			for (uint32_t j = 0; j < q.count; j++) {
				idr[j] = (light_idrad_t) {
					.id = light_ids[i][j],
					.radius = light_radii[i][j],
					.leafnum = light_leafs[i][j],
				};
				pos[j] = light_positions[i][j];
			}
		}
	}

	QFV_PacketSubmit (packet);
}

static void
lighting_update_descriptors (const exprval_t **params, exprval_t *result,
							 exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
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
	qfZoneNamed (zone, true);
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

	if (stage == lighting_hull) {
		bool planes = shadow_type == ST_PLANE;
		VkDescriptorSet sets[] = {
			Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
			lframe->lights_set,
			planes ? Vulkan_Translucent_Descriptors (ctx, ctx->curFrame) : 0,
		};
		dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
										layout, 0, 2 + planes, sets, 0, 0);

		VkBuffer buffers[] = {
			lframe->id_buffer,
			lframe->radius_buffer,
			lctx->splat_verts,
		};
		VkDeviceSize offsets[] = { 0, 0, 0 };
		dfunc->vkCmdBindVertexBuffers (cmd, 0, 3, buffers, offsets);
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
	qfZoneNamed (zone, true);
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
lighting_rewrite_ids (lightingframe_t *lframe, vulkan_ctx_t *ctx)
{
	uint32_t    count = 0;
	auto lctx = ctx->lighting_context;

	for (int i = 0; i < ST_COUNT; i++) {
		auto q = &lframe->light_queue[i];
		count += q->count;
	}
	uint32_t light_ids[count];
	float light_radii[count];
	uint32_t light_leafs[count];
	uint32_t light_count = 0;
	light_queue_t queue[ST_COUNT] = {};
	for (int i = 0; i < ST_COUNT; i++) {
		auto q = &lframe->light_queue[i];
		for (uint32_t j = 0; j < q[0].count; j++) {
			uint32_t    id = lframe->id_radius[q[0].start + j].id;
			float       radius = lframe->id_radius[q[0].start + j].radius;
			uint32_t    leaf = lframe->id_radius[q[0].start + j].leafnum;
			if (id != ~0u) {
				light_ids[queue[i].start + queue[i].count] = id;
				light_radii[queue[i].start + queue[i].count] = radius;
				light_leafs[queue[i].start + queue[i].count] = leaf;
				queue[i].count++;
			}
		}
		if (i < ST_COUNT - 1) {
			queue[i + 1].start = queue[i].start + queue[i].count;
		} else {
			light_count = queue[i].start + queue[i].count;
		}
	}
	for (int i = 0; i < ST_COUNT; i++) {
		lframe->light_queue[i] = queue[i];
	}

	for (int i = 0; i < LIGHTING_STAGES; i++) {
		lframe->stage_queue[i].count = 0;
	}
	int matrix_id_count = 0;
	for (uint32_t i = 0; i < light_count; i++) {
		auto r = &lctx->light_control.a[light_ids[i]];
		if (r->light_id != light_ids[i]) {
			Sys_Error ("%d != %d", r->light_id, light_ids[i]);
		}
		lframe->stage_queue[r->stage_index].count += r->numLayers;
		matrix_id_count += r->numLayers;

		lframe->id_radius[i] = (light_idrad_t) {
			.id = light_ids[i],
			.radius = light_radii[i],
			.leafnum = light_leafs[i],
		};
	}
	lframe->stage_queue[0].start = 0;
	for (int i = 1; i < LIGHTING_STAGES; i++) {
		auto q = &lframe->stage_queue[i];
		q[0].start = q[-1].start + q[-1].count;
		q[-1].count = 0;
	}
	lframe->stage_queue[LIGHTING_STAGES - 1].count = 0;

	size_t packet_size = 0;
	packet_size += sizeof (uint32_t[light_count]);
	packet_size += sizeof (float[light_count]);
	packet_size += sizeof (uint32_t[matrix_id_count]);

	if (!packet_size) {
		return;
	}

	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
	auto packet = QFV_PacketAcquire (ctx->staging);
	byte *packet_start = QFV_PacketExtend (packet, packet_size);
	byte *packet_data = packet_start;

	qfv_scatter_t id_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = sizeof (uint32_t[light_count]),
	};
	auto id_data = (uint32_t *) packet_data;
	packet_data += id_scatter.length;

	qfv_scatter_t radius_scatter = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = sizeof (uint32_t[light_count]),
	};
	auto radius_data = (uint32_t *) packet_data;
	packet_data += radius_scatter.length;

	qfv_scatter_t matrix_id_scater = {
		.srcOffset = packet_data - packet_start,
		.dstOffset = 0,
		.length = sizeof (uint32_t[matrix_id_count]),
	};
	auto matrix_ids = (uint32_t *) packet_data;
	packet_data += matrix_id_scater.length;

	memcpy (id_data, light_ids, packet_size);
	memcpy (radius_data, light_radii, packet_size);
	for (uint32_t i = 0; i < light_count; i++) {
		auto r = &lctx->light_control.a[light_ids[i]];
		enqueue_map (matrix_ids, lframe, r);
	}

	QFV_PacketScatterBuffer (packet, lframe->id_buffer, 1, &id_scatter, bb);
	QFV_PacketScatterBuffer (packet, lframe->radius_buffer,
							 1, &radius_scatter, bb);
	QFV_PacketScatterBuffer (packet, lframe->shadowmat_id_buffer,
							 1, &matrix_id_scater, bb);

	QFV_PacketSubmit (packet);
	transition_shadow_targets (lframe, ctx);

	if (developer & SYS_lighting) {
		Vulkan_Draw_String (vid.width - 32, 16,
							va (ctx->va_ctx, "%3d", light_count),
							ctx);
	}
}

static void
lighting_cull_select_renderpass (const exprval_t **params, exprval_t *result,
								 exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	auto light_cull = QFV_GetStep (params[0], ctx->render_context->job);
	auto render = light_cull->render;

	if (scr_fisheye) {
		render->active = &render->renderpasses[1];
	} else {
		render->active = &render->renderpasses[0];
	}
}

static void
lighting_cull_lights (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	auto queue = lframe->light_queue;
	uint32_t count = queue[ST_CUBE].count + queue[ST_PLANE].count;
	if (!count) {
		return;
	}
	if (scr_fisheye) {
		count *= 6;
	}

	auto light_cull = QFV_GetStep (params[0], ctx->render_context->job);
	auto render = light_cull->render;

	auto cmd = QFV_GetCmdBuffer (ctx, false);
	dfunc->vkBeginCommandBuffer (cmd, &(VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});
	qftCVkCollect (lframe->qftVkCtx, cmd);
	dfunc->vkCmdResetQueryPool (cmd, lframe->query, 0, MaxLights * 6);
	auto qftVkCtx = taskctx->frame->qftVkCtx;
	taskctx->frame->qftVkCtx = lframe->qftVkCtx;
	QFV_RunRenderPassCmd (cmd, ctx, render->active, 0);
	taskctx->frame->qftVkCtx = qftVkCtx;
	dfunc->vkEndCommandBuffer (cmd);

	qfMessageL ("submit");
	auto dev_queue = &device->queue;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};
	dfunc->vkResetFences (device->dev, 1, &lframe->fence);
	dfunc->vkQueueSubmit (dev_queue->queue, 1, &submitInfo, lframe->fence);
	dfunc->vkWaitForFences (device->dev, 1, &lframe->fence, VK_TRUE, 200000000);

	uint32_t    frag_counts[count];
	VkDeviceSize size = sizeof (frag_counts);
	dfunc->vkGetQueryPoolResults (device->dev, lframe->query, 0, count,
								  size, frag_counts, sizeof (uint32_t),
								  VK_QUERY_RESULT_WAIT_BIT);
	if (scr_fisheye) {
		uint32_t *p = frag_counts;
		for (uint32_t i = 0; i < count; i += 6) {
			uint32_t sum = 0;
			for (int j = 0; j < 6; j++) {
				// care only about non-zero, not actual count
				sum |= frag_counts[i + j];
			}
			*p++ = sum;
		}
	}
	uint32_t c = 0;
	uint32_t ci = 0;
	vec4f_t  cam = r_refdef.camera[3];
	uint32_t id = 0;
	if (lframe->light_queue[ST_CUBE].count) {
		auto q = lframe->light_queue[ST_CUBE];
		for (uint32_t i = 0; i < q.count; i++) {
			uint32_t fc = frag_counts[id++];
			c += fc != 0;
			if (!fc) {
				uint32_t hull = q.start + i;
				vec4f_t  dist = cam - lframe->positions[hull];
				dist[3] = 0;
				float    rad = lframe->id_radius[hull].radius;
				constexpr float s = 1.5835921350012616f;
				bool     inside = dotf(dist, dist)[0] < rad * rad * s;
				ci += inside;
				if (!inside) {
					lframe->id_radius[hull].id = ~0u;
				}
			}
		}
	}
	if (lframe->light_queue[ST_PLANE].count) {
		auto q = lframe->light_queue[ST_PLANE];
		for (uint32_t i = 0; i < q.count; i++) {
			uint32_t fc = frag_counts[id++];
			c += fc != 0;
			if (!fc) {
				uint32_t hull = q.start + i;
				vec4f_t  dist = cam - lframe->positions[hull];
				dist[3] = 0;
				float    rad = lframe->id_radius[hull].radius;
				bool     inside = dotf(dist, dist)[0] < rad * rad;
				ci += inside;
				if (!inside) {
					lframe->id_radius[hull].id = ~0u;
				}
			}
		}
	}
	lighting_rewrite_ids (lframe, ctx);
}

static void
draw_hull (uint32_t indexCount, uint32_t firstIndex, int32_t vertOffset,
		   uint32_t hull, uint32_t id, VkCommandBuffer cmd, VkQueryPool query,
		   qfv_devfuncs_t *dfunc, qftVkCtx_t *vk)
{
	qfZoneNamed (zone, true);
//	qftVkScopedZoneC (vk, cmd, "draw_hull", 0xc0a000);
	dfunc->vkCmdBeginQuery (cmd, query, id, 0);
	dfunc->vkCmdDrawIndexed (cmd, indexCount, 1, firstIndex, vertOffset, hull);
	dfunc->vkCmdEndQuery (cmd, query, id);
}

static void
lighting_draw_hulls (const exprval_t **params, exprval_t *result,
					 exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;
	auto cmd = taskctx->cmd;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	uint32_t id = 0;
	uint32_t id_step = scr_fisheye ? 6 : 1;
	if (lframe->light_queue[ST_CUBE].count) {
		auto q = lframe->light_queue[ST_CUBE];
		for (uint32_t i = 0; i < q.count; i++, id += id_step) {
			uint32_t hull = q.start + i;
			draw_hull (num_ico_inds, 0, 0, hull, id,
					   cmd, lframe->query, dfunc, taskctx->frame->qftVkCtx);
		}
	}
	if (lframe->light_queue[ST_PLANE].count) {
		auto q = lframe->light_queue[ST_PLANE];
		for (uint32_t i = 0; i < q.count; i++, id += id_step) {
			uint32_t hull = q.start + i;
			draw_hull (num_cone_inds, num_ico_inds, 12, hull, id,
					   cmd, lframe->query, dfunc, taskctx->frame->qftVkCtx);
		}
	}
}

static void
lighting_draw_lights (const exprval_t **params, exprval_t *result,
					  exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
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

	//FIXME dup of z_range (sort of)
	vec4f_t depths = {
		r_nearclip / 32, r_nearclip / 256, r_nearclip / 1024, 0,
	};
	qfv_push_constants_t push_constants[] = {
		{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (depths), &depths },
		{ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(depths), sizeof(queue), &queue },
	};
	QFV_PushConstants (device, cmd, layout, 2, push_constants);

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
	lighting_hull,
};
static exprsym_t lighting_stage_symbols[] = {
	{"main", &lighting_stage_type, lighting_stage_values + 0},
	{"shadow", &lighting_stage_type, lighting_stage_values + 1},
	{"hull", &lighting_stage_type, lighting_stage_values + 2},
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
static exprfunc_t lighting_cull_select_renderpass_func[] = {
	{ .func = lighting_cull_select_renderpass, .num_params = 1,
		.param_types = stepref_param },
	{}
};
static exprfunc_t lighting_cull_lights_func[] = {
	{ .func = lighting_cull_lights, .num_params = 1,
		.param_types = stepref_param },
	{}
};
static exprfunc_t lighting_draw_hulls_func[] = {
	{ .func = lighting_draw_hulls },
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
	{ "lighting_cull_select_renderpass", &cexpr_function,
		lighting_cull_select_renderpass_func },
	{ "lighting_cull_lights", &cexpr_function, lighting_cull_lights_func },
	{ "lighting_draw_hulls", &cexpr_function, lighting_draw_hulls_func },
	{ "lighting_draw_lights", &cexpr_function, lighting_draw_lights_func },
	{ "lighting_setup_shadow", &cexpr_function, lighting_setup_shadow_func },
	{ "lighting_draw_shadow_maps", &cexpr_function,
		lighting_draw_shadow_maps_func },
	{}
};

static int
round_light_size (int size)
{
	size = ((size + shadow_quanta - 1) / shadow_quanta) * shadow_quanta;
	return min (size, 1024);
}

static void
dynlight_size_listener (void *data, const cvar_t *cvar)
{
	dynlight_size = round_light_size (dynlight_size);
}

void
Vulkan_Lighting_Init (vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = calloc (1, sizeof (lightingctx_t));
	ctx->lighting_context = lctx;

	Cvar_Register (&dynlight_size_cvar, dynlight_size_listener, 0);

	QFV_Render_AddTasks (ctx, lighting_task_syms);

	lctx->shadow_info = (qfv_attachmentinfo_t) {
		.name = "$shadow",
		.format = VK_FORMAT_D32_SFLOAT,
		.samples = 1,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,//FIXME plist
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
			&(qfv_output_t) { .format = VK_FORMAT_D32_SFLOAT });

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
									// light matrices
									+ sizeof (qfv_resobj_t[frames])
									// light matrix ids
									+ sizeof (qfv_resobj_t[frames])
									// light ids
									+ sizeof (qfv_resobj_t[frames])
									// light radii
									+ sizeof (qfv_resobj_t[frames])
									// light data
									+ sizeof (qfv_resobj_t[frames])
									// light render
									+ sizeof (qfv_resobj_t[frames])
									// light styles
									+ sizeof (qfv_resobj_t[frames])
									// light entids
									+ sizeof (qfv_resobj_t[frames]));
	lctx->light_resources[0] = (qfv_resource_t) {
		.name = "lights",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 + 3 + 8 * frames,
		.objects = (qfv_resobj_t *) &lctx->light_resources[1],
	};
	auto splat_verts = lctx->light_resources->objects;
	auto splat_inds = &splat_verts[1];
	auto default_map = &splat_inds[1];
	auto default_view_cube = &default_map[1];
	auto default_view_2d = &default_view_cube[1];
	auto light_mats = &default_view_2d[1];
	auto light_mat_ids = &light_mats[frames];
	auto light_ids = &light_mat_ids[frames];
	auto light_radii = &light_ids[frames];
	auto light_data = &light_radii[frames];
	auto light_render = &light_data[frames];
	auto light_styles = &light_render[frames];
	auto light_entids = &light_styles[frames];
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
			.format = VK_FORMAT_D32_SFLOAT,
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
			.format = VK_FORMAT_D32_SFLOAT,
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
			.format = VK_FORMAT_D32_SFLOAT,
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
		light_radii[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "radii:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = MaxLights * sizeof (float),
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
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
		light_mat_ids[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "matrix ids:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				// never need more than 6 matrices per light
				.size = sizeof (uint32_t[MaxLights * 6]),
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
			.shadowmat_id_buffer = light_mat_ids[i].buffer.buffer,
			.light_buffer = light_data[i].buffer.buffer,
			.render_buffer = light_render[i].buffer.buffer,
			.style_buffer = light_styles[i].buffer.buffer,
			.id_buffer = light_ids[i].buffer.buffer,
			.radius_buffer = light_radii[i].buffer.buffer,
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

		VkDescriptorBufferInfo bufferInfo[] = {
			{	.buffer = lframe->shadowmat_buffer,
				.offset = 0, .range = VK_WHOLE_SIZE, },
			{	.buffer = lframe->shadowmat_id_buffer,
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
				.dstSet = lframe->shadowmat_set,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[1], },

			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[2], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[3], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[4], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[5], },
			{	.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = lframe->lights_set,
				.dstBinding = 4,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo[6], },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 7, bufferWrite, 0, 0);

		dfunc->vkCreateQueryPool (device->dev, &(VkQueryPoolCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_OCCLUSION,
			.queryCount = MaxLights * 6,	// 6 for cube maps
		}, 0, &lframe->query);
		lframe->fence = QFV_CreateFence (device, 1);
#ifdef TRACY_ENABLE
		auto instance = ctx->instance->instance;
		auto physdev = ctx->device->physDev->dev;
		auto gipa = ctx->vkGetInstanceProcAddr;
		auto gdpa = ctx->instance->funcs->vkGetDeviceProcAddr;
		lframe->qftVkCtx = qftCVkContextHostCalibrated (instance, physdev,
														device->dev,
														gipa, gdpa);
#endif
	}
	size_t target_count = MaxLights * 6;
	size_t target_size = frames * sizeof (uint16_t[target_count]);
	size_t idr_size = frames * sizeof (light_idrad_t[MaxLights]);
	size_t position_size = frames * sizeof (vec4f_t[MaxLights]);
	lctx->frames.a[0].stage_targets = malloc (target_size);
	lctx->frames.a[0].id_radius = malloc (idr_size);
	lctx->frames.a[0].positions = malloc (position_size);
	for (size_t i = 1; i < frames; i++) {
		auto lframe = &lctx->frames.a[i];
		lframe[0].stage_targets = lframe[-1].stage_targets + target_count;
		lframe[0].id_radius = lframe[-1].id_radius + MaxLights;
		lframe[0].positions = lframe[-1].positions + MaxLights;
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
	qfZoneScoped (true);
	qfv_device_t *device = ctx->device;
	auto dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	if (lctx->shadow_resources) {
		QFV_DestroyResource (device, lctx->shadow_resources);
		free (lctx->shadow_resources);
		lctx->shadow_resources = 0;
	}
	for (int i = 0; i < LIGHTING_STAGES; i++) {
		for (int j = 0; j < 32; j++) {
			auto framebuffer = lctx->stage_framebuffers[j][i];
			if (framebuffer) {
				dfunc->vkDestroyFramebuffer (device->dev, framebuffer, 0);
			}
			lctx->stage_framebuffers[j][i] = 0;
		}
		// images and views freed via shadow_resources
		lctx->stage_images[i] = 0;
		lctx->stage_views[i] = 0;
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
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;

	clear_shadows (ctx);

	QFV_DestroyResource (device, lctx->light_resources);
	free (lctx->light_resources);

	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		dfunc->vkDestroyQueryPool (device->dev, lframe->query, 0);
		dfunc->vkDestroyFence (device->dev, lframe->fence, 0);
	}
	free (lctx->frames.a[0].stage_targets);
	DARRAY_CLEAR (&lctx->light_mats);
	DARRAY_CLEAR (&lctx->light_control);
	free (lctx->map_images);
	free (lctx->map_views);
	free (lctx->map_cube);
	free (lctx->frames.a);
	free (lctx);
}

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
		vec4f_t     dir;

		switch (r->mode) {
			default:
			case ST_NONE:
				continue;
			case ST_CUBE:
				mat4fidentity (view);
				break;
			case ST_CASCADE:
			case ST_PLANE:
				dir = light->direction;
				dir[3] = 0;
				vec4f_t q = dir[0] == -1 ? (vec4f_t) { 0, 0, 1, 0 }
										 : qrotf (dir, ref_direction);
				mat4fquat (view, q);
				break;
		}
		vec4f_t pos = -light->position;
		pos[3] = 1;
		view[3] = mvmulf (view, pos);

		switch (r->mode) {
			case ST_NONE:
				continue;
			case ST_CUBE:
				QFV_PerspectiveTan (proj, 1, 1, lnearclip);
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
				for (int j = 0; j < num_cascade; j++) {
					mmulf (lm[j], proj, view);
				}
				break;
			case ST_PLANE:
				QFV_PerspectiveCos (proj, -light->direction[3], lnearclip);
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

	packet = QFV_PacketAcquire (ctx->staging);
	size_t id_size = sizeof (uint32_t[MaxLights * 6]);
	uint32_t *id_data = QFV_PacketExtend (packet, id_size);
	memset (id_data, -1, id_size);
	for (size_t i = 0; i < lctx->frames.size; i++) {
		auto lframe = &lctx->frames.a[i];
		QFV_PacketCopyBuffer (packet, lframe->shadowmat_id_buffer, 0, bb);
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
			.id_data = make_id(r, style_enable),
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
		// same size
		if (l1->position[3] == l2->position[3]) {
			// same "type" (point/spot vs directional)
			// sort by spot size (1 for point/directional)
			return (l2->direction[3] > -0.5) - (l1->direction[3] > -0.5);
		}
		// sort by "type" (point/spot vs directional)
		return l2->position[3] - l1->position[3];
	}
	// sort by size
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
	int         layers = ((int[ST_COUNT]) { 0, 1, num_cascade, 6 })[type];
	int         cube = type == ST_CUBE;

	for (int i = 0; i < mctx->numLights; i++) {
		auto li = mctx->lightMap[i];
		auto lr = &mctx->control[li];

		if (lr->mode != type) {
			continue;
		}
		int light_size = getsize (&mctx->lights[li]);
		light_size = round_light_size (light_size);
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
			.stage_index = (size / shadow_quanta) - 1,
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
	int         maxLayers = physDev->p.properties.limits.maxImageArrayLayers;
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
	mapdesc_t   maps[numLights + dynlight_max];

	for (int i = 0; i < numLights; i++) {
		lightMap[i] = i;
	}
	// sort lights by size, type, spot-size
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
	int stage_layers[LIGHTING_STAGES] = {};
	if (mctx.numMaps) {
		for (int i = 0; i < mctx.numMaps; i++) {
			int ind = (maps[i].size / shadow_quanta) - 1;
			stage_layers[ind] += maps[i].layers;
		}
		int stage_maps = 0;
		for (int i = 0; i < LIGHTING_STAGES; i++) {
			stage_layers[i] = min (stage_layers[i], max_views);
			if (stage_layers[i]) {
				stage_maps++;
			}
		}
		qfv_resource_t *shad = calloc (1, sizeof (qfv_resource_t)
									   + sizeof (qfv_resobj_t[mctx.numMaps])
									   + sizeof (qfv_resobj_t[mctx.numMaps])
									   + sizeof (qfv_resobj_t[stage_maps])
									   + sizeof (qfv_resobj_t[stage_maps]));
		lctx->shadow_resources = shad;
		*shad = (qfv_resource_t) {
			.name = "shadow",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = 2 * (mctx.numMaps + stage_maps),
			.objects = (qfv_resobj_t *) &shad[1],
		};
		lctx->map_images = malloc (sizeof (VkImage[mctx.numMaps]));
		lctx->map_views = malloc (sizeof (VkImageView[mctx.numMaps]));
		lctx->map_cube = malloc (sizeof (bool[mctx.numMaps]));
		auto images = shad->objects;
		auto stage_images = &images[mctx.numMaps];
		auto views = &stage_images[stage_maps];
		auto stage_views = &views[mctx.numMaps];
		for (int i = 0; i < mctx.numMaps; i++) {
			int         cube = maps[i].cube;
			lctx->map_cube[i] = cube;
			images[i] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "map:image:%02d:%d", i, maps[i].size),
				.type = qfv_res_image,
				.image = {
					.flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
					.type = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_D32_SFLOAT,
					.extent = { maps[i].size, maps[i].size, 1 },
					.num_mipmaps = 1,
					.num_layers = maps[i].layers,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
							| VK_IMAGE_USAGE_TRANSFER_DST_BIT
							| VK_IMAGE_USAGE_SAMPLED_BIT,
				},
			};
			views[i] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "map:view:%02d:%d", i, maps[i].size),
				.type = qfv_res_image_view,
				.image_view = {
					.image = i,
					.type = cube ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
								 : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
					.format = VK_FORMAT_D32_SFLOAT,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					},
				},
			};
		}
		for (int i = 0, ind = 0; i < LIGHTING_STAGES; i++) {
			if (!stage_layers[i]) {
				continue;
			}
			int size = (i + 1) * shadow_quanta;
			stage_images[ind] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "stage_map:image:%02d:%d", ind, size),
				.type = qfv_res_image,
				.image = {
					.type = VK_IMAGE_TYPE_2D,
					.format = VK_FORMAT_D32_SFLOAT,
					.extent = { size, size, 1 },
					.num_mipmaps = 1,
					.num_layers = stage_layers[i],
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
							| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				},
			};
			stage_views[ind] = (qfv_resobj_t) {
				.name = va (ctx->va_ctx, "stage_map:view:%02d:%d", ind, size),
				.type = qfv_res_image_view,
				.image_view = {
					.image = mctx.numMaps + ind,
					.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
					.format = VK_FORMAT_D32_SFLOAT,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					},
				},
			};
			ind++;
		}
		QFV_CreateResource (device, shad);
		for (int i = 0; i < mctx.numMaps; i++) {
			lctx->map_images[i] = images[i].image.image;
			lctx->map_views[i] = views[i].image_view.view;
		}
		for (int i = 0, ind = 0; i < LIGHTING_STAGES; i++) {
			if (!stage_layers[i]) {
				continue;
			}
			lctx->stage_images[i] = stage_images[ind].image.image;
			lctx->stage_views[i] = stage_views[ind].image_view.view;
			ind++;
		}
	}

	for (int i = 0; i < numLights; i++) {
		int  li = lightMap[i];
		auto lr = &lctx->light_control.a[li];

		if (imageMap[li] == -1) {
			continue;
		}

		lr->stage_index = -1;
		if (lr->numLayers) {
			lr->stage_index = (lr->size / shadow_quanta) - 1;
		}
		lr->map_index = imageMap[li];
	}
	Sys_MaskPrintf (SYS_lighting,
					"shadow maps: %d layers in %d images: %"PRId64"\n",
					totalLayers, lctx->num_maps,
					lctx->shadow_resources ? lctx->shadow_resources->size
										   : (VkDeviceSize) 0);
	if (developer & SYS_lighting) {
		auto images = lctx->shadow_resources->objects;
		for (int i = 0; i < lctx->num_maps; i++) {
			Sys_Printf ("map id:%d width:%d layers:%d\n", i,
						images[i].image.extent.width,
						images[i].image.num_layers);
		}
		for (int i = 0; i < LIGHTING_STAGES; i++) {
			Sys_Printf ("stage:%d width:%d layers:%d\n", i,
						(i + 1) * shadow_quanta, stage_layers[i]);
		}
	}
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
light_dyn_light_ui (void *comp, imui_ctx_t *imui_ctx,
					ecs_registry_t *reg, uint32_t ent, void *data)
{
	dlight_t *dlight = comp;
	UI_Horizontal {
		UI_Labelf ("Origin: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.1f %6.1f %6.1f %6g", VEC4_EXP (dlight->origin));
	}
	UI_Horizontal {
		UI_Label ("Color: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%5.3f %5.3f %5.3f %5.3f", VEC4_EXP (dlight->color));
	}
	UI_Horizontal {
		UI_Labelf ("Radius: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6g", dlight->radius);
	}
	UI_Horizontal {
		UI_Labelf ("Die: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.2f", dlight->die);
	}
	UI_Horizontal {
		UI_Labelf ("Decay: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.2f", dlight->decay);
	}
	UI_Horizontal {
		UI_Labelf ("Min Light: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6g", dlight->minlight);
	}
}

static void
light_light_ui (void *comp, imui_ctx_t *imui_ctx,
				ecs_registry_t *reg, uint32_t ent, void *data)
{
	light_t *light = comp;
	UI_Horizontal {
		UI_Label ("Color: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%5.3f %5.3f %5.3f %3g", VEC4_EXP (light->color));
	}
	UI_Horizontal {
		UI_Labelf ("Position: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.1f %6.1f %6.1f %6g", VEC4_EXP (light->position));
	}
	UI_Horizontal {
		UI_Labelf ("Direction: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.3f %6.3f %6.3f %6.3g", VEC4_EXP (light->direction));
	}
	UI_Horizontal {
		UI_Labelf ("Attenuation: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%g %g %g %g", VEC4_EXP (light->attenuation));
	}
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

static void
scene_lightstyle_ui (void *comp, imui_ctx_t *imui_ctx,
					 ecs_registry_t *reg, uint32_t ent, void *data)
{
	auto style = *(uint32_t *) comp;

	UI_Horizontal {
		UI_Labelf ("%5u", style);

		UI_FlexibleSpace ();
		auto val = d_lightstylevalue[style];
		UI_Labelf ("%3f", val / 65536.0);
	}
}

static void
scene_lightid_ui (void *comp, imui_ctx_t *imui_ctx,
				  ecs_registry_t *reg, uint32_t ent, void *data)
{
	auto id = *(uint32_t *) comp;

	UI_Horizontal {
		UI_Labelf ("%5u", id);
		UI_FlexibleSpace ();
	}
}

void
Vulkan_LoadLights (scene_t *scene, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	lightingctx_t *lctx = ctx->lighting_context;

	lctx->scene = scene;

	clear_shadows (ctx);

	lctx->ldata = 0;
	if (lctx->scene) {
		auto reg = lctx->scene->reg;
		reg->components.a[scene_dynlight].ui = light_dyn_light_ui;
		reg->components.a[scene_light].ui = light_light_ui;
		reg->components.a[scene_efrags].ui = scene_efrags_ui;
		reg->components.a[scene_lightstyle].ui = scene_lightstyle_ui;
		reg->components.a[scene_lightleaf].ui = scene_lightleaf_ui;
		reg->components.a[scene_lightid].ui = scene_lightid_ui;

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
