/*
	qf_scene.h

	Vulkan specific scene stuff

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/5/24

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
#ifndef __QF_Vulkan_qf_scene_h
#define __QF_Vulkan_qf_scene_h

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "QF/darray.h"
#include "QF/simd/types.h"

typedef struct entdata_s {
	// transpose of entity transform matrix without row 4
	vec4f_t     xform[3];
	vec4f_t     color;
} entdata_t;

typedef struct entdataset_s
    DARRAY_TYPE (entdata_t) entdataset_t;

typedef struct scnframe_s {
	// used to check if the entity has been pooled this frame (cleared
	// every frame)
	struct set_s *pooled_entities;
	// data for entities pooled this frame (cleared every frame). transfered
	// to gpu
	entdataset_t entity_pool;
	VkDescriptorSet descriptors;
} scnframe_t;

typedef struct scnframeset_s
    DARRAY_TYPE (scnframe_t) scnframeset_t;

typedef struct scenectx_s {
	struct qfv_resource_s *entities;
	scnframeset_t frames;
	VkDescriptorPool pool;
	VkDescriptorSetLayout setLayout;
	int         max_entities;
} scenectx_t;

struct vulkan_ctx_s;
struct entity_s;

void Vulkan_Scene_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Scene_Shutdown (struct vulkan_ctx_s *ctx);
int Vulkan_Scene_MaxEntities (struct vulkan_ctx_s *ctx) __attribute__((pure));
VkDescriptorSet Vulkan_Scene_Descriptors (struct vulkan_ctx_s *ctx) __attribute__((pure));
int Vulkan_Scene_AddEntity (struct vulkan_ctx_s *ctx, struct entity_s *entity);
void Vulkan_Scene_Flush (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_scene_h
