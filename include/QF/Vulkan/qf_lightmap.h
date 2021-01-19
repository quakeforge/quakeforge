/*
	qf_lightmap.h

	Vulkan lightmap stuff from the renderer.

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_Vulkan_qf_lightmap_h
#define __QF_Vulkan_qf_lightmap_h

#define MAX_LIGHTMAPS	1024
#define BLOCK_WIDTH		64
#define BLOCK_HEIGHT	64

#include "QF/Vulkan/qf_vid.h"

struct vulkan_ctx_s;
struct model_s;
struct msurface_s;

void Vulkan_lightmap_init (struct vulkan_ctx_s *ctx);
void Vulkan_BuildLightmaps (struct model_s **models, int num_models, struct vulkan_ctx_s *ctx);
void Vulkan_CalcLightmaps (struct vulkan_ctx_s *ctx);
void Vulkan_BuildLightMap (struct msurface_s *surf, struct vulkan_ctx_s *ctx);
VkImageView Vulkan_LightmapImageView (struct vulkan_ctx_s *ctx) __attribute__((pure));
void Vulkan_FlushLightmaps (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lightmap_h
