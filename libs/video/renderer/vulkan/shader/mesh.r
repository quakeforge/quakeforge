//#extension GL_EXT_multiview : enable

#define GLSL(op) @intrinsic(OpExtInst, "GLSL.std.450", op)
#define SPV(op) @intrinsic(op)
@generic(genFType=@vector(float),genBType=@vector(bool)) {
genFType normalize(genFType x) = GLSL(Normalize);
genFType clamp(genFType x, genFType minVal, genFType maxVal) = GLSL(FClamp);
genFType mix(genFType x, genFType y, genFType a) = GLSL(FMix);
genFType mix(genFType x, genFType y, float a) = GLSL(FMix)
	[x, y, @construct (genFType, a)];
genFType mix(genFType x, genFType y, genBType a) = SPV(OpSelect) [a, y, x];
};

[out("Position")] vec4 gl_Position;
[in("ViewIndex")] int gl_ViewIndex;

typedef enum {
	qfm_position,
	qfm_normal,
	qfm_tangent,
	qfm_texcoord,
	qfm_color,
	qfm_joints,
	qfm_weights
} qfm_attr_t;
#define qfm_attr_count (qfm_weights + 1)

#define attrmask(a) (1u << (a))

#define qfm_bones (attrmask(qfm_joints) | attrmask(qfm_weights))

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

[buffer, readonly, set(2), binding(0)] @block Bones {
	// NOTE these are transposed, so v * m
	mat3x4      bones[];
};

[push_constant] @block PushConstants {
	mat4 Model;
	uint  enabled_mask;
	float blend;
	uint  debug_bone;
};

[in(qfm_position)] vec3 vert_position;
[in(qfm_normal)]   vec3 vert_normal;
[in(qfm_tangent)]  vec4 vert_tangent;
[in(qfm_texcoord)] vec2 vert_texcoord;
[in(qfm_color)]    vec4 vert_color;
[in(qfm_joints)]   uvec4 joints;
[in(qfm_weights)]  vec4 weights;

[in(qfm_attr_count + qfm_position)] vec3 morph_position;
[in(qfm_attr_count + qfm_normal)]   vec3 morph_normal;
[in(qfm_attr_count + qfm_tangent)]  vec4 morph_tangent;
[in(qfm_attr_count + qfm_texcoord)] vec2 morph_texcoord;
[in(qfm_attr_count + qfm_color)]    vec4 morph_color;

[out(0)] vec2 out_texcoord;
[out(1)] vec4 out_position;
[out(2)] vec3 out_normal;
[out(3)] vec4 out_color;

[shader("Vertex")]
[capability("MultiView")]
void
main (void)
{
	vec3        position;
	vec3        normal;
	vec4        tangent;
	vec2        texcoord;
	vec4        color = vec4(1,1,1,1);

#define morhed_attr(attr) \
	do { \
		if (enabled_mask & attrmask(qfm_##attr)) { \
			attr = vert_##attr; \
			if (enabled_mask & attrmask(qfm_##attr + qfm_attr_count)) { \
				attr = mix (attr, morph_##attr, blend); \
			} \
		} \
	} while (0)
	morhed_attr (position);
	morhed_attr (normal);
	morhed_attr (tangent);
	morhed_attr (texcoord);
	morhed_attr (color);

	if ((enabled_mask & qfm_bones) == qfm_bones) {
		mat3x4      m = bones[joints.x] * weights.x;
		m += bones[joints.y] * weights.y;
		m += bones[joints.z] * weights.z;
		m += bones[joints.w] * weights.w;
		if (debug_bone != ~0u) {
			bvec4 foo = uvec4(debug_bone) == joints;
			float w = mix(vec4(0), weights, foo) • vec4(1,1,1,1);
			auto c1 = mix(vec4(0,0,1,1), vec4(0,1,0,1), clamp(w, 0, 0.5) * 2);
			color = mix(c1, vec4(1,0,0,1), clamp(w-0.5, 0, 0.5) * 2);
		}
#if 1
		m += mat3x4(1,0,0,0, 0,1,0,0, 0,0,1,0) * (1 - weights • vec4(1,1,1,1));
		position = vec4(position, 1) * m;
		if (enabled_mask & attrmask(qfm_normal)) {
			auto mat = mat3 (m[0][0], m[1][0], m[2][0],
							 m[0][1], m[1][1], m[2][1],
							 m[0][2], m[1][2], m[2][2]);
			mat = mat3 (Model) * mat;
			auto norm_mat = mat3 (mat[1] × mat[2],
								  mat[2] × mat[0],
								  mat[0] × mat[1]);
			float sign = mat[0] • norm_mat[0] >= 0 ? 1 : -1;
			normal = norm_mat * normal * sign;
			if (enabled_mask & attrmask (qfm_tangent)) {
				tangent = vec4 (mat * vec3 (tangent), tangent.w);
			}
		}
#else
		m += mat3x4(0,0,0,0, 0,0,0,1, 1,1,1,0) * (1 - weights • vec4(1,1,1,1));
		m[1] /= sqrt(m[1] • m[1]);
		auto v = vector (position * m[2].xyz);
		auto q = quaternion (m[1]);
		auto t = vector (m[0].xyz);
		position = t + q * v;
		if (enabled_mask & attrmask(qfm_normal)) {
			normal = q * vector(normal);
			if (enabled_mask & attrmask(qfm_tangent)) {
				tangent = vec4(q * vector(tangent), tangent.w);
			}
		}
#endif
	} else {
		// FIXME assumes Model has uniform scale/no shear
		normal = vec3 (Model * vec4 (normal, 0));
	}

	auto pos = Model * vec4(position, 1);
	gl_Position = Projection3d * (View[gl_ViewIndex] * pos);
	out_position = pos;
	out_normal = normalize (normal);
	out_texcoord = texcoord;
	out_color = color;
}
