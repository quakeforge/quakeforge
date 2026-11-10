#ifndef __QF_qfmodel_h
#define __QF_qfmodel_h

#ifdef __QFCC__
typedef uint uint32_t;
typedef ushort uint16_t;
typedef byte uint8_t;
typedef int  int32_t;
typedef vec4 vec4f_t;
typedef vec3 vec3_t;
#define static_assert(...)
#define __attribute__(...) @attribute __VA_ARGS__
#else
# include "QF/math/vector.h"
# include "QF/simd/types.h"
#endif

typedef struct clipdesc_s {
	uint32_t    firstframe;		// index into keyframe array
	uint32_t    numframes;		// 1 for single frames
	uint32_t    flags;
	uint32_t    name;			// offset into model's text region
} clipdesc_t;

// for skin channels, data points to the skin data (texture, etc)
// for morph channels, data points to the vertex data for the frame
// for joint channels, data points the joint channel data for the frame
// for event channels, data is either 0 (null event) or points to a single
//                     event's data.
typedef struct keyframe_s {
	float       endtime;		// immediate event if same as previous frame
	uint32_t    data;			// pointer to channel-specific data
} keyframe_t;

// for skin and morph animations, the offsets are from the owning mesh
// for joint animations, the offsets are from the model
typedef struct anim_s {
	uint32_t    numclips;
	uint32_t    clips;		// offset to clipdesc_t array
	uint32_t    keyframes;	// offset to keyframe_t array
	uint32_t    data;		// offset
} anim_t;

typedef struct qfm_loc_s {
	uint32_t    offset;		// offset relative to owning object (mesh/model)
	uint32_t    count;		// number of array elements
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
	unsigned    set:3;
	unsigned    morph:2;		// up to 4 sets of morph attributes
	qfm_type_t  type:4;
	unsigned    components:4;
} qfm_attrdesc_t;
static_assert (sizeof (qfm_attrdesc_t) == 2 * sizeof (uint32_t),
			   "qfm_attrdesc_t wrong size");

typedef struct qfm_joint_s {
	vec3_t      translate;
	uint32_t    name;
	vec4f_t     rotate;
	vec3_t      scale;
	int32_t     parent;
} qfm_joint_t;
static_assert (sizeof (qfm_joint_t) == 12 * sizeof (uint32_t),
			   "qfm_joint_t wrong size");

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
static_assert (sizeof (qfm_motor_t) == 12 * sizeof (uint32_t),
			   "qfm_motor_t wrong size");

typedef struct qfm_blend_s {
	uint8_t     indices[4];
	uint8_t		weights[4];
} qfm_blend_t;

// value comes from keyframe_t.data
// values are layout out per channel in a single frame's block of data
// currently, values are always uint16_t (as per IQM format)
// *(base + data) = channel.base + channel.scale * value[channel]
typedef struct qfm_channel_s {
	uint32_t    data;			// relative offset to field being updated
	float       base;			// base value
	float       scale;			// scale
} qfm_channel_t;

typedef struct qfm_frame_s {
	uint32_t    name;			// alias only, not iqm
	uint32_t    data;			// not used?
	vec3_t      bounds_min;		// from loaded model data
	vec3_t      bounds_max;		// from loaded model data
} qfm_frame_t;

/* adjacency information
*/

typedef struct halfedge_s {
	int         twin;
	int         next;
	int         prev;
	int         vert;
	int         edge;
	int         face;
} halfedge_t;

/* Analytic half-edge. Useful when next, prev and face can be calculated from
   the half-edge index (eg, for levels 1+ Catmull-Clark subdivision, face is
   index/4, and next and previous are index+1 and index-1 wrapped at the
   multiple of 4 boundary because each subdivision level always has exactly
   4 times the faces as the previous level, and all faces are quads regardless
   of the level-0 topology).
   https://onrendering.com/data/papers/catmark/HalfedgeCatmullClark.pdf

   quarter-edge because it's half the size of a half-edge.
*/
typedef struct quarteredge_s {
	int         twin;
	int         vert;
	int         edge;
} quarteredge_t;

