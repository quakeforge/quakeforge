/*
	qf_compose.h

	Vulkan compose pass

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/2/24

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
#ifndef __QF_Vulkan_qf_compose_h
#define __QF_Vulkan_qf_compose_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

#define COMPOSE_IMAGE_INFOS 1

typedef struct composeframe_s {
	VkCommandBuffer cmd;
	VkDescriptorImageInfo imageInfo[COMPOSE_IMAGE_INFOS];
	VkWriteDescriptorSet descriptors[COMPOSE_IMAGE_INFOS];
} composeframe_t;

typedef struct composeframeset_s
    DARRAY_TYPE (composeframe_t) composeframeset_t;

typedef struct composectx_s {
	composeframeset_t frames;
	VkPipeline   pipeline;
	VkPipelineLayout layout;
} composectx_t;

struct vulkan_ctx_s;

void Vulkan_Compose_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Compose_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_compose_h
