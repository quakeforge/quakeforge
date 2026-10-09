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

typedef struct set_s set_t;
typedef struct qfv_resource_s qfv_resource_t;

/** \defgroup vulkan_bsp Brush model rendering
	\ingroup vulkan
*/

typedef struct bsp_model_s bsp_model_t;
typedef struct bsp_texanim_s bsp_texanim_t;

/** Holds texture animation data for brush models.
 *
 * Brush models support one or two texture animation groups, based on the
 * entity's frame (0 or non-0). When the entity's frame is 0, group 0 is used,
 * otherwise group 1 is used. If there is no alternate (group 1) animation
 * data for the texture, then the texture's group 0 data is copied to group 1
 * in order to avoid complications in selecting which texture a face is to use.
 *
 * As all of a group's frames are together, `frame_map` is used to get the
 * actual texture id for the frame.
 */
typedef struct texdata_s {
	bsp_texanim_t *anim_main;	///< group 0 animations
	bsp_texanim_t *anim_alt;	///< group 1 animations
	uint16_t   *frame_map;	///< map from texture frame to texture id
} texdata_t;
///@}

/** \defgroup vulkan_bsp_draw Brush model drawing
 * \ingroup vulkan_bsp
 */
///@{
typedef struct vulktex_s {
	VkImageView view;
	VkDescriptorSet descriptor;
	int         tex_id;
	const char *name;
} vulktex_t;

typedef struct regtexset_s
    DARRAY_TYPE (vulktex_t *) regtexset_t;

typedef struct bsp_pass_s {
	vec4f_t     position;			///< view position
	const vec4f_t *transform;		///< transform for current model
	const struct mod_brush_s *brush;///< data for current model
	struct bspctx_s *bsp_context;	///< owning bsp context
	struct entqueue_s *entqueue;	///< entities to render this pass
	/** \name GPU data
	 *
	 * The entity ids associated with each draw
	 * instance are updated each frame. The pointers are to the per-frame
	 * mapped buffers for the respective data.
	 */
	///@{
	uint32_t   *entid_data;			///< instance id to entity id map
	uint32_t    entid_count;		///< number of entids written to buffer
	///@}
	/** \name Potentially Visible Sets
	 *
	 * For an object to be in the PVS, its frame id must match the current
	 * visibility frame id, thus clearing all sets is done by incrementing
	 * `vis_frame`, and adding an object to the PVS is done by setting its
	 * current frame id to the current visibility frame id.
	 */
	visstate_t  visstate;
	regtexset_t *textures;			///< textures to bind when emitting calls
	set_t      *tex_set;			///< per-pipeline set of textures
} bsp_pass_t;
///@}

/// \ingroup vulkan_bsp
///@{
typedef enum {
	QFV_bspSolid,
	QFV_bspBackground,
	QFV_bspSky,
	QFV_bspTrans,	// texture translucency
	QFV_bspTurb,	// also translucent via r_wateralpha
	QFV_bspTransEnt,// translucent entities

	QFV_bspNumPasses
} QFV_BspQueue;

typedef enum {
	QFV_bspMain,
	QFV_bspLightmap,	// same as main, but using lightmaps
	QFV_bspShadow,
	QFV_bspDebug,

	QFV_bspNumStages
} QFV_BspPass;

typedef struct bspframe_s {
	uint32_t   *entid_data;
	uint32_t    entid_offset;
	uint32_t    entid_count;
	uint32_t    queue;
	uint32_t    style_offset;
} bspframe_t;

typedef struct bspframeset_s
    DARRAY_TYPE (bspframe_t) bspframeset_t;

typedef struct bsp_buffer_s {
	VkBuffer    buffer;
	size_t      size;
	VkDeviceAddress addr;
	VkDeviceAddress *bb;
} bsp_buffer_t;

typedef struct bsp_prefixsum_s {
	VkDeviceAddress in;
	VkDeviceAddress out;
	VkDeviceAddress tmp;
	VkDeviceAddress total;
	VkBuffer    invoke;		// bsp_invoke_t [2]
} bsp_prefixsum_t;

/** Main BSP context structure
 *
 * This holds all the state and resources needed for rendering brush models.
 */
