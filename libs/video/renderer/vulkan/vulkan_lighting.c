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
#include "QF/ui/view.h"

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
#if 0
static void
lighting_draw_maps (qfv_orenderframe_t *rFrame)
{
	vulkan_ctx_t *ctx = rFrame->vulkan_ctx;
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	if (rFrame->subpassCmdSets[0].size) {
		__auto_type sets = &rFrame->subpassCmdSets[0];
		dfunc->vkFreeCommandBuffers (device->dev, lctx->cmdpool,
									 sets->size, sets->a);
		sets->size = 0;
	}

	if (!lctx->ldata || !lctx->ldata->lights.size) {
		return;
	}
	if (!lctx->light_renderers.a[0].renderPass) {
		//FIXME goes away when lighting implemented properly
		return;
	}

	__auto_type bufferset = QFV_AllocCommandBufferSet (1, alloca);
	QFV_AllocateCommandBuffers (device, lctx->cmdpool, 0, bufferset);
	VkCommandBuffer cmd = bufferset->a[0];
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_COMMAND_BUFFER,
						 cmd, va (ctx->va_ctx, "lighting:%d", ctx->curFrame));

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	dfunc->vkBeginCommandBuffer (cmd, &beginInfo);

	__auto_type rp = rFrame->renderpass;
	QFV_CmdBeginLabel (device, cmd, rp->name, rp->color);

	__auto_type lr = &lctx->light_renderers.a[0];
	VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderArea = { {0, 0}, {lr->size, lr->size} },
		.framebuffer = lr->framebuffer,
		.renderPass = lr->renderPass,
		.pClearValues = lctx->qfv_renderpass->clearValues->a,
	};
	__auto_type subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
	if (renderPassInfo.renderPass) {
		dfunc->vkCmdBeginRenderPass (cmd, &renderPassInfo, subpassContents);
		//...
		dfunc->vkCmdEndRenderPass (cmd);
	}
	QFV_CmdEndLabel (device, cmd);
	dfunc->vkEndCommandBuffer (cmd);

	DARRAY_APPEND (&rFrame->subpassCmdSets[0], cmd);
}

void
Vulkan_Lighting_CreateRenderPasses (vulkan_ctx_t *ctx)
{

	// extents are dynamic and filled in for each light
	// frame buffers are highly dynamic
	__auto_type rp = QFV_RenderPass_New (ctx, "shadow", lighting_draw_maps);
	QFV_RenderPass_CreateRenderPass (rp);
	rp->primary_commands = 1;
	rp->order = QFV_rp_shadowmap;
	DARRAY_APPEND (&ctx->renderPasses, rp);

	lctx->qfv_renderpass = rp;
}
#endif
static void
lighting_update_lights (const exprval_t **params, exprval_t *result,
						exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto lctx = ctx->lighting_context;

	auto lframe = &lctx->frames.a[ctx->curFrame];

	lframe->ico_count = 0;
	lframe->cone_count = 0;
	lframe->flat_count = 0;
	if (!lctx->scene || !lctx->scene->lights) {
		return;
	}

	lightingdata_t *ldata = lctx->ldata;

	Light_FindVisibleLights (ldata);

	dlight_t   *lights[MaxLights];
	auto packet = QFV_PacketAcquire (ctx->staging);
	qfv_light_buffer_t *light_data = QFV_PacketExtend (packet,
													   sizeof (*light_data));

	float       style_intensities[NumStyles];
	for (int i = 0; i < NumStyles; i++) {
		style_intensities[i] = d_lightstylevalue[i] / 65536.0;
	}

	uint32_t ico_ids[MaxLights];
	uint32_t cone_ids[MaxLights];
	uint32_t flat_ids[MaxLights];

	light_data->lightCount = 0;
	R_FindNearLights (r_refdef.frame.position, MaxLights - 1, lights);
	for (int i = 0; i < MaxLights - 1; i++) {
		if (!lights[i]) {
			break;
		}
		ico_ids[lframe->ico_count++] = light_data->lightCount++;

		VectorCopy (lights[i]->color, light_data->lights[i].color);
		// dynamic lights seem a tad faint, so 16x map lights
		light_data->lights[i].color[3] = lights[i]->radius / 16;
		VectorCopy (lights[i]->origin, light_data->lights[i].position);
		// dlights are local point sources
		light_data->lights[i].position[3] = 1;
		light_data->lights[i].attenuation =
			(vec4f_t) { 0, 0, 1, 1/lights[i]->radius };
		// full sphere, normal light (not ambient)
		light_data->lights[i].direction = (vec4f_t) { 0, 0, 1, 1 };
	}
	for (size_t i = 0; (i < ldata->lightvis.size
						&& light_data->lightCount < MaxLights); i++) {
		if (ldata->lightvis.a[i]) {
			uint32_t id = light_data->lightCount++;
			auto light = &light_data->lights[id];
			*light = ldata->lights.a[i];
			light->color[3] *= style_intensities[ldata->lightstyles.a[i]];
			if (light->position[3] && !VectorIsZero (light->direction)
				&& light->attenuation[3]) {
				if (light->direction[3] < 0) {
					cone_ids[lframe->cone_count++] = id;
				} else {
					ico_ids[lframe->ico_count++] = id;
				}
			} else {
				flat_ids[lframe->flat_count++] = id;
			}
		}
	}
	if (developer & SYS_lighting) {
		Vulkan_Draw_String (vid.width - 32, 8,
							va (ctx->va_ctx, "%3d", light_data->lightCount),
							ctx);
	}

	QFV_PacketCopyBuffer (packet, lframe->data_buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead]);
	QFV_PacketSubmit (packet);

	packet = QFV_PacketAcquire (ctx->staging);
	uint32_t id_count = lframe->ico_count + lframe->cone_count
						+ lframe->flat_count;
	uint32_t *ids = QFV_PacketExtend (packet, id_count * sizeof (uint32_t));
	memcpy (ids, ico_ids, lframe->ico_count * sizeof (uint32_t));
	ids += lframe->ico_count;
	memcpy (ids, cone_ids, lframe->cone_count * sizeof (uint32_t));
	ids += lframe->cone_count;
	memcpy (ids, flat_ids, lframe->flat_count * sizeof (uint32_t));
	QFV_PacketCopyBuffer (packet, lframe->id_buffer, 0,
						  &bufferBarriers[qfv_BB_TransferWrite_to_IndexRead]);
	QFV_PacketSubmit (packet);
}

