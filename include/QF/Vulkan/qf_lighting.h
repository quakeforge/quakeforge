/*
	qf_lighting.h

	Vulkan lighting pass

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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
#ifndef __QF_Vulkan_qf_lighting_h
#define __QF_Vulkan_qf_lighting_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

typedef struct qfv_light_s {
	vec3_t      color;
	float       intensity;
	vec3_t      position;
	int         radius;
	vec3_t      direction;
	float       cone;
} qfv_light_t;

#define NUM_LIGHTS 128

typedef struct qfv_light_buffer_s {
	int         lightCount;
	qfv_light_t lights[NUM_LIGHTS] __attribute__((aligned(16)));
} qfv_light_buffer_t;


#define LIGHTING_BUFFER_INFOS 1
#define LIGHTING_IMAGE_INFOS 4

typedef struct lightingframe_s {
	VkCommandBuffer cmd;
	VkBuffer    light_buffer;
	VkDescriptorBufferInfo bufferInfo[LIGHTING_BUFFER_INFOS];
	VkDescriptorImageInfo imageInfo[LIGHTING_IMAGE_INFOS];
	VkWriteDescriptorSet descriptors[LIGHTING_BUFFER_INFOS
									 + LIGHTING_IMAGE_INFOS];
} lightingframe_t;

typedef struct lightingframeset_s
    DARRAY_TYPE (lightingframe_t) lightingframeset_t;

typedef struct lightingctx_s {
	lightingframeset_t frames;
	VkPipeline   pipeline;
	VkPipelineLayout layout;
	VkDeviceMemory light_memory;
} lightingctx_t;

struct vulkan_ctx_s;

void Vulkan_Lighting_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Draw (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lighting_h
