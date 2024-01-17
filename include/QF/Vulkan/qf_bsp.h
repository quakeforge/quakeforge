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

/** \defgroup vulkan_bsp Brush model rendering
	\ingroup vulkan
*/

/** Represent a single face (polygon) of a brush model.
 *
 * There is one of these for each face in the bsp (brush) model, built at run
 * time when the model is loaded (actually, after all models are loaded but
 * before rendering begins).
 */
typedef struct bsp_face_s {
	uint32_t    first_index;	///< index of first index in poly_indices
	uint32_t    index_count;	///< includes primitive restart
	uint32_t    tex_id;			///< texture bound to this face (maybe animated)
	uint32_t    flags;			///< face drawing (alpha, side, sky, turb)
} bsp_face_t;

/** Represent a brush model, both main and sub-model.
 *
 * Used for rendering non-world models.
 */
typedef struct bsp_model_s {
	uint32_t    first_face;
	uint32_t    face_count;
} bsp_model_t;

#if 0
typedef struct texname_s {
	char        name[MIPTEXNAME];
} texname_t;

typedef struct texmip_s {
	uint32_t    width;
	uint32_t    height;
	uint32_t    offsets[MIPLEVELS];
} texmip_t;
#endif
/** \defgroup vulkan_bsp_texanim Animated Textures
 * \ingroup vulkan_bsp
 *
 * Brush models support texture animations. For general details, see
 * \ref bsp_texture_animation. These structures allow for quick lookup
 * of the correct texture to use in an animation cycle, or even whether there
 * is an animation cycle.
 */
///@{
/** Represent a texture's animation group.
 *
 * Every texture is in an animation group, even when not animated. When the
 * texture is not animated, `count` is 1, otherwise `count` is the number of
 * frames in the group, thus every texture has at least one frame.
 *
 * Each texture in a particular groupp shares the same `base` frame, with
 * `offset` giving the texture's relative frame number within the group.
 * The current frame is given by `base + (anim_index + offset) % count` where
 * `anim_index` is the global time-based texture animation frame.
 */
typedef struct texanim_s {
	uint16_t    base;		///< first frame in group
	byte        offset;		///< relative frame in group
	byte        count;		///< number of frames in group
} texanim_t;

/** Holds texture animation data for brush models.
 *
 * Brush models support one or two texture animation groups, based on the
 * entity's frame (0 or non-0). When the entity's frame is 0, group 0 is used,
 * otherwise group 1 is used. If there is no alternate (group 1) animation
 * data for the texture, then the texture's group 0 data is copied to group 1
 * in order to avoid coplications in selecting which texture a face is to use.
 *
 * As all of a group's frames are together, `frame_map` is used to get the
 * actual texture id for the frame.
 */
typedef struct texdata_s {
//	texname_t  *names;
//	texmip_t  **mips;
	texanim_t  *anim_main;	///< group 0 animations
	texanim_t  *anim_alt;	///< group 1 animations
	uint16_t   *frame_map;	///< map from texture frame to texture id
//	int         num_tex;
} texdata_t;
///@}

/** \defgroup vulkan_bsp_draw Brush model drawing
 * \ingroup vulkan_bsp
 */
///@{
typedef struct vulktex_s {
	struct qfv_tex_s *tex;
	VkDescriptorSet descriptor;
	int         tex_id;
} vulktex_t;

typedef struct regtexset_s
    DARRAY_TYPE (vulktex_t *) regtexset_t;