static VkDescriptorBufferInfo base_buffer_info = {
	0, 0, VK_WHOLE_SIZE
};
static VkDescriptorImageInfo base_image_info = {
	0, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};
static VkWriteDescriptorSet base_buffer_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	0, 0, 0
};
static VkWriteDescriptorSet base_attachment_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	0, 0, 0
};
static VkWriteDescriptorSet base_image_write = {
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0, 0,
	0, 0, 1,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	0, 0, 0
};

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

	auto fb = &taskctx->renderpass->framebuffer;
	lframe->bufferInfo[0].buffer = lframe->data_buffer;
	lframe->attachInfo[0].imageView = fb->views[QFV_attachDepth];
	lframe->attachInfo[1].imageView = fb->views[QFV_attachColor];
	lframe->attachInfo[2].imageView = fb->views[QFV_attachEmission];
	lframe->attachInfo[3].imageView = fb->views[QFV_attachNormal];
	lframe->attachInfo[4].imageView = fb->views[QFV_attachPosition];
	dfunc->vkUpdateDescriptorSets (device->dev,
								   LIGHTING_DESCRIPTORS,
								   lframe->descriptors, 0, 0);
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
	auto cmd = taskctx->cmd;
	auto layout = taskctx->pipeline->layout;

	auto lframe = &lctx->frames.a[ctx->curFrame];

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		lframe->bufferWrite[0].dstSet,
		lframe->attachWrite[0].dstSet,
		lframe->shadowWrite.dstSet,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 3, sets, 0, 0);

	VkBuffer buffers[] = {
		lframe->id_buffer,
		lctx->splat_verts,
	};
	VkDeviceSize offsets[] = { 0, 0 };
	dfunc->vkCmdBindVertexBuffers (cmd, 0, 2, buffers, offsets);
	dfunc->vkCmdBindIndexBuffer (cmd, lctx->splat_inds, 0,
								 VK_INDEX_TYPE_UINT32);
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
	if (lframe->ico_count) {
		dfunc->vkCmdDrawIndexed (cmd, num_ico_inds, lframe->ico_count, 0, 0, 0);
	}
	if (lframe->cone_count) {
		dfunc->vkCmdDrawIndexed (cmd, num_cone_inds, lframe->cone_count,
								 num_ico_inds, 12, lframe->ico_count);
	}
}

