/*
	vulkan_planes.c

	Vulkan debug planes pipeline

	Copyright (C) 2023       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/7/20

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

#include "QF/simd/types.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/qf_planes.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/staging.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct {
	vec4f_t     p[3];
	vec4f_t     scolor;
	vec4f_t     tcolor;
} qfv_plane_t;

typedef struct {
	int         num_planes;
	qfv_plane_t planes[64];
} qfv_planebuf_t;

static qfv_plane_t
make_plane (vec4f_t s, vec4f_t t, vec4f_t scolor, vec4f_t tcolor)
{
	vec4f_t n = crossf (s, t);
	vec4f_t d = -r_refdef.camera[3];
	vec4f_t S = dotf (t, t) * s - dotf (s, t) * t;
	vec4f_t T = dotf (s, s) * t - dotf (s, t) * s;
	vec4f_t dn = dotf (d, n);
	vec4f_t Sd = dotf (S, d);
	vec4f_t Td = dotf (T, d);
	vec4f_t SS = dn * S - Sd * n;
	vec4f_t TT = dn * T - Td * n;
	vec4f_t nn = dotf (n, n) * n;

	return (qfv_plane_t) {
		.p = {
			{SS[0], TT[0], nn[0], dn[0]},
			{SS[1], TT[1], nn[1], 0},
			{SS[2], TT[2], nn[2], 0},
		},
		.scolor = scolor,
		.tcolor = tcolor,
	};
}

static void
debug_planes_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;

	auto pctx = ctx->planes_context;
	auto pframe = &pctx->frames.a[ctx->curFrame];
	auto layout = taskctx->pipeline->layout;
	auto cmd = taskctx->cmd;

	auto packet = QFV_PacketAcquire (ctx->staging);
	qfv_planebuf_t *planes = QFV_PacketExtend (packet, sizeof (qfv_planebuf_t));
	vec4f_t x = {256, 0, 0, 0};
	vec4f_t y = {0, 256, 0, 0};
	vec4f_t z = {0, 0, 256, 0};
	vec4f_t r = {1, 0, 0, 1 };
	vec4f_t g = {0, 1, 0, 1 };
	vec4f_t b = {0, 0, 1, 1 };
	*planes = (qfv_planebuf_t) {
		.num_planes = 3,
		.planes[0] = make_plane (x, y, r, g),
		.planes[1] = make_plane (y, z, g, b),
		.planes[2] = make_plane (z, x, b, r),
	};
	auto buffer = &pctx->resources->objects[ctx->curFrame].buffer;
	auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
	QFV_PacketCopyBuffer (packet, buffer->buffer, 0, bb);
	QFV_PacketSubmit (packet);

	VkDescriptorSet sets[] = {
		Vulkan_Matrix_Descriptors (ctx, ctx->curFrame),
		pframe->descriptor,
	};
	dfunc->vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
									layout, 0, 2, sets, 0, 0);
	dfunc->vkCmdDraw (cmd, 3, 1, 0, 0);
}

static exprtype_t *debug_planes_draw_params[] = {
	&cexpr_int,
};
static exprfunc_t debug_planes_draw_func[] = {
	{ .func = debug_planes_draw, .num_params = 1, .param_types = debug_planes_draw_params },
	{}
};
static exprsym_t debug_planes_task_syms[] = {
	{ "debug_planes_draw", &cexpr_function, debug_planes_draw_func },
	{}
};

void
Vulkan_Planes_Init (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "debug planes init");
	QFV_Render_AddTasks (ctx, debug_planes_task_syms);

	planesctx_t *pctx = calloc (1, sizeof (planesctx_t));
	ctx->planes_context = pctx;

	qfvPopDebug (ctx);
}

void
Vulkan_Planes_Setup (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto pctx = ctx->planes_context;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&pctx->frames, frames);
	DARRAY_RESIZE (&pctx->frames, frames);
	pctx->frames.grow = 0;

	pctx->resources = malloc (sizeof (qfv_resource_t)
							  + sizeof (qfv_resobj_t[frames]));
	*pctx->resources = (qfv_resource_t) {
		.name = "planes",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames,
		.objects = (qfv_resobj_t *) &pctx->resources[1],
	};
	for (size_t i = 0; i < frames; i++) {
		auto obj = &pctx->resources->objects[i];
		*obj = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "planes:%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_planebuf_t),
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
						| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			},
		};
	}
	QFV_CreateResource (device, pctx->resources);

	auto packet = QFV_PacketAcquire (ctx->staging);
	qfv_planebuf_t *planes = QFV_PacketExtend (packet, sizeof (qfv_planebuf_t));
	*planes = (qfv_planebuf_t) {
		.num_planes = 3,
		.planes[0] = {{
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
		}},
		.planes[1] = {{
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{1, 0, 0, 0},
		}},
		.planes[2] = {{
			{0, 0, 1, 0},
			{1, 0, 0, 0},
			{0, 1, 0, 0},
		}},
	};
	for (size_t i = 0; i < frames; i++) {
		auto buffer = &pctx->resources->objects[i].buffer;
		auto bb = &bufferBarriers[qfv_BB_TransferWrite_to_UniformRead];
		QFV_PacketCopyBuffer (packet, buffer->buffer, 0, bb);
	}
	QFV_PacketSubmit (packet);

	auto dsmanager = QFV_Render_DSManager (ctx, "planes_set");
	for (size_t i = 0; i < frames; i++) {
		auto pframe = &pctx->frames.a[i];
		auto buffer = &pctx->resources->objects[i].buffer;
		auto set = QFV_DSManager_AllocSet (dsmanager);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_DESCRIPTOR_SET, set,
							 va (ctx->va_ctx, "debug:planes_set:%zd", i));

		pframe->descriptor = set;
		VkDescriptorBufferInfo bufferInfo[] = {
			{ buffer->buffer, 0, VK_WHOLE_SIZE },
		};
		VkWriteDescriptorSet write[] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = set,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = bufferInfo,
			},
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
	}
}

void
Vulkan_Planes_Shutdown (vulkan_ctx_t *ctx)
{
	auto device = ctx->device;
	auto pctx = ctx->planes_context;

	QFV_DestroyResource (device, pctx->resources);

	free (pctx->frames.a);
	free (pctx);
}
