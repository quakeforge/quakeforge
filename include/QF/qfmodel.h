#ifndef __QF_qfmodel_h
#define __QF_qfmodel_h

#include "QF/qtypes.h"
#include "QF/simd/types.h"

typedef struct qfm_loc_s {
	uint32_t    offset;
	uint32_t    count;
} qfm_loc_t;

typedef enum : uint32_t {
	qfm_position,
	qfm_normal,
	qfm_tangent,
	qfm_texcoord,
	qfm_color,
	qfm_joints,
	qfm_weights,
} qfm_attr_t;

typedef enum : uint32_t {
	qfm_s8,
	qfm_s16,
	qfm_s32,
	qfm_s64,

	qfm_u8,
	qfm_u16,
	qfm_u32,
	qfm_u64,

	qfm_s8n,
	qfm_s16n,
	qfm_u8n,
	qfm_u16n,

	qfm_f16,
	qfm_f32,
	qfm_f64,

	qfm_special
} qfm_type_t;

typedef struct qfm_attrdesc_s {
	uint32_t    offset;
	unsigned    stride:16;
	qfm_attr_t  attr:3;
	unsigned    abs:1;		// for morph: attrib is absolute instead of delta
	unsigned    set:4;
	qfm_type_t  type:4;
	unsigned    components:4;
} qfm_attrdesc_t;
static_assert (sizeof (qfm_attrdesc_t) == 2 * sizeof (uint32_t),
			   "qfm_attrdesc_t wrong size");

typedef struct qfm_joint_s {
	float       translate[3];
	uint32_t    name;
	vec4f_t     rotate;
	float       scale[3];
	int32_t     parent;
} qfm_joint_t;

typedef struct qfm_pose_s {
	int32_t     parent;
	uint32_t    mask;
	float       channeloffset[10];
	uint32_t    channelbase[10];
	float       channelscale[10];
} qfm_pose_t;

typedef struct qfm_frame_s {
	uint32_t    name;
	uint32_t    data;
	vec3_t      bounds_min;
	vec3_t      bounds_max;
} qfm_frame_t;

typedef struct qf_mesh_s {
	uint32_t    name;
	uint32_t    triangle_count;
	qfm_type_t  index_type;
	uint32_t    indices;
	qfm_type_t  adjacency_type;
	uint32_t    adjacency;
	qfm_loc_t   attributes;
	qfm_loc_t   vertices;			// count is per morph
	qfm_loc_t   morph_attributes;
	anim_t      morph;
	anim_t      skin;
	vec3_t      scale;
	vec3_t      scale_origin;
	vec3_t      bounds_min;
	vec3_t      bounds_max;
} qf_mesh_t;

typedef struct qf_model_s {
	qfm_loc_t   meshes;
	qfm_loc_t   joints;
	qfm_loc_t   poses;
	qfm_loc_t   text;
	anim_t      anim;
	uint16_t    crc;
	uint32_t    render_data;
} qf_model_t;

#endif//__QF_qfmodel_h