static void
lighting_draw_flats (const exprval_t **params, exprval_t *result,
					 exprctx_t *ectx)
{
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto lctx = ctx->lighting_context;
	auto cmd = taskctx->cmd;

	auto lframe = &lctx->frames.a[ctx->curFrame];
	if (!lframe->flat_count) {
		return;
	}

	uint32_t splat_count = lframe->ico_count + lframe->cone_count;
	dfunc->vkCmdDraw (cmd, 3, lframe->flat_count, 0, splat_count);
}

static exprfunc_t lighting_update_lights_func[] = {
	{ .func = lighting_update_lights },
	{}
};
static exprfunc_t lighting_update_descriptors_func[] = {
	{ .func = lighting_update_descriptors },
	{}
};
static exprfunc_t lighting_bind_descriptors_func[] = {
	{ .func = lighting_bind_descriptors },
	{}
};
static exprfunc_t lighting_draw_splats_func[] = {
	{ .func = lighting_draw_splats },
	{}
};
static exprfunc_t lighting_draw_flats_func[] = {
	{ .func = lighting_draw_flats },
	{}
};
static exprsym_t lighting_task_syms[] = {
	{ "lighting_update_lights", &cexpr_function, lighting_update_lights_func },
	{ "lighting_update_descriptors", &cexpr_function,
		lighting_update_descriptors_func },
	{ "lighting_bind_descriptors", &cexpr_function,
		lighting_bind_descriptors_func },
	{ "lighting_draw_splats", &cexpr_function, lighting_draw_splats_func },
	{ "lighting_draw_flats", &cexpr_function, lighting_draw_flats_func },
	{}
};

