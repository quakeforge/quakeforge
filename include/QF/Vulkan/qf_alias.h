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
#include "QF/Vulkan/command.h"

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

typedef struct qfv_alias_skin_s {
	VkDeviceMemory memory;
	VkImage     image;
	VkImageView view;
	byte        colora[4];
	byte        colorb[4];
} qfv_alias_skin_t;

#define ALIAS_BUFFER_INFOS 1
#define ALIAS_IMAGE_INFOS 1

enum {
	QFV_aliasDepth,
	QFV_aliasGBuffer,
	//QFV_aliasTranslucent,

	QFV_aliasNumPasses
};

typedef struct aliasframe_s {
	qfv_cmdbufferset_t cmdSet;
	VkDescriptorBufferInfo bufferInfo[ALIAS_BUFFER_INFOS];
	VkDescriptorImageInfo imageInfo[ALIAS_IMAGE_INFOS];
	VkWriteDescriptorSet descriptors[ALIAS_BUFFER_INFOS + ALIAS_IMAGE_INFOS];
} aliasframe_t;

typedef struct aliasframeset_s
    DARRAY_TYPE (aliasframe_t) aliasframeset_t;

typedef struct aliasctx_s {
	aliasframeset_t frames;
	VkPipeline   depth;
	VkPipeline   gbuf;
	VkPipelineLayout layout;
	VkSampler    sampler;
} aliasctx_t;

struct vulkan_ctx_s;
struct entity_s;
struct mod_alias_ctx_s;
void *Vulkan_Mod_LoadSkin (struct mod_alias_ctx_s *alias_ctx, byte *skin,
						   int skinsize, int snum, int gnum, qboolean group,
						   maliasskindesc_t *skindesc,
						   struct vulkan_ctx_s *ctx);
void Vulkan_Mod_FinalizeAliasModel (struct mod_alias_ctx_s *alias_ctx,
									struct vulkan_ctx_s *ctx);
void Vulkan_Mod_LoadExternalSkins (struct mod_alias_ctx_s *alias_ctx,
								   struct vulkan_ctx_s *ctx);
void Vulkan_Mod_MakeAliasModelDisplayLists (struct mod_alias_ctx_s *alias_ctx,
											void *_m, int _s, int extra,
											struct vulkan_ctx_s *ctx);

void Vulkan_AliasBegin (struct vulkan_ctx_s *ctx);
void Vulkan_DrawAlias (struct entity_s *ent, struct vulkan_ctx_s *ctx);
void Vulkan_AliasEnd (struct vulkan_ctx_s *ctx);

void Vulkan_Alias_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Alias_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_alias_h
