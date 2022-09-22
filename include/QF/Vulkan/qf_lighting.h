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
#include "QF/scene/light.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/qf_renderpass.h"
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/image.h"
#include "QF/simd/types.h"

typedef struct qfv_lightmatset_s DARRAY_TYPE (mat4f_t) qfv_lightmatset_t;

#define MaxLights   768

#define ST_NONE     0	// no shadows
#define ST_PLANE    1	// single plane shadow map (small spotlight)
#define ST_CASCADE  2	// cascaded shadow maps
#define ST_CUBE     3	// cubemap (omni, large spotlight)

typedef struct qfv_light_buffer_s {
	light_t     lights[MaxLights] __attribute__((aligned(16)));
	int         lightCount;
	//mat4f_t     shadowMat[MaxLights];
	//vec4f_t     shadowCascade[MaxLights];
} qfv_light_buffer_t;

#define LIGHTING_BUFFER_INFOS 1
#define LIGHTING_ATTACH_INFOS 5
#define LIGHTING_SHADOW_INFOS MaxLights
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
} lightingframe_t;

typedef struct lightingframeset_s
    DARRAY_TYPE (lightingframe_t) lightingframeset_t;

typedef struct light_renderer_s {
	VkRenderPass renderPass;	// shared
	VkFramebuffer framebuffer;
	VkImage     image;			// shared
	VkImageView view;
	uint32_t    size;
	uint32_t    layer;
	uint32_t    numLayers;
	int         mode;
} light_renderer_t;

typedef struct light_renderer_set_s
    DARRAY_TYPE (light_renderer_t) light_renderer_set_t;

typedef struct lightingctx_s {
	lightingframeset_t frames;
	VkPipeline   pipeline;
	VkPipelineLayout layout;
	VkSampler    sampler;
	VkDeviceMemory light_memory;
	VkDeviceMemory shadow_memory;
	qfv_lightmatset_t light_mats;
	qfv_imageset_t light_images;
	light_renderer_set_t light_renderers;

	qfv_renderpass_t *qfv_renderpass;
	VkRenderPass renderpass_6;
	VkRenderPass renderpass_4;
	VkRenderPass renderpass_1;

	VkCommandPool cmdpool;

	struct lightingdata_s *ldata;
	struct scene_s *scene;
} lightingctx_t;

struct vulkan_ctx_s;
struct qfv_renderframe_s;

void Vulkan_Lighting_CreateRenderPasses (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Draw (struct qfv_renderframe_s *rFrame);
void Vulkan_LoadLights (struct scene_s *scene, struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lighting_h