void
Vulkan_Lighting_Init (vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = calloc (1, sizeof (lightingctx_t));
	ctx->lighting_context = lctx;

	QFV_Render_AddTasks (ctx, lighting_task_syms);
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
	auto lctx = ctx->lighting_context;

	lctx->sampler = QFV_Render_Sampler (ctx, "shadow_sampler");

	Vulkan_Script_SetOutput (ctx,
			&(qfv_output_t) { .format = VK_FORMAT_X8_D24_UNORM_PACK32 });
#if 0
	plitem_t   *rp_def = lctx->qfv_renderpass->renderpassDef;
	plitem_t   *rp_cfg = PL_ObjectForKey (rp_def, "renderpass_6");
	lctx->renderpass_6 = QFV_ParseRenderPass (ctx, rp_cfg, rp_def);
	rp_cfg = PL_ObjectForKey (rp_def, "renderpass_4");
	lctx->renderpass_4 = QFV_ParseRenderPass (ctx, rp_cfg, rp_def);
	rp_cfg = PL_ObjectForKey (rp_def, "renderpass_1");
	lctx->renderpass_1 = QFV_ParseRenderPass (ctx, rp_cfg, rp_def);
#endif

	DARRAY_INIT (&lctx->light_mats, 16);
	DARRAY_INIT (&lctx->light_images, 16);
	DARRAY_INIT (&lctx->light_renderers, 16);

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
									// light data
									+ sizeof (qfv_resobj_t[frames])
									// light indices
									+ sizeof (qfv_resobj_t[frames]));
	auto splat_verts = (qfv_resobj_t *) &lctx->light_resources[1];
	auto splat_inds = &splat_verts[1];
	auto light_data = &splat_inds[1];
	auto light_ids = &light_data[frames];
	lctx->light_resources[0] = (qfv_resource_t) {
		.name = "lights",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = 2 + 2 * frames,
		.objects = splat_verts,
	};
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
	for (size_t i = 0; i < frames; i++) {
		light_data[i] = (qfv_resobj_t) {
			.name = "data",
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_light_buffer_t),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
		light_ids[i] = (qfv_resobj_t) {
			.name = "ids",
			.type = qfv_res_buffer,
			.buffer = {
				.size = 2 * MaxLights * sizeof (uint32_t),
				.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
						| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
	}

	QFV_CreateResource (ctx->device, lctx->light_resources);

	lctx->splat_verts = splat_verts[0].buffer.buffer;
	lctx->splat_inds = splat_inds[0].buffer.buffer;

	auto attach_mgr = QFV_Render_DSManager (ctx, "lighting_attach");
	auto lights_mgr = QFV_Render_DSManager (ctx, "lighting_lights");
	auto shadow_mgr = QFV_Render_DSManager (ctx, "lighting_shadow");
	for (size_t i = 0; i < frames; i++) {
		auto lframe = &lctx->frames.a[i];
		*lframe = (lightingframe_t) {
			.data_buffer = light_data[i].buffer.buffer,
			.id_buffer = light_ids[i].buffer.buffer,
		};

		auto attach = QFV_DSManager_AllocSet (attach_mgr);
		auto lights = QFV_DSManager_AllocSet (lights_mgr);
		auto shadow = QFV_DSManager_AllocSet (shadow_mgr);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, attach,
							 va (ctx->va_ctx, "lighting:attach_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, lights,
							 va (ctx->va_ctx, "lighting:lights_set:%zd", i));
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, shadow,
							 va (ctx->va_ctx, "lighting:shadow_set:%zd", i));

		for (int j = 0; j < LIGHTING_BUFFER_INFOS; j++) {
			lframe->bufferInfo[j] = base_buffer_info;
			lframe->bufferWrite[j] = base_buffer_write;
			lframe->bufferWrite[j].dstSet = lights;
			lframe->bufferWrite[j].dstBinding = j;
			lframe->bufferWrite[j].pBufferInfo = &lframe->bufferInfo[j];
		}
		for (int j = 0; j < LIGHTING_ATTACH_INFOS; j++) {
			lframe->attachInfo[j] = base_image_info;
			lframe->attachInfo[j].sampler = 0;
			lframe->attachWrite[j] = base_attachment_write;
			lframe->attachWrite[j].dstSet = attach;
			lframe->attachWrite[j].dstBinding = j;
			lframe->attachWrite[j].pImageInfo = &lframe->attachInfo[j];
		}
		for (int j = 0; j < LIGHTING_SHADOW_INFOS; j++) {
			lframe->shadowInfo[j] = base_image_info;
			lframe->shadowInfo[j].sampler = lctx->sampler;
			lframe->shadowInfo[j].imageView = ctx->default_black->view;
		}
		lframe->shadowWrite = base_image_write;
		lframe->shadowWrite.dstSet = shadow;
		lframe->shadowWrite.dstBinding = 0;
		lframe->shadowWrite.descriptorCount = LIGHTING_SHADOW_INFOS;
		lframe->shadowWrite.pImageInfo = lframe->shadowInfo;
	}

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
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	for (size_t i = 0; i < lctx->light_renderers.size; i++) {
		__auto_type lr = &lctx->light_renderers.a[i];
		dfunc->vkDestroyFramebuffer (device->dev, lr->framebuffer, 0);
		dfunc->vkDestroyImageView (device->dev, lr->view, 0);
	}
	if (lctx->shadow_resources) {
		QFV_DestroyResource (device, lctx->shadow_resources);
		free (lctx->shadow_resources);
		lctx->shadow_resources = 0;
	}
	lctx->light_images.size = 0;
	lctx->light_renderers.size = 0;
}

void
Vulkan_Lighting_Shutdown (vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;
	lightingctx_t *lctx = ctx->lighting_context;

	clear_shadows (ctx);

	dfunc->vkDestroyRenderPass (device->dev, lctx->renderpass_6, 0);
	dfunc->vkDestroyRenderPass (device->dev, lctx->renderpass_4, 0);
	dfunc->vkDestroyRenderPass (device->dev, lctx->renderpass_1, 0);

	QFV_DestroyResource (device, lctx->light_resources);
	free (lctx->light_resources);

	DARRAY_CLEAR (&lctx->light_mats);
	DARRAY_CLEAR (&lctx->light_images);
	DARRAY_CLEAR (&lctx->light_renderers);
	free (lctx->frames.a);
	free (lctx);
}

static vec4f_t  ref_direction = { 0, 0, 1, 0 };

