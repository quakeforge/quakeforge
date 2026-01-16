#include "GLSL/general.h"

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

[buffer, readonly, set(0), binding(0)] @block ShadowView {
	mat4x4      shadowView[];
};

[buffer, readonly, set(0), binding(1)] @block ShadowId {
	uint        shadowId[];
};

[buffer, readonly, set(1), binding(0)] @block Bones {
	// NOTE these are transposed, so v * m
	mat3x4      bones[];
};

[push_constant] @block PushConstants {
	mat4 Model;
	uint  enabled_mask;
	float blend;
	uint MatrixBase;
};

[in(qfm_position)] vec3 vert_position;
[in(qfm_joints)]   uvec4 joints;
[in(qfm_weights)]  vec4 weights;

[in(qfm_attr_count + qfm_position)] vec3 morph_position;

[shader("Vertex")]
[capability("MultiView")]
void
main (void)
{
	vec3        position;

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

	if ((enabled_mask & qfm_bones) == qfm_bones) {
		mat3x4      m = bones[joints.x] * weights.x;
		m += bones[joints.y] * weights.y;
		m += bones[joints.z] * weights.z;
		m += bones[joints.w] * weights.w;
#if 1
		m += mat3x4(1,0,0,0, 0,1,0,0, 0,0,1,0) * (1 - weights • vec4(1,1,1,1));
		position = vec4(position, 1) * m;
#else
		m += mat3x4(0,0,0,0, 0,0,0,1, 1,1,1,0) * (1 - weights • vec4(1,1,1,1));
		m[1] /= sqrt(m[1] • m[1]);
		auto v = vector (position * m[2].xyz);
		auto q = quaternion (m[1]);
		auto t = vector (m[0].xyz);
		position = t + q * v;
#endif
	}

	//DebugPrintf ("MB:%u VI:%u\n", MatrixBase, gl_ViewIndex);
	uint matid = shadowId[MatrixBase + gl_ViewIndex];
	gl_Position = shadowView[matid] * (Model * vec4(position, 1));
}
