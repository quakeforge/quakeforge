/*
	qf_alias.h

	Vulkan specific brush model stuff

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/7
	Date: 2021/1/18

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
#ifndef __QF_Vulkan_qf_alias_h
#define __QF_Vulkan_qf_alias_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"

typedef struct aliasvrt_s {
	float       vertex[4];
	float       normal[4];
} aliasvrt_t;

typedef struct aliasuv_s {
	float       u, v;
} aliasuv_t;

typedef struct qfv_alias_mesh_s {
	VkBuffer    vertex_buffer;
	VkBuffer    uv_buffer;
	VkBuffer    index_buffer;
	VkDeviceMemory memory;
} qfv_alias_mesh_t;

typedef struct qfv_light_s {
	vec3_t      color;
	float       dist;
	vec3_t      position;
	int         type;
	vec3_t      direction;
	float       cone;
} qfv_light_t;

typedef struct qfv_light_buffer_s {
	int         light_count;
	qfv_light_t lights[] __attribute__((aligned(16)));
} qfv_light_buffer_t;

typedef struct alias_ctx_s {
} alias_ctx_t;

struct vulkan_ctx_s;
void *Vulkan_Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum,
		                   qboolean group, maliasskindesc_t *skindesc,
						   struct vulkan_ctx_s *ctx);
void Vulkan_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr,
									struct vulkan_ctx_s *ctx);
void Vulkan_Mod_LoadExternalSkins (model_t *mod, struct vulkan_ctx_s *ctx);
void Vulkan_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr,
											void *_m, int _s, int extra,
											struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_alias_h