static void
create_light_matrices (lightingctx_t *lctx)
{
	lightingdata_t *ldata = lctx->ldata;
	DARRAY_RESIZE (&lctx->light_mats, ldata->lights.size);
	for (size_t i = 0; i < ldata->lights.size; i++) {
		light_t    *light = &ldata->lights.a[i];
		mat4f_t     view;
		mat4f_t     proj;
		int         mode = ST_NONE;

		if (!light->position[3]) {
			mode = ST_CASCADE;
		} else {
			if (light->direction[3] > -0.5) {
				mode = ST_CUBE;
			} else {
				mode = ST_PLANE;
			}
		}
		switch (mode) {
			default:
			case ST_NONE:
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
		VectorNegate (light->position, view[3]);

		switch (mode) {
			case ST_NONE:
				mat4fidentity (proj);
				break;
			case ST_CUBE:
				QFV_PerspectiveTan (proj, 1, 1);
				break;
			case ST_CASCADE:
				// dependent on view fustrum and cascade level
				mat4fidentity (proj);
				break;
			case ST_PLANE:
				QFV_PerspectiveCos (proj, -light->direction[3]);
				break;
		}
		mmulf (lctx->light_mats.a[i], proj, view);
	}
}

static int
light_compare (const void *_li2, const void *_li1, void *_ldata)
{
	const int *li1 = _li1;
	const int *li2 = _li2;
	lightingdata_t *ldata = _ldata;
	const light_t *l1 = &ldata->lights.a[*li1];
	const light_t *l2 = &ldata->lights.a[*li2];
	int         s1 = abs ((int) l1->color[3]);
	int         s2 = abs ((int) l2->color[3]);

	if (s1 == s2) {
		return (l1->position[3] == l2->position[3])
			&& (l1->direction[3] > -0.5) == (l2->direction[3] > -0.5);
	}
	return s1 - s2;
}

static VkImageView
create_view (const light_renderer_t *lr, int id, vulkan_ctx_t *ctx)
{
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkImageViewType type = 0;
	const char *viewtype = 0;

	switch (lr->mode) {
		case ST_NONE:
			return 0;
		case ST_PLANE:
			type = VK_IMAGE_VIEW_TYPE_2D;
			viewtype = "plane";
			break;
		case ST_CASCADE:
			type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewtype = "cascade";
			break;
		case ST_CUBE:
			type = VK_IMAGE_VIEW_TYPE_CUBE;
			viewtype = "cube";
			break;
	}

	VkImageViewCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0,
		0,
		lr->image, type, VK_FORMAT_X8_D24_UNORM_PACK32,
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, lr->layer, lr->numLayers }
	};

	VkImageView view;
	dfunc->vkCreateImageView (device->dev, &createInfo, 0, &view);
	QFV_duSetObjectName (device, VK_OBJECT_TYPE_IMAGE_VIEW, view,
						 va (ctx->va_ctx, "iview:shadowmap:%s:%d",
							 viewtype, id));
	(void) viewtype;//silence unused warning when vulkan debug disabled
	return view;
}

static VkFramebuffer
create_framebuffer (const light_renderer_t *lr, vulkan_ctx_t *ctx)
{
	return 0;//FIXME
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	VkFramebuffer framebuffer;
	VkFramebufferCreateInfo cInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = lr->renderPass,
		.attachmentCount = 1,
		.pAttachments = &lr->view,
		.width = lr->size,
		.height = lr->size,
		.layers = 1,
	};
	dfunc->vkCreateFramebuffer (device->dev, &cInfo, 0, &framebuffer);
	return framebuffer;
}

