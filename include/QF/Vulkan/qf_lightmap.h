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

typedef struct vulkan_ctx_s vulkan_ctx_t;
typedef struct bspctx_s bspctx_t;
typedef struct model_s model_t;
typedef struct mod_brush_s mod_brush_t;
typedef struct msurface_s msurface_t;
typedef struct transform_s transform_t;
typedef struct subpic_s subpic_t;

void Vulkan_lightmap_init (struct vulkan_ctx_s *ctx);
void Vulkan_BuildLightmaps (model_t **models, int num_models, vulkan_ctx_t *ctx);
void Vulkan_BuildLightMap (const vec4f_t *transform, const mod_brush_t *brush, msurface_t *surf, vulkan_ctx_t *ctx);
VkImageView Vulkan_LightmapImageView (struct vulkan_ctx_s *ctx) __attribute__((pure));
void Vulkan_FlushLightmaps (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_lightmap_h