/** Represent a single draw call.
 *
 * For each texture that has faces to be rendered, one or more draw calls is
 * made. Normally, only one call per texture is made, but if different models
 * use the same texture, then a separate draw call is made for each model.
 * When multiple entities use the same model, instanced rendering is used to
 * draw all the faces sharing a texture for all the entities using that model.
 * Thus when there are multiple draw calls for a single texture, they are
 * grouped together so there is only one bind per texture.
 *
 * The index buffer is populated every frame with the vertex indices of the
 * faces to be rendered for the current frame, grouped by texture and instance
 * id (model render id).
 *
 * The model render id is assigned after models are loaded but before rendering
 * begins and remains constant until the next time models are loaded (level
 * change).
 *
 * The entid buffer is also populated every frame with the render id of the
 * entities to be drawn that frame, It is used to map gl_InstanceIndex to
 * entity id so as to look up the entity's transform and color (and any other
 * data in the future).
 *
 * \dot
 * digraph vulkan_bsp_draw_call {
 *     layout=dot; rankdir=LR; compound=true; nodesep=1.0;
 *     vertices [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td>vertex</td></tr>
 *                  <tr><td>vertex</td></tr>
 *                  <tr><td>...</td></tr>
 *                  <tr><td port="p">vertex</td></tr>
 *                  <tr><td>vertex</td></tr>
 *              </table> >];
 *     indices  [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td>index</td></tr>
 *                  <tr><td>index</td></tr>
 *                  <tr><td>...</td></tr>
 *                  <tr><td port="p">index</td></tr>
 *                  <tr><td>index</td></tr>
 *              </table> >];
 *     entids   [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td>entid</td></tr>
 *                  <tr><td>...</td></tr>
 *                  <tr><td port="p">entid</td></tr>
 *                  <tr><td>entid</td></tr>
 *                  <tr><td>...</td></tr>
 *                  <tr><td>entid</td></tr>
 *              </table> >];
 *     entdata  [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td>transform</td><td>color</td></tr>
 *                  <tr><td>transform</td><td>color</td></tr>
 *                  <tr><td colspan="2">...</td></tr>
 *                  <tr><td port="p">transform</td><td>color</td></tr>
 *                  <tr><td colspan="2">...</td></tr>
 *                  <tr><td>transform</td><td>color</td></tr>
 *              </table> >];
 *     drawcall [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td port="tex" >tex_id</td></tr>
 *                  <tr><td            >inst_id</td></tr>
 *                  <tr><td port="ind" >first_index</td></tr>
 *                  <tr><td            >index_count</td></tr>
 *                  <tr><td port="inst">first_instance</td></tr>
 *                  <tr><td            >instance_count</td></tr>
 *              </table> >];
 *     textures [shape=none,label=< <table border="1" cellborder="1">
 *                  <tr><td>texture</td></tr>
 *                  <tr><td>texture</td></tr>
 *                  <tr><td port="p">texture</td></tr>
 *                  <tr><td>...</td></tr>
 *                  <tr><td>texture</td></tr>
 *              </table> >];
 *     vertex   [label="vertex shader"];
 *     fragment [label="fragment shader"];
 *     drawcall:tex -> textures:p;
 *     drawcall:ind -> indices:p;
 *     drawcall:inst -> entids:p;
 *     entids:p -> entdata:p;
 *     indices:p -> vertices:p;
 *     vertex -> entdata [label="storage buffer"];
 *     vertex -> entids [label="per instance"];
 *     vertex -> indices [label="index buffer"];
 *     vertex -> vertices [label="per vertex"];
 *     fragment -> textures [label="per call"];
 * }
 * \enddot
 */
///@{
typedef struct bsp_draw_s {
	uint32_t    tex_id;			///< texture to bind for this draw call
	uint32_t    inst_id;		///< model render id owning this draw call
	uint32_t    index_count;	///< number of indices for this draw call
	uint32_t    instance_count;	///< number of instances to draw
	uint32_t    first_index;	///< index into index buffer
	uint32_t    first_instance;	///< index into entid buffer
} bsp_draw_t;

typedef struct bsp_drawset_s
    DARRAY_TYPE (bsp_draw_t) bsp_drawset_t;
///@}

/** Tag models that are to be queued for translucent drawing.
 */
#define INST_ALPHA (1u<<31)

/** Representation of a single face queued for drawing.
 */
///@{
typedef struct instface_s {
	uint32_t    inst_id;		///< model render id owning this face
	uint32_t    face;			///< index of face in context array
} instface_t;

typedef struct bsp_instfaceset_s
    DARRAY_TYPE (instface_t) bsp_instfaceset_t;
///@}

/** Track entities using a model.
 */
///@{
typedef struct bsp_modelentset_s
	DARRAY_TYPE (uint32_t) bsp_modelentset_t;

/** Represent a single model and the entities using it.
 */
typedef struct bsp_instance_s {
	int         first_instance;	///< index into entid buffer
	bsp_modelentset_t entities;	///< list of entity render ids using this model
} bsp_instance_t;
///@}

