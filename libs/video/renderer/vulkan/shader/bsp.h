#ifndef __shader_bsp_h
#define __shader_bsp_h

#ifndef __QFCC__
QF_VEC_TYPE (ushort, usvec2, 2);
QF_VEC_TYPE (float, vec2, 2);
#endif

#define workgroup_size 512
// frame 0, frame 1 solid
// frame 0, fraem 1 trans
#define mod_queues 4

typedef struct bsp_command_s {// matches VkDrawIndexedIndirectCommand
	uint        indexCount;
	uint        instanceCount;
	uint        firstIndex;
	uint        vertexOffset;
	uint        firstInstace;
} bsp_command_t;

typedef struct cluster_s {//FIXME dup in model.h
	uint        first;
	uint        count;
} cluster_t;

/** Represent a brush model, both main and sub-model.
 *
 * Used for rendering non-world models.
 */
typedef struct bsp_model_s {
	uint        first_cluster;
	uint        cluster_count;
	uint        first_texture;
	uint        texture_count;
} bsp_model_t;

typedef struct bsp_cluster_s {
	uint        first_index;
	uint        index_count;
	uint        tex_id;
} bsp_cluster_t;

typedef struct bsp_queue_s {
	uint        cluster;
	uint        first_instance;
	uint        instance_count;
	ushort      frame;
	ushort      trans;
} bsp_queue_t;

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
 * Each texture in a particular group shares the same `base` frame, with
 * `offset` giving the texture's relative frame number within the group.
 * The current frame is given by `base + (anim_index + offset) % count` where
 * `anim_index` is the global time-based texture animation frame.
 */
typedef struct bsp_texanim_s {
	ushort      base;		///< first frame in group
	byte        offset;		///< relative frame in group
	byte        count;		///< number of frames in group
} bsp_texanim_t;

typedef struct bsp_lightinfo_s {
	usvec2      pos;		///< position of atlas subimage
	usvec2      size;		///< size of atlas subimage
	byte        styles[4];
	uint        data;
} bsp_lightinfo_t;

typedef struct bsp_surfinfo_s {
	uint        plane;
	uint        tex;
	vec2        mins;
} bsp_surfinfo_t;

#endif//__shader_bsp_h
