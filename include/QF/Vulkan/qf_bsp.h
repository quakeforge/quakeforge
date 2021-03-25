/*
	qf_bsp.h

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
#ifndef __QF_Vulkan_qf_bsp_h
#define __QF_Vulkan_qf_bsp_h

#include "QF/darray.h"
#include "QF/model.h"
#include "QF/Vulkan/qf_vid.h"
#include "QF/Vulkan/command.h"

typedef struct bspvert_s {
	quat_t      vertex;
	quat_t      tlst;
} bspvert_t;

typedef struct elements_s {
	struct elements_s *_next;
	struct elements_s *next;
	uint32_t    first_index;
	uint32_t    index_count;
} elements_t;

typedef struct elechain_s {
	struct elechain_s *_next;
	struct elechain_s *next;
	int         index;
	elements_t *elements;
	vec_t      *transform;
	float      *color;
} elechain_t;

typedef enum {
	qfv_bsp_texture,
	qfv_bsp_glowmap,
	qfv_bsp_lightmap,
	qfv_bsp_skysheet,
	qfv_bsp_skycube,
} qfv_bsp_tex;
// view matrix
#define BSP_BUFFER_INFOS 1
// Texture, GlowMap, LightMap, SkySheet, SkyCube
#define BSP_IMAGE_INFOS 5

typedef enum {
	QFV_bspDepth,
	QFV_bspGBuffer,
	QFV_bspSky,
	QFV_bspTurb,

	QFV_bspNumPasses
} QFV_BspSubpass;

typedef struct bspframe_s {
	uint32_t    *index_data;	// pointer into mega-buffer for this frame (c)
	uint32_t     index_offset;	// offset of index_data within mega-buffer (c)
	uint32_t     index_count;	// number if indices queued (d)
	qfv_cmdbufferset_t cmdSet;
	VkDescriptorBufferInfo bufferInfo[BSP_BUFFER_INFOS];
	VkDescriptorImageInfo imageInfo[BSP_IMAGE_INFOS];
	VkWriteDescriptorSet descriptors[BSP_BUFFER_INFOS + BSP_IMAGE_INFOS];
} bspframe_t;

typedef struct fragconst_s {
	quat_t      fog;
	float       time;
} fragconst_t;

typedef struct bspframeset_s
    DARRAY_TYPE (bspframe_t) bspframeset_t;

typedef struct texchainset_s
    DARRAY_TYPE (struct vulktex_s *) texchainset_t;

typedef struct bspctx_s {
	instsurf_t  *waterchain;
	instsurf_t **waterchain_tail;
	instsurf_t  *sky_chain;
	instsurf_t **sky_chain_tail;

	texchainset_t texture_chains;

	// for world and non-instance models
	instsurf_t  *static_instsurfs;
	instsurf_t **static_instsurfs_tail;
	instsurf_t  *free_static_instsurfs;

	// for instance models
	elechain_t  *elechains;
	elechain_t **elechains_tail;
	elechain_t  *free_elechains;
	elements_t  *elementss;
	elements_t **elementss_tail;
	elements_t  *free_elementss;
	instsurf_t  *instsurfs;
	instsurf_t **instsurfs_tail;
	instsurf_t  *free_instsurfs;

	struct qfv_tex_s *default_skysheet;
	struct qfv_tex_s *skysheet_tex;

	struct qfv_tex_s *default_skybox;
	struct qfv_tex_s *skybox_tex;
	quat_t       sky_rotation[2];
	quat_t       sky_velocity;
	quat_t       sky_fix;
	double       sky_time;

	quat_t       default_color;
	quat_t       last_color;

	struct scrap_s *light_scrap;
	struct qfv_stagebuf_s *light_stage;

	struct bsppoly_s *polys;

	VkSampler    sampler;
	VkDeviceMemory texture_memory;
	VkPipeline   depth;
	VkPipeline   gbuf;
	VkPipeline   skysheet;
	VkPipeline   skybox;
	VkPipeline   turb;
	VkPipelineLayout layout;
	size_t       vertex_buffer_size;
	size_t       index_buffer_size;
	VkBuffer     vertex_buffer;
	VkDeviceMemory vertex_memory;
	VkBuffer     index_buffer;
	VkDeviceMemory index_memory;
	bspframeset_t frames;
} bspctx_t;

struct vulkan_ctx_s;
void Vulkan_ClearElements (struct vulkan_ctx_s *ctx);
void Vulkan_DrawWorld (struct vulkan_ctx_s *ctx);
void Vulkan_DrawSky (struct vulkan_ctx_s *ctx);
void Vulkan_LoadSkys (const char *sky, struct vulkan_ctx_s *ctx);
void Vulkan_RegisterTextures (model_t **models, int num_models,
							  struct vulkan_ctx_s *ctx);
void Vulkan_BuildDisplayLists (model_t **models, int num_models,
							   struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Shutdown (struct vulkan_ctx_s *ctx);
void Vulkan_DrawWaterSurfaces (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_bsp_h