typedef struct bsp_pass_s {
	vec4f_t     position;			///< view position
	const vec4f_t *transform;		///< transform for current model
	const struct mod_brush_s *brush;///< data for current model
	struct bspctx_s *bsp_context;	///< owning bsp context
	struct entqueue_s *entqueue;	///< entities to render this pass
	/** \name GPU data
	 *
	 * The indices to be drawn and the entity ids associated with each draw
	 * instance are updated each frame. The pointers are to the per-frame
	 * mapped buffers for the respective data.
	 */
	///@{
	uint32_t   *indices;			///< polygon vertex indices
	uint32_t    index_count;		///< number of indices written to buffer
	uint32_t   *entid_data;			///< instance id to entity id map
	uint32_t    entid_count;		///< numer of entids written to buffer
	///@}
	/** \name Potentially Visible Sets
	 *
	 * For an object to be in the PVS, its frame id must match the current
	 * visibility frame id, thus clearing all sets is done by incrementing
	 * `vis_frame`, and adding an object to the PVS is done by setting its
	 * current frame id to the current visibility frame id.
	 */
	///@{
	int         vis_frame;			///< current visibility frame id
	int        *face_frames;		///< per-face visibility frame ids
	int        *leaf_frames;		///< per-leaf visibility frame ids
	int        *node_frames;		///< per-node visibility frame ids
	///@}
	bsp_instfaceset_t *face_queue;	///< per-texture face queues
	regtexset_t *textures;			///< textures to bind when emitting calls
	int         num_queues;			///< number of pipeline queues
	bsp_drawset_t *draw_queues;		///< per-pipeline draw queues
	uint32_t    inst_id;			///< render id of current model
	bsp_instance_t *instances;		///< per-model entid lists
	// FIXME There are several potential optimizations here:
	// 1) ent_frame could be forced to be 0 or 1 and then used to index a
	// two-element array of texanim pointers
	// 2) ent_frame could be a pointer to the correct texanim array
	// 3) could update a tex_id map each frame and unconditionally index that
	//
	// As the texture id is used for selecting the face queue, 3 could be used
	// for mapping all textures to 1 or two queues for shadow rendering
	int         ent_frame;			///< animation frame of current entity
} bsp_pass_t;
///@}

/// \ingroup vulkan_bsp
///@{
typedef enum {
	QFV_bspSolid,
	QFV_bspSky,
	QFV_bspTrans,	// texture translucency
	QFV_bspTurb,	// also translucent via r_wateralpha

	QFV_bspNumPasses
} QFV_BspQueue;

typedef enum {
	QFV_bspMain,
	QFV_bspShadow,
	QFV_bspDebug,

	QFV_bspNumStages
} QFV_BspPass;

typedef struct bspframe_s {
	uint32_t   *index_data;		// pointer into mega-buffer for this frame (c)
	uint32_t    index_offset;	// offset of index_data within mega-buffer (c)
	uint32_t    index_count;	// number if indices queued (d)
	uint32_t   *entid_data;
	uint32_t    entid_offset;
	uint32_t    entid_count;
} bspframe_t;

typedef struct bspframeset_s
    DARRAY_TYPE (bspframe_t) bspframeset_t;

/** Main BSP context structure
 *
 * This holds all the state and resources needed for rendering brush models.
 */
typedef struct bspctx_s {
	struct vulkan_ctx_s *vulkan_ctx;

	vulktex_t    notexture;			///< replacement for invalid textures

	struct scrap_s *light_scrap;
	struct qfv_stagebuf_s *light_stage;
	VkDescriptorSet lightmap_descriptor;

	int         num_models;			///< number of loaded brush models
	uint32_t    num_faces;
	bsp_model_t *models;			///< all loaded brush models
	bsp_face_t *faces;				///< all faces from all loaded brush models
	msurface_t **surfaces;			///< all faces from all loaded brush models
	uint32_t   *poly_indices;	///< face indices from all loaded brush models

	regtexset_t registered_textures;///< textures for all loaded brush models
	texdata_t   texdata;			///< texture animation data
	int         anim_index;			///< texture animation frame (5fps)
	struct qfv_tex_s *default_skysheet;
	struct qfv_tex_s *skysheet_tex;	///< scrolling sky texture for current map

	struct qfv_tex_s *default_skybox;
	struct qfv_tex_s *skybox_tex;	///< sky box texture for current map
	VkDescriptorSet skybox_descriptor;

	bsp_pass_t  main_pass;			///< camera view depth, gbuffer, etc
	bsp_pass_t  shadow_pass;
	bsp_pass_t  debug_pass;

	VkSampler    sampler;

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
void Vulkan_LoadSkys (const char *sky, struct vulkan_ctx_s *ctx);
void Vulkan_RegisterTextures (model_t **models, int num_models,
							  struct vulkan_ctx_s *ctx);
void Vulkan_BuildDisplayLists (model_t **models, int num_models,
							   struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Init (struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Setup (struct vulkan_ctx_s *ctx);
void Vulkan_Bsp_Shutdown (struct vulkan_ctx_s *ctx);
bsp_pass_t *Vulkan_Bsp_GetPass (struct vulkan_ctx_s *ctx, QFV_BspPass pass_ind);
///@}

#endif//__QF_Vulkan_qf_bsp_h
