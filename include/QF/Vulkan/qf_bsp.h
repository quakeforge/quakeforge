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

#include "QF/simd/types.h"

typedef struct bsp_face_s {
	uint32_t    first_index;
	uint32_t    index_count;
	uint32_t    tex_id;
	uint32_t    flags;
} bsp_face_t;

typedef struct bsp_packet_s {
	int         first_index;
	int         index_count;
	int         transform_id;
	int         color_id;
} bsp_packet_t;

typedef struct bsp_packetset_s
    DARRAY_TYPE (bsp_packet_t) bsp_packetset_t;

typedef struct bsp_indexset_s
    DARRAY_TYPE (uint32_t) bsp_indexset_t;

typedef struct bsp_tex_s {
	VkDescriptorSet descriptors;
	bsp_packetset_t packets;
	bsp_indexset_t indices;
} bsp_tex_t;

typedef struct vulktex_s {
	struct qfv_tex_s *tex;
	VkDescriptorSet descriptor;
	int         tex_id;
} vulktex_t;

typedef struct regtexset_s
    DARRAY_TYPE (vulktex_t *) regtexset_t;

typedef struct bsp_texset_s
    DARRAY_TYPE (bsp_tex_t) bsp_texset_t;

typedef struct bsp_draw_s {
	uint32_t    tex_id;
	uint32_t    index_count;
	uint32_t    instance_count;
	uint32_t    first_index;
	uint32_t    first_instance;
} bsp_draw_t;

typedef struct bsp_drawset_s
    DARRAY_TYPE (bsp_draw_t) bsp_drawset_t;

typedef struct instface_s {
	uint32_t    inst_id;
	uint32_t    face;
} instface_t;

typedef struct bsp_instfaceset_s
    DARRAY_TYPE (instface_t) bsp_instfaceset_t;

typedef struct bsp_pass_s {
	uint32_t   *indices;		// points into index buffer
	uint32_t    index_count;	// number of indices written to buffer
	uint32_t   *entid_data;
	uint32_t    entid_count;
	int         vis_frame;
	int        *face_frames;
	int        *leaf_frames;
	int        *node_frames;
	bsp_instfaceset_t *face_queue;
	regtexset_t *textures;
	int         num_queues;
	bsp_drawset_t *draw_queues;
} bsp_pass_t;

typedef struct bspvert_s {
	quat_t      vertex;
	quat_t      tlst;
} bspvert_t;

typedef enum {
	qfv_bsp_texture,
	qfv_bsp_glowmap,
	qfv_bsp_lightmap,
	qfv_bsp_skysheet,
	qfv_bsp_skycube,
} qfv_bsp_tex;

typedef enum {
	QFV_bspDepth,
	QFV_bspGBuffer,
	QFV_bspSky,
	QFV_bspTurb,

	QFV_bspNumPasses
} QFV_BspSubpass;

typedef struct bspframe_s {
	uint32_t   *index_data;		// pointer into mega-buffer for this frame (c)
	uint32_t    index_offset;	// offset of index_data within mega-buffer (c)
	uint32_t    index_count;	// number if indices queued (d)
	uint32_t   *entid_data;
	uint32_t    entid_offset;
	uint32_t    entid_count;
	qfv_cmdbufferset_t cmdSet;
} bspframe_t;

typedef struct bspframeset_s
    DARRAY_TYPE (bspframe_t) bspframeset_t;

typedef struct bspctx_s {
	uint32_t     inst_id;

	regtexset_t registered_textures;

	struct qfv_tex_s *default_skysheet;
	struct qfv_tex_s *skysheet_tex;

	struct qfv_tex_s *default_skybox;
	struct qfv_tex_s *skybox_tex;
	VkDescriptorSet skybox_descriptor;

	quat_t       default_color;
	quat_t       last_color;

	struct scrap_s *light_scrap;
	struct qfv_stagebuf_s *light_stage;

	bsp_face_t *faces;
	uint32_t   *poly_indices;

	bsp_pass_t  main_pass;	// camera view depth, gbuffer, etc

	VkSampler    sampler;
	VkPipelineLayout layout;

	VkDeviceMemory texture_memory;
	VkPipeline   depth;
	VkPipeline   gbuf;
	VkPipeline   skysheet;
	VkPipeline   skybox;
	VkPipeline   turb;
	size_t       vertex_buffer_size;
	size_t       index_buffer_size;
	VkBuffer     vertex_buffer;
	VkDeviceMemory vertex_memory;
	VkBuffer     index_buffer;
	VkDeviceMemory index_memory;
	VkBuffer     entid_buffer;
	VkDeviceMemory entid_memory;
	bspframeset_t frames;
} bspctx_t;

struct vulkan_ctx_s;
struct qfv_renderframe_s;
void Vulkan_ClearElements (struct vulkan_ctx_s *ctx);
void Vulkan_DrawWorld (struct qfv_renderframe_s *rFrame);
void Vulkan_DrawSky (struct qfv_renderframe_s *rFrame);
void Vulkan_DrawWaterSurfaces (struct qfv_renderframe_s *rFrame);
void Vulkan_Bsp_Flush (struct vulkan_ctx_s *ctx);
void Vulkan_LoadSkys (const char *sky, struct vulkan_ctx_s *ctx);
void Vulkan_RegisterTextures (model_t **models, int num_models,
							  struct vulkan_ctx_s *ctx);
void Vulkan_BuildDisplayLists (model_t **models, int num_models,
							   struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Shutdown (struct vulkan_ctx_s *ctx);

#endif//__QF_Vulkan_qf_bsp_h
