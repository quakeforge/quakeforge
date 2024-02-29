/*
	qf_alias.h

	Vulkan specific alias model stuff

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
#include "QF/Vulkan/command.h"

enum {
	alias_main,
	alias_shadow,
};

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
	struct qfv_resource_s *resources;
	uint32_t    numtris;
} qfv_alias_mesh_t;

typedef struct qfv_alias_skin_s {
	VkDeviceMemory memory;
	VkImage     image;
	VkImageView view;
	byte        colors[4];
	VkDescriptorSet descriptor;
} qfv_alias_skin_t;

typedef enum {
	QFV_aliasDepth,
	QFV_aliasGBuffer,
	QFV_aliasTranslucent,

	QFV_aliasNumPasses
} QFV_AliasSubpass;

typedef struct aliasindset_s
    DARRAY_TYPE (unsigned) aliasindset_t;

typedef struct aliasctx_s {
	VkSampler    sampler;
} aliasctx_t;

struct vulkan_ctx_s;
struct entity_s;
struct mod_alias_ctx_s;

void Vulkan_Mod_LoadAllSkins (struct mod_alias_ctx_s *alias_ctx,
							  struct vulkan_ctx_s *ctx);
void Vulkan_Mod_FinalizeAliasModel (struct mod_alias_ctx_s *alias_ctx,
									struct vulkan_ctx_s *ctx);
void Vulkan_Mod_LoadExternalSkins (struct mod_alias_ctx_s *alias_ctx,
								   struct vulkan_ctx_s *ctx);
void Vulkan_Mod_MakeAliasModelDisplayLists (struct mod_alias_ctx_s *alias_ctx,
											void *_m, int _s, int extra,
											struct vulkan_ctx_s *ctx);

void Vulkan_AliasAddSkin (struct vulkan_ctx_s *ctx, qfv_alias_skin_t *skin);
void Vulkan_AliasRemoveSkin (struct vulkan_ctx_s *ctx, qfv_alias_skin_t *skin);

void Vulkan_Alias_Init (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_alias_h
