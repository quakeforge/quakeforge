/*
	qf_matrices.h

	Vulkan matrix "pass"

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/12/8

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
#ifndef __QF_Vulkan_qf_matrices_h
#define __QF_Vulkan_qf_matrices_h

#include "QF/darray.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/image.h"
#include "QF/simd/types.h"

typedef __attribute__((aligned(64))) struct qfv_matrix_buffer_s {
	// projection and view matrices (model is push constant)
	mat4f_t     Projection3d;
	mat4f_t     View[6];
	mat4f_t     Sky;
	mat4f_t     Projection2d;
	vec2f_t     ScreenSize;
} qfv_matrix_buffer_t;

typedef struct matrixframe_s {
	VkBuffer    buffer;
	VkDescriptorSet descriptors;
} matrixframe_t;

typedef struct matrixframeset_s
    DARRAY_TYPE (matrixframe_t) matrixframeset_t;

typedef struct matrixctx_s {
	qfv_matrix_buffer_t matrices;
	vec4f_t      sky_rotation[2];
	vec4f_t      sky_velocity;
	vec4f_t      sky_fix;
	double       sky_time;
	int             dirty;

	matrixframeset_t frames;

	struct qfv_resource_s *resource;
	struct qfv_stagebuf_s *stage;
} matrixctx_t;

struct vulkan_ctx_s;

void Vulkan_CalcViewMatrix (struct vulkan_ctx_s *ctx);
void Vulkan_SetViewMatrices (struct vulkan_ctx_s *ctx, mat4f_t views[],
							 int count);
void Vulkan_SetSkyMatrix (struct vulkan_ctx_s *ctx, mat4f_t sky);
void Vulkan_SetSkyMatrix (struct vulkan_ctx_s *ctx, mat4f_t sky);

void Vulkan_Matrix_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Matrix_Setup (struct vulkan_ctx_s *ctx);
void Vulkan_Matrix_Shutdown (struct vulkan_ctx_s *ctx);
VkDescriptorSet Vulkan_Matrix_Descriptors (struct vulkan_ctx_s *ctx, int frame)
	__attribute__((pure));

#endif//__QF_Vulkan_qf_matrices_h
