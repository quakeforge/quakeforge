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
#include "QF/Vulkan/command.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/render.h"
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
#define LIGHTING_SHADOW_INFOS 32
#define LIGHTING_DESCRIPTORS (LIGHTING_BUFFER_INFOS + LIGHTING_ATTACH_INFOS + 1)

typedef struct qfv_framebufferset_s
	DARRAY_TYPE (VkFramebuffer) qfv_framebufferset_t;

typedef struct lightingframe_s {
	VkDescriptorSet shadowmat_set;
	VkDescriptorSet lights_set;
	VkDescriptorSet attach_set;

	VkBuffer    shadowmat_buffer;
	VkBuffer    light_buffer;
	VkBuffer    id_buffer;
	uint32_t    ico_count;
	uint32_t    cone_count;
	uint32_t    flat_count;

	qfv_imageviewset_t views;
	qfv_framebufferset_t framebuffers;
} lightingframe_t;

typedef struct lightingframeset_s
    DARRAY_TYPE (lightingframe_t) lightingframeset_t;

typedef struct light_renderer_s {
	uint8_t     renderpass_index;
	uint8_t     image_index;
	uint16_t    size;
	uint16_t    layer;
	uint8_t     numLayers;
	uint8_t     mode;
	uint16_t    matrix_base;
} light_renderer_t;

typedef struct light_renderer_set_s
	DARRAY_TYPE (light_renderer_t) light_renderer_set_t;


typedef struct lightingctx_s {
	lightingframeset_t frames;
	VkSampler    sampler;
	struct qfv_resource_s *shadow_resources;
	struct qfv_resource_s *light_resources;

	qfv_lightmatset_t light_mats;
	qfv_imageset_t light_images;

	light_renderer_set_t light_renderers;

	qfv_attachmentinfo_t shadow_info;

	VkBuffer splat_verts;
	VkBuffer splat_inds;

	struct lightingdata_s *ldata;
	struct scene_s *scene;
} lightingctx_t;

struct vulkan_ctx_s;

void Vulkan_Lighting_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Setup (struct vulkan_ctx_s *ctx);
void Vulkan_Lighting_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_LoadLights (struct scene_s *scene, struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lighting_h
