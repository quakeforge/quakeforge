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
#include "QF/simd/types.h"

typedef struct qfv_light_s {
	vec3_t      color;
	int         data;
	vec3_t      position;
	float       light;
	vec3_t      direction;
	float       cone;
} qfv_light_t;

typedef struct qfv_lightset_s DARRAY_TYPE (qfv_light_t) qfv_lightset_t;
typedef struct qfv_lightleafset_s DARRAY_TYPE (int) qfv_lightleafset_t;
typedef struct qfv_lightvisset_s DARRAY_TYPE (byte) qfv_lightvisset_t;

#define NUM_LIGHTS 256
#define NUM_STYLES 64

typedef struct qfv_light_buffer_s {
	float       intensity[NUM_STYLES + 4];
	float       distFactor1;
	float       distFactor2;
	int         lightCount;
	qfv_light_t lights[NUM_LIGHTS] __attribute__((aligned(16)));
	mat4f_t     shadowMat[NUM_LIGHTS];
	vec4f_t     shadowCascade[NUM_LIGHTS];
} qfv_light_buffer_t;

#define LIGHTING_BUFFER_INFOS 1
#define LIGHTING_ATTACH_INFOS 5
#define LIGHTING_SHADOW_INFOS NUM_LIGHTS
#define LIGHTING_DESCRIPTORS (LIGHTING_BUFFER_INFOS + LIGHTING_ATTACH_INFOS + 1)

typedef struct lightingframe_s {
	VkCommandBuffer cmd;
	VkBuffer    light_buffer;
	VkDescriptorBufferInfo bufferInfo[LIGHTING_BUFFER_INFOS];
	VkDescriptorImageInfo attachInfo[LIGHTING_ATTACH_INFOS];
	VkDescriptorImageInfo shadowInfo[LIGHTING_SHADOW_INFOS];
	union {
		VkWriteDescriptorSet descriptors[LIGHTING_DESCRIPTORS];
		struct {
			VkWriteDescriptorSet bufferWrite[LIGHTING_BUFFER_INFOS];
			VkWriteDescriptorSet attachWrite[LIGHTING_ATTACH_INFOS];
			VkWriteDescriptorSet shadowWrite;
		};
	};
	// A fat PVS of leafs visible from visible leafs so hidden lights can
	// illuminate the leafs visible to the player
	byte        pvs[MAP_PVS_BYTES];
	struct mleaf_s *leaf;	// the last leaf used to generate the pvs
	qfv_lightvisset_t lightvis;
} lightingframe_t;

typedef struct lightingframeset_s
    DARRAY_TYPE (lightingframe_t) lightingframeset_t;

typedef struct lightingctx_s {
	lightingframeset_t frames;
	VkPipeline   pipeline;
	VkPipelineLayout layout;
	VkSampler    sampler;
	VkDeviceMemory light_memory;
	qfv_lightset_t lights;
	qfv_lightleafset_t lightleafs;
} lightingctx_t;

struct vulkan_ctx_s;

void Vulkan_Lighting_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Draw (struct vulkan_ctx_s *ctx);
void Vulkan_LoadLights (model_t *model, const char *entity_data,
						struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lighting_h