typedef struct bspctx_s {
	struct vulkan_ctx_s *vulkan_ctx;

	VkImageView    notexture;			///< replacement for invalid textures
	VkDescriptorSet notexture_descriptor;

	struct qfv_dsmanager_s *dsmanager;
	qfv_resource_t *lightmap_resource;
	struct scrap_s *light_scrap;
	VkDescriptorSet lightmap_descriptor;
	VkDescriptorSet lightmap_image;

	unsigned    max_edges;

	regtexset_t registered_textures;///< textures for all loaded brush models
	VkImageView default_skysheet;
	VkImageView skysheet_tex;	///< scrolling sky texture for current map

	texdata_t   texdata;

	VkImageView default_skybox;
	struct qfv_tex_s *skybox_tex;		///< sky box texture for current map
	VkDescriptorSet skybox_descriptor;
	uint32_t    skybox_id;

	vulktex_t   notexture_render;
	vulktex_t   background_render;
	texture_t   notexture_tex;
	texture_t   background_tex;

	VkImageView default_skymap;
	struct qfv_tex_s *skymap_tex;		///< sky eqrec map for current map
	VkDescriptorSet skymap_descriptor;
	uint32_t    skymap_id;

	bsp_pass_t  main_pass;			///< camera view depth, gbuffer, etc
	bsp_pass_t  shadow_pass;
	bsp_pass_t  debug_pass;

	VkSampler    sampler;
	VkSampler    equrect;

	// for vkCmdDrawIndexedIndirectCount
	uint32_t    *command_offsets;
	uint32_t    *command_counts;

	qfv_resource_t *base_resource;
	qfv_resource_t *tex_resource;
	uint32_t     num_tex_anim;
	VkBuffer     default_verts;
	qfv_resource_t *bsp_resource;

	bspframeset_t frames;

	bsp_prefixsum_t mod_offsets;
	bsp_prefixsum_t mod_clusters;
	bsp_prefixsum_t light_surfs;
	bsp_prefixsum_t light_luxels;
	bsp_prefixsum_t subclusters;

	bsp_buffer_t mod_tmp_buffer;
#define BB_buffer(name) bsp_buffer_t name##_buffer
	BB_buffer (index);
	BB_buffer (vertex);

	//cluster (both realtime lights and lightmap)
	BB_buffer (command_counts);
	BB_buffer (command_offsets);
	BB_buffer (commands);
	BB_buffer (subclusters);
	BB_buffer (clusters);
	BB_buffer (cluster_map);
	BB_buffer (anim_main);
	BB_buffer (anim_alt);
	BB_buffer (frame_map);

	uint32_t   *texture_count;
	uint32_t   *anim_index;

	//ent (both realtime lights and lightmap)
	BB_buffer (ent_ids);
	BB_buffer (ent_rel);
	BB_buffer (inst_ids);
	VkDeviceAddress *entities;
	BB_buffer (subcluster_queue);
	BB_buffer (subcluster_tmp);
	BB_buffer (instance_queue);
	BB_buffer (cluster_queue);
	BB_buffer (models);
	BB_buffer (tex_ids);//XXX not used
	BB_buffer (mod_counts);
	BB_buffer (mod_offsets);
	BB_buffer (mod_clusters);
	BB_buffer (mod_invoke);
	uint32_t   *ent_count;
	uint32_t   *num_models;

	//lightmap
	BB_buffer (lightinfo);
	BB_buffer (lightsize);
	BB_buffer (surfinfo);
	BB_buffer (light_style_values);
	BB_buffer (lightmap_data);
	BB_buffer (light_cache);
	BB_buffer (light_queue_offs);
	BB_buffer (light_queue_inds);
	BB_buffer (light_queue_tmp);
	BB_buffer (light_clusters);
	BB_buffer (light_cluster_surfs);
	BB_buffer (light_cluster_tmp);
	BB_buffer (light_queue);
	uint32_t   *num_lightmaps;

	//prefixsum
	struct {
		VkDeviceAddress *in_data;
		VkDeviceAddress *out_data;
		VkDeviceAddress *sum_data;
		VkDeviceAddress *count;
		uint32_t   *mode;
	}           psum;
	BB_buffer (prefixsum_counts);
	BB_buffer (block_sums);

	//render
	uint32_t   *MatrixBase;
	vec4f_t    *fog;
	float      *time;
	float      *alpha;
	float      *turb_scale;
	uint32_t   *control;

#undef BB_buffer
} bspctx_t;

struct vulkan_ctx_s;
void Vulkan_LoadSkys (const char *sky, struct vulkan_ctx_s *ctx);
void Vulkan_SetSkyId (uint32_t id, struct vulkan_ctx_s *ctx);
void Vulkan_RegisterTextures (model_t **models, int num_models,
							  struct vulkan_ctx_s *ctx);
void Vulkan_BuildDisplayLists (model_t **models, int num_models,
							   struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Init (struct vulkan_ctx_s *ctx);
bsp_pass_t *Vulkan_Bsp_GetPass (struct vulkan_ctx_s *ctx, QFV_BspPass pass_ind);
///@}

#endif//__QF_Vulkan_qf_bsp_h
