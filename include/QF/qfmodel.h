#ifndef __QF_qfmodel_h
#define __QF_qfmodel_h

#include "QF/math/vector.h"
#include "QF/simd/types.h"

typedef struct clipdesc_s {
	uint32_t    firstframe;
	uint32_t    numframes;		// 1 for single frames
	uint32_t    flags;
	uint32_t    name;
} clipdesc_t;

typedef struct keyframe_s {
	float       endtime;		// 0 for single frames
	uint32_t    data;
} keyframe_t;

typedef struct anim_s {
	uint32_t    numclips;
	uint32_t    clips;
	uint32_t    keyframes;
	uint32_t    data;
} anim_t;

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
#define qfm_attr_count (qfm_weights + 1)

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

	qfm_special,
} qfm_type_t;
#define qfm_type_count (qfm_special + 1)

typedef struct qfm_attrdesc_s {
	uint32_t    offset;
	unsigned    stride:16;
	qfm_attr_t  attr:3;
	unsigned    abs:1;		// for morph: attrib is absolute instead of delta
	unsigned    set:3;
	unsigned    morph:1;
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

typedef enum : uint32_t {
	qfm_nonuniform = 1,
	qfm_nonleaf = 2,
} qfm_mflag_t;

typedef struct qfm_motor_s {
	vec4f_t     q;		// e23 e31 e12 1
	vec4f_t     t;		// e01 e02 e03 e0123
	vec3_t      s;
	qfm_mflag_t flags;
} qfm_motor_t;

typedef struct qfm_blend_s {
	uint8_t     indices[4];
	uint8_t		weights[4];
} qfm_blend_t;

typedef struct qfm_channel_s {
	uint32_t    data;
	float       base;
	float       scale;
} qfm_channel_t;

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
	uint32_t    vertex_stride;
	uint32_t    morph_stride;
	uint32_t    material;
	anim_t      morph;
	anim_t      skin;
	vec3_t      scale;
	vec3_t      scale_origin;
	vec3_t      bounds_min;
	vec3_t      bounds_max;
} __attribute__((aligned (16))) qf_mesh_t;

typedef struct qf_model_s {
	qfm_loc_t   meshes;
	qfm_loc_t   joints;		// joint definitions (qfm_joint_t)
	qfm_loc_t   inverse;	// inverse joint motors (qfm_motor_t)
	qfm_loc_t   pose;		// (qfm_joint_t) base pose for animation
	qfm_loc_t   channels;
	qfm_loc_t   text;
	anim_t      anim;
	uint16_t    crc;
	uint32_t    render_data;
} __attribute__((aligned (16))) qf_model_t;

#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

GNU89INLINE inline uint32_t qfm_uniform_scale (const vec3_t s)
	__attribute__((const));
GNU89INLINE inline qfm_motor_t qfm_make_motor (qfm_joint_t joint)
	__attribute__((const));
GNU89INLINE inline qfm_motor_t qfm_motor_invert (qfm_motor_t m)
	__attribute__((const));
GNU89INLINE inline qfm_motor_t qfm_motor_mul (qfm_motor_t m1, qfm_motor_t m2)
	__attribute__((const));
GNU89INLINE inline void qfm_motor_to_mat (mat4f_t mat, qfm_motor_t m);

#ifndef IMPLEMENT_QFMODEL_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
uint32_t
qfm_uniform_scale (const vec3_t s)
{
	return s[0] == s[1] && s[0] == s[2] ? 0 : qfm_nonuniform;
}

#ifndef IMPLEMENT_QFMODEL_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
qfm_motor_t
qfm_make_motor (qfm_joint_t joint)
{
	auto q = qconjf (joint.rotate);
	auto t = loadvec3f (joint.translate) * 0.5;
	auto sa = loadxyzf (q);
	float c = q[3];
	auto p = crossf (t, sa) - c * t;
	p[3] = -dotf(t, sa)[3];
	return (qfm_motor_t) {
		.q = q,
		.t = p,
		.s = { VectorExpand (joint.scale) },
		.flags = qfm_uniform_scale (joint.scale),
	};
}

#ifndef IMPLEMENT_QFMODEL_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
qfm_motor_t
qfm_motor_invert (qfm_motor_t m)
{
	auto is = 1 / loadvec3f (m.s);
	return (qfm_motor_t) {
		.q = qconjf (m.q),
		.t = qconjf (m.t),
		.s = { VectorExpand (is) },
		.flags = m.flags,
	};
}

#ifndef IMPLEMENT_QFMODEL_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
qfm_motor_t
qfm_motor_mul (qfm_motor_t m1, qfm_motor_t m2)
{
	float a1 = m1.q[3];
	auto  b1 = loadxyzf (m1.q);
	auto  c1 = loadxyzf (m1.t);
	float d1 = m1.t[3];
	float a2 = m2.q[3];
	auto  b2 = loadxyzf (m2.q);
	auto  c2 = loadxyzf (m2.t);
	float d2 = m2.t[3];

	float a = a1*a2 - dotf (b1,b2)[3];
	auto  b = a1*b2 + a2*b1 - crossf (b1,b2);
	auto  c = a1*c2 + a2*c1 - crossf (b1,c2) - crossf (c1,b2) - d1*b2 - d2*b1;
	float d = a1*d2 + a2*d1 + dotf(b1,c2)[3] + dotf(c1,b2)[3];
	auto s = loadvec3f (m1.s) * loadvec3f (m2.s);
	return (qfm_motor_t) {
		.q = { VectorExpand (b), a },
		.t = { VectorExpand (c), d },
		.s = { VectorExpand (s) },
		.flags = m1.flags | m2.flags,
	};
}

#ifndef IMPLEMENT_QFMODEL_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
void
qfm_motor_to_mat (mat4f_t mat, qfm_motor_t m)
{
	auto q = qconjf (m.q);
	auto t = -2 * qmulf (m.t, m.q);
	t[3] = 1;
	mat4fquat (mat, q);
	mat[0] *= m.s[0];
	mat[1] *= m.s[1];
	mat[2] *= m.s[2];
	mat[3] = t;
}

#endif//__QF_qfmodel_h
