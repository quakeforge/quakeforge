/*
	qf_mesh.h

	Vulkan specific mesh rendering

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
#ifndef __QF_Vulkan_qf_mesh_h
#define __QF_Vulkan_qf_mesh_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

enum {
	mesh_main,
	mesh_shadow,
};

typedef struct mesh_vrt_s {
	float       vertex[4];
	float       normal[4];
} mesh_vrt_t;

typedef struct mesh_uv_s {
	float       uv[2];
} mesh_uv_t;

typedef struct qfv_mesh_s {
	VkBuffer    geom_buffer;
	VkBuffer    rend_buffer;
	VkBuffer    index_buffer;
	VkBuffer    bones_buffer;
	VkDeviceMemory bones_memory;
	uint32_t    resources;
	uint32_t    bone_descriptors;
} qfv_mesh_t;

typedef struct qfv_skin_s {
	VkDeviceMemory memory;
	VkImage     image;
	VkImageView view;
	byte        colors[4];
	VkDescriptorSet descriptor;
} qfv_skin_t;

typedef enum {
	QFV_meshDepth,
	QFV_meshGBuffer,
	QFV_meshTranslucent,

	QFV_meshNumPasses
} QFV_MeshSubpass;

typedef struct meshindset_s
    DARRAY_TYPE (unsigned) meshindset_t;

typedef struct meshctx_s {
	VkSampler    sampler;
	struct qfv_dsmanager_s *dsmanager;
	struct qfv_resource_s *resource;
	VkBuffer     null_bone;
	qfv_skin_t   default_skin;
} meshctx_t;

typedef struct vulkan_ctx_s vulkan_ctx_t;
typedef struct mod_alias_ctx_s mod_alias_ctx_t;
typedef struct mod_mesh_ctx_s mod_mesh_ctx_t;
typedef struct qf_model_s qf_model_t;

void Vulkan_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx, vulkan_ctx_t *ctx);
void Vulkan_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx,
									vulkan_ctx_t *ctx);
void Vulkan_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx,
								   vulkan_ctx_t *ctx);
void Vulkan_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx,
											void *_m, int _s, int extra,
											vulkan_ctx_t *ctx);
void Vulkan_Mod_MeshFinish (mod_mesh_ctx_t *mesh_ctx, vulkan_ctx_t *ctx);

void Vulkan_MeshAddBones (vulkan_ctx_t *ctx, qf_model_t *model);
void Vulkan_MeshRemoveBones (vulkan_ctx_t *ctx, qf_model_t *model);

void Vulkan_MeshAddSkin (vulkan_ctx_t *ctx, qfv_skin_t *skin);
void Vulkan_MeshRemoveSkin (vulkan_ctx_t *ctx, qfv_skin_t *skin);

void Vulkan_Mesh_Init (vulkan_ctx_t *ctx);

#endif//__QF_Vulkan_qf_mesh_h