static void
build_shadow_maps (lightingctx_t *lctx, vulkan_ctx_t *ctx)
{
	typedef struct {
		int         size;
		int         layers;
		int         cube;
	} mapdesc_t;

	qfv_device_t *device = ctx->device;
	qfv_physdev_t *physDev = device->physDev;
	int         maxLayers = physDev->properties->limits.maxImageArrayLayers;
	lightingdata_t *ldata = lctx->ldata;
	light_t    *lights = ldata->lights.a;
	int         numLights = ldata->lights.size;
	int         size = -1;
	int         numLayers = 0;
	int         totalLayers = 0;
	int        *imageMap = alloca (numLights * sizeof (int));
	int        *lightMap = alloca (numLights * sizeof (int));
	int         numMaps = 0;
	mapdesc_t  *maps = alloca (numLights * sizeof (mapdesc_t));

	for (int i = 0; i < numLights; i++) {
		lightMap[i] = i;
	}
	heapsort_r (lightMap, numLights, sizeof (int), light_compare, ldata);

	DARRAY_RESIZE (&lctx->light_renderers, numLights);
	for (int i = 0; i < numLights; i++) {
		int         layers = 1;
		int         li = lightMap[i];
		__auto_type lr = &lctx->light_renderers.a[li];
		*lr = (light_renderer_t) {};

		if (!lights[li].position[3]) {
			if (!VectorIsZero (lights[li].direction)) {
				lr->mode = ST_CASCADE;
			}
		} else {
			if (lights[li].direction[3] > -0.5) {
				lr->mode = ST_CUBE;
			} else {
				lr->mode = ST_PLANE;
			}
		}
		if (lr->mode == ST_CASCADE || lr->mode == ST_NONE) {
			// cascade shadows will be handled separately, and "none" has no
			// shadow map at all
			imageMap[li] = -1;
			continue;
		}
		if (lr->mode == ST_CUBE) {
			layers = 6;
		}
		if (size != abs ((int) lights[li].color[3])
			|| numLayers + layers > maxLayers) {
			if (numLayers) {
				maps[numMaps++] = (mapdesc_t) {
					.size = size,
					.layers = numLayers,
					.cube = 1,
				};
				numLayers = 0;
			}
			size = abs ((int) lights[li].color[3]);
		}
		imageMap[li] = numMaps;
		lr->size = size;
		lr->layer = numLayers;
		lr->numLayers = layers;
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		maps[numMaps++] = (mapdesc_t) {
			.size = size,
			.layers = numLayers,
			.cube = 1,
		};
	}

	numLayers = 0;
	size = 1024;
	for (int i = 0; i < numLights; i++) {
		int         layers = 4;
		int         li = lightMap[i];
		__auto_type lr = &lctx->light_renderers.a[li];

		if (lr->mode != ST_CASCADE) {
			continue;
		}
		if (numLayers + layers > maxLayers) {
			maps[numMaps++] = (mapdesc_t) {
				.size = size,
				.layers = numLayers,
				.cube = 0,
			};
			numLayers = 0;
		}
		imageMap[li] = numMaps;
		lr->size = size;
		lr->layer = numLayers;
		lr->numLayers = layers;
		numLayers += layers;
		totalLayers += layers;
	}
	if (numLayers) {
		maps[numMaps++] = (mapdesc_t) {
			.size = size,
			.layers = numLayers,
			.cube = 0,
		};
	}

	if (numMaps) {
		qfv_resource_t *shad = calloc (1, sizeof (qfv_resource_t)
									   + numMaps * sizeof (qfv_resobj_t));
		lctx->shadow_resources = shad;
		*shad = (qfv_resource_t) {
			.name = "shadow",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = numMaps,
			.objects = (qfv_resobj_t *) &shad[1],
		};
		for (int i = 0; i < numMaps; i++) {
			int         cube = maps[i].layers < 6 ? 0 : maps[i].cube;
			shad->objects[i] = (qfv_resobj_t) {
				.name = "map",
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
		}
		QFV_CreateResource (device, shad);
		for (int i = 0; i < numMaps; i++) {
			DARRAY_APPEND (&lctx->light_images, shad->objects[i].image.image);
		}
	}

	for (int i = 0; i < numLights; i++) {
		int         li = lightMap[i];
		__auto_type lr = &lctx->light_renderers.a[li];

		if (imageMap[li] == -1) {
			continue;
		}

		switch (lr->numLayers) {
			case 6:
				lr->renderPass = lctx->renderpass_6;
				break;
			case 4:
				lr->renderPass = lctx->renderpass_4;
				break;
			case 1:
				lr->renderPass = lctx->renderpass_1;
				break;
			default:
				Sys_Error ("build_shadow_maps: invalid light layer count: %u",
						   lr->numLayers);
		}
		lr->image = lctx->light_images.a[imageMap[li]];
		lr->view = create_view (lr, li, ctx);
		lr->framebuffer = create_framebuffer(lr, ctx);
	}
	Sys_MaskPrintf (SYS_vulkan,
					"shadow maps: %d layers in %zd images: %"PRId64"\n",
					totalLayers, lctx->light_images.size,
					lctx->shadow_resources->size);
}

void
Vulkan_LoadLights (scene_t *scene, vulkan_ctx_t *ctx)
{
	lightingctx_t *lctx = ctx->lighting_context;

	lctx->scene = scene;
	lctx->ldata = scene ? scene->lights : 0;

	clear_shadows (ctx);

	if (lctx->ldata && lctx->ldata->lights.size) {
		build_shadow_maps (lctx, ctx);
		create_light_matrices (lctx);
	}
}
