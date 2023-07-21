/*
	qf_planes.h

	Vulkan debug planes rendering

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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
#ifndef __QF_Vulkan_qf_planes_h
#define __QF_Vulkan_qf_planes_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

typedef struct planes_frame_s {
	VkDescriptorSet descriptor;
} planes_frame_t;

typedef struct planes_frameset_s
    DARRAY_TYPE (planes_frame_t) planes_frameset_t;

typedef struct planesctx_s {
	planes_frameset_t frames;
	VkSampler    sampler;
	struct qfv_dsmanager_s *dsmanager;
	struct qfv_resource_s *resources;
} planesctx_t;

struct vulkan_ctx_s;
struct entity_s;
struct mod_planes_ctx_s;
struct planes_s;

void Vulkan_Planes_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Planes_Setup (struct vulkan_ctx_s *ctx);
void Vulkan_Planes_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_planes_h
