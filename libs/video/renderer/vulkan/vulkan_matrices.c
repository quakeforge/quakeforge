/*
	vid_common_vulkan.c

	Common Vulkan video driver functions

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/Vulkan/qf_matrices.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/buffer.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/projection.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/staging.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static void
setup_view (vulkan_ctx_t *ctx)
{
	//FIXME this should check for cube map rather than fisheye
	if (scr_fisheye) {
		__auto_type mctx = ctx->matrix_context;
		QFV_PerspectiveTan (mctx->matrices.Projection3d, 1, 1, r_nearclip);
		mat4f_t     views[6];
		for (int i = 0; i < 6; i++) {
			mat4f_t     rotinv;
			mat4ftranspose (rotinv, qfv_box_rotations[i]);
			mmulf (views[i], rotinv, r_refdef.camera_inverse);
			mmulf (views[i], qfv_z_up, views[i]);
		}
		Vulkan_SetViewMatrices (ctx, views, 6);
	} else {
		mat4f_t     view;
		mmulf (view, qfv_z_up, r_refdef.camera_inverse);
		Vulkan_SetViewMatrices (ctx, &view, 1);
	}
}

static void
setup_sky (vulkan_ctx_t *ctx)
{
	__auto_type mctx = ctx->matrix_context;
	vec4f_t     q;
	mat4f_t     m;
	float       blend;
	mat4f_t     mat;

	while (vr_data.realtime - mctx->sky_time > 1) {
		mctx->sky_rotation[0] = mctx->sky_rotation[1];
		mctx->sky_rotation[1] = qmulf (mctx->sky_velocity,
									   mctx->sky_rotation[0]);
		mctx->sky_time += 1;
	}
	blend = bound (0, (vr_data.realtime - mctx->sky_time), 1);

	q = Blend (mctx->sky_rotation[0], mctx->sky_rotation[1], blend);
	q = normalf (qmulf (mctx->sky_fix, q));
	mat4fidentity (mat);
	VectorNegate (r_refdef.frame.position, mat[3]);
	mat4fquat (m, q);
	mmulf (mat, m, mat);
	Vulkan_SetSkyMatrix (ctx, mat);
}

void
Vulkan_SetViewMatrices (vulkan_ctx_t *ctx, mat4f_t views[], int count)
{
	__auto_type mctx = ctx->matrix_context;

	memcpy (mctx->matrices.View, views, count * sizeof (mat4f_t));
	mctx->dirty = mctx->frames.size;
}

void
Vulkan_SetSkyMatrix (vulkan_ctx_t *ctx, mat4f_t sky)
{
	__auto_type mctx = ctx->matrix_context;

	if (memcmp (mctx->matrices.Sky, sky, sizeof (mat4f_t))) {
		memcpy (mctx->matrices.Sky, sky, sizeof (mat4f_t));
		mctx->dirty = mctx->frames.size;
	}
}

static void
update_matrices (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto device = ctx->device;
	auto dfunc = device->funcs;
	auto mctx = ctx->matrix_context;
	auto mframe = &mctx->frames.a[ctx->curFrame];

	setup_view (ctx);
	setup_sky (ctx);

	if (mctx->dirty <= 0) {
		mctx->dirty = 0;
		return;
	}

	mctx->dirty--;

	qfv_packet_t *packet = QFV_PacketAcquire (mctx->stage);
	qfv_matrix_buffer_t *m = QFV_PacketExtend (packet, sizeof (*m));
	*m = mctx->matrices;

	qfv_bufferbarrier_t bb = bufferBarriers[qfv_BB_Unknown_to_TransferWrite];		bb.barrier.buffer = mframe->buffer;
	bb.barrier.size = packet->length;

	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);

	VkBufferCopy copy_region = { packet->offset, 0, packet->length };
	dfunc->vkCmdCopyBuffer (packet->cmd, mctx->stage->buffer,
							mframe->buffer, 1, &copy_region);

	bb = bufferBarriers[qfv_LT_TransferDst_to_ShaderReadOnly];
	bb.barrier.buffer = mframe->buffer;
	bb.barrier.size = packet->length;

	dfunc->vkCmdPipelineBarrier (packet->cmd, bb.srcStages, bb.dstStages,
								 0, 0, 0, 1, &bb.barrier, 0, 0);

	QFV_PacketSubmit (packet);
}

static exprfunc_t update_matrices_func[] = {
	{ .func = update_matrices },
	{}
};
static exprsym_t matrix_task_syms[] = {
	{ "update_matrices", &cexpr_function, update_matrices_func },
	{}
};

void
Vulkan_Matrix_Init (vulkan_ctx_t *ctx)
{
	QFV_Render_AddTasks (ctx, matrix_task_syms);

	matrixctx_t *mctx = calloc (1, sizeof (matrixctx_t));
	ctx->matrix_context = mctx;
}

void
Vulkan_Matrix_Setup (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "matrix init");
	qfv_device_t *device = ctx->device;
	qfv_devfuncs_t *dfunc = device->funcs;

	auto mctx = ctx->matrix_context;
	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&mctx->frames, frames);
	DARRAY_RESIZE (&mctx->frames, frames);
	mctx->frames.grow = 0;

	mctx->resource = malloc (sizeof (qfv_resource_t)
							 + sizeof (qfv_resobj_t[frames]));	// buffers
	auto buffers = (qfv_resobj_t *) &mctx->resource[1];
	*mctx->resource = (qfv_resource_t) {
		.name = "matrix",
		.va_ctx = ctx->va_ctx,
		.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.num_objects = frames,
		.objects = buffers,
	};
	for (size_t i = 0; i < frames; i++) {
		buffers[i] = (qfv_resobj_t) {
			.name = va (ctx->va_ctx, "%zd", i),
			.type = qfv_res_buffer,
			.buffer = {
				.size = sizeof (qfv_matrix_buffer_t),
				.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
						 | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			},
		};
	}
	QFV_CreateResource (device, mctx->resource);

	auto dsmanager = QFV_Render_DSManager (ctx, "matrix_set");
	for (size_t i = 0; i < frames; i++) {
		auto mframe = &mctx->frames.a[i];
		mframe->buffer = buffers[i].buffer.buffer;
		mframe->descriptors = QFV_DSManager_AllocSet (dsmanager);
		VkDescriptorBufferInfo bufferInfo = {
			mframe->buffer, 0, VK_WHOLE_SIZE
		};
		VkWriteDescriptorSet write[] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0,
			  mframe->descriptors, 0, 0, 1,
			  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			  0, &bufferInfo, 0 },
		};
		dfunc->vkUpdateDescriptorSets (device->dev, 1, write, 0, 0);
	}

	mctx->sky_fix = (vec4f_t) { 0, 0, 1, 1 } * sqrtf (0.5);
	mctx->sky_rotation[0] = (vec4f_t) { 0, 0, 0, 1};
	mctx->sky_rotation[1] = mctx->sky_rotation[0];
	mctx->sky_velocity = (vec4f_t) { };
	mctx->sky_velocity = qexpf (mctx->sky_velocity);
	mctx->sky_time = vr_data.realtime;

	mat4fidentity (mctx->matrices.Projection3d);
	for (int i = 0; i < 6; i++) {
		mat4fidentity (mctx->matrices.View[i]);
	}
	mat4fidentity (mctx->matrices.Sky);
	mat4fidentity (mctx->matrices.Projection2d);

	mctx->dirty = mctx->frames.size;

	mctx->stage = QFV_CreateStagingBuffer (device, "matrix",
										frames * sizeof (qfv_matrix_buffer_t),
										ctx->cmdpool);

	qfvPopDebug (ctx);
}

void
Vulkan_Matrix_Shutdown (vulkan_ctx_t *ctx)
{
	qfvPushDebug (ctx, "matrix shutdown");
	auto device = ctx->device;
	auto mctx = ctx->matrix_context;

	QFV_DestroyStagingBuffer (mctx->stage);
	QFV_DestroyResource (device, mctx->resource);

	qfvPopDebug (ctx);
}

VkDescriptorSet
Vulkan_Matrix_Descriptors (vulkan_ctx_t *ctx, int frame)
{
	auto mctx = ctx->matrix_context;
	return mctx->frames.a[frame].descriptors;
}