typedef struct qf_mesh_s {
	uint32_t    name;
	uint32_t    triangle_count;
	qfm_type_t  index_type;
	uint32_t    indices;
	qfm_loc_t   adjacency;			// halfedge_t or quarteredge_t
	qfm_loc_t   attributes;			// qfm_attrdesc_t
	qfm_loc_t   vertices;			// count is per morph
	qfm_loc_t   morph_attributes;	// qfm_attrdesc_t
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
	qfm_loc_t   channels;	// qfm_channel_t
	qfm_loc_t   text;		// all text data for the model
	anim_t      anim;		// joint animations
	uint16_t    crc;
	uint32_t    render_data;
} __attribute__((aligned (16))) qf_model_t;

#ifndef __QFCC__
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

#define QFMINLINE GNU89INLINE inline

QFMINLINE clipdesc_t *qfm_clipdesc (qf_model_t *model, uint32_t clip);
QFMINLINE keyframe_t *qfm_keyframe (qf_model_t *model, uint32_t frame);

QFMINLINE uint32_t qfm_uniform_scale (const vec3_t s)
	__attribute__((const));
QFMINLINE qfm_motor_t qfm_make_motor (qfm_joint_t joint)
	__attribute__((const));
QFMINLINE qfm_motor_t qfm_motor_invert (qfm_motor_t m)
	__attribute__((const));
QFMINLINE qfm_motor_t qfm_motor_mul (qfm_motor_t m1, qfm_motor_t m2)
	__attribute__((const));
QFMINLINE void qfm_motor_to_mat (mat4f_t mat, qfm_motor_t m);

#undef QFMINLINE
#ifndef IMPLEMENT_QFMODEL_Funcs
#define QFMINLINE GNU89INLINE inline
#else
#define QFMINLINE VISIBLE
#endif

QFMINLINE clipdesc_t *
qfm_clipdesc (qf_model_t *model, uint32_t clip)
{
	return &((clipdesc_t *) ((byte *) model + model->anim.clips))[clip];
}

QFMINLINE keyframe_t *
qfm_keyframe (qf_model_t *model, uint32_t frame)
{
	return &((keyframe_t *) ((byte *) model + model->anim.keyframes))[frame];
}

QFMINLINE uint32_t
qfm_uniform_scale (const vec3_t s)
{
	return s[0] == s[1] && s[0] == s[2] ? 0 : qfm_nonuniform;
}

QFMINLINE qfm_motor_t
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

QFMINLINE qfm_motor_t
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

QFMINLINE qfm_motor_t
qfm_motor_mul (qfm_motor_t m1, qfm_motor_t m2)
{
	float a1 = m1.q[3];
	auto  b1 = loadxyzf (m1.q);
	auto  c1 = loadxyzf (m1.t);
	float d1 = m1.t[3];
	auto  s1 = loadvec3f (m1.s);
	float a2 = m2.q[3];
	auto  b2 = loadxyzf (m2.q);
	// scale translation (c2 and d2) (FIXME assumes uniform scale (though
	// non-uniform can never be perfect due to shear))
	auto  c2 = loadxyzf (m2.t) * s1[0];
	float d2 = m2.t[3] * s1[0];
	auto  s2 = loadvec3f (m2.s);

	float a = a1*a2 - dotf (b1,b2)[3];
	auto  b = a1*b2 + a2*b1 - crossf (b1,b2);
	auto  c = a1*c2 + a2*c1 - crossf (b1,c2) - crossf (c1,b2) - d1*b2 - d2*b1;
	float d = a1*d2 + a2*d1 + dotf(b1,c2)[3] + dotf(c1,b2)[3];
	auto  s = s1 * s2;
	return (qfm_motor_t) {
		.q = { VectorExpand (b), a },
		.t = { VectorExpand (c), d },
		.s = { VectorExpand (s) },
		.flags = m1.flags | m2.flags,
	};
}

QFMINLINE void
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
#endif

#endif//__QF_qfmodel_h
