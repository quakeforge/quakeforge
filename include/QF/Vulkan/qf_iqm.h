/*
	qf_iqm.h

	Vulkan specific iqm model stuff

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/5/3

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
#ifndef __QF_Vulkan_qf_iqm_h
#define __QF_Vulkan_qf_iqm_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/modelgen.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

// geometry attributes
typedef struct iqmgvert_s {
	float       vertex[3];
	byte        bones[4];	// uint
	byte        weights[4];	// unorm
} iqmgvert_t;

// rendering attributes
typedef struct iqmrvert_s {
	float       uv[2];
	float       normal[3];
	float       tangent[4];
	byte        color[4];	// unorm
} iqmrvert_t;

typedef struct qfv_iqm_skin_s {
	VkImageView view;
	byte        colora[4];
	byte        colorb[4];
	VkDescriptorSet descriptor;
} qfv_iqm_skin_t;

typedef struct qfv_iqm_s {
	VkBuffer    geom_buffer;
	VkBuffer    rend_buffer;
	VkBuffer    index_buffer;
	qfv_iqm_skin_t *skins;
	struct qfv_resource_s *mesh;
	struct qfv_resource_s *bones;
	VkBuffer     bones_buffer;
	VkDescriptorSet *bones_descriptors;	// one per frame FIXME per instance!!!
} qfv_iqm_t;

typedef enum {
	QFV_iqmDepth,
	QFV_iqmGBuffer,
	QFV_iqmTranslucent,

	QFV_iqmNumPasses
} QFV_IQMSubpass;

typedef struct iqm_frame_s {
	qfv_cmdbufferset_t cmdSet;
} iqm_frame_t;

typedef struct iqm_frameset_s
    DARRAY_TYPE (iqm_frame_t) iqm_frameset_t;

typedef struct iqmindset_s
    DARRAY_TYPE (unsigned) iqmindset_t;

typedef struct iqmctx_s {
	iqm_frameset_t frames;
	VkPipeline   depth;
	VkPipeline   gbuf;
	VkPipelineLayout layout;
	VkSampler    sampler;
	VkDescriptorPool bones_pool;
	VkDescriptorSetLayout bones_setLayout;
} iqmctx_t;

struct vulkan_ctx_s;
struct qfv_orenderframe_s;
struct entity_s;
struct mod_iqm_ctx_s;
struct iqm_s;

void Vulkan_Mod_IQMFinish (struct model_s *mod, struct vulkan_ctx_s *ctx);

void Vulkan_IQMAddBones (struct vulkan_ctx_s *ctx, struct iqm_s *iqm);
void Vulkan_IQMRemoveBones (struct vulkan_ctx_s *ctx, struct iqm_s *iqm);

void Vulkan_IQMAddSkin (struct vulkan_ctx_s *ctx, qfv_iqm_skin_t *skin);
void Vulkan_IQMRemoveSkin (struct vulkan_ctx_s *ctx, qfv_iqm_skin_t *skin);

void Vulkan_IQM_Init (struct vulkan_ctx_s *ctx);
void Vulkan_IQM_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_iqm_h
