#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

layout (constant_id = 0) const bool IQMDepthOnly = false;

layout (set = 0, binding = 0) buffer ShadowView {
	mat4x4      shadowView[];
};

layout (set = 0, binding = 1) buffer ShadowId {
	uint        shadowId[];
};

layout (set = 3, binding = 0) buffer Bones {
	// NOTE these are transposed, so v * m
	mat3x4      bones[];
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
	uint MatrixBase;
};

layout (location = 0) in vec3 vposition;
layout (location = 1) in uvec4 vbones;
layout (location = 2) in vec4 vweights;
layout (location = 3) in vec2 vtexcoord;
layout (location = 4) in vec3 vnormal;
layout (location = 5) in vec4 vtangent;
layout (location = 6) in vec4 vcolor;

layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec4 position;
layout (location = 2) out vec3 normal;
layout (location = 3) out vec3 tangent;
layout (location = 4) out vec3 bitangent;
layout (location = 5) out vec4 color;

vec3
qmul (vec4 q, vec3 v)
{
	vec3 uv = cross (q.xyz, v);
	vec3 uuv = cross (q.xyz, uv);
	return v + ((uv * q.w) + uuv) * 2;
}

void
main (void)
{
	mat3x4      m = bones[vbones.x] * vweights.x;
	m += bones[vbones.y] * vweights.y;
	m += bones[vbones.z] * vweights.z;
	m += bones[vbones.w] * vweights.w;
#if 1
	m += mat3x4(1,0,0,0,0,1,0,0,0,0,1,0) * (1 - dot(vweights, vec4(1,1,1,1)));
	vec4        pos = vec4 (vec4(vposition, 1) * m, 1);
#else
	m += mat3x4(0,0,0,0,0,0,0,1,1,1,1,0) * (1 - dot(vweights, vec4(1,1,1,1)));
	m[1] /= sqrt(dot(m[1], m[1]));
	vec4 pos = m[0] + vec4 (qmul (m[1], m[2].xyz * vposition), 1);
#endif
	pos = Model * pos;
	uint matid = shadowId[MatrixBase + gl_ViewIndex];
	gl_Position = shadowView[matid] * pos;

	if (!IQMDepthOnly) {
		position = pos;
		mat3 adjTrans = mat3 (cross(m[1].xyz, m[2].xyz),
							  cross(m[2].xyz, m[0].xyz),
							  cross(m[0].xyz, m[1].xyz));
		normal = normalize (mat3 (Model) * vnormal * adjTrans);
		tangent = mat3 (Model) * vtangent.xyz * adjTrans;
		tangent = normalize (tangent - dot (tangent, normal) * normal);
		bitangent = cross (normal, tangent) * vtangent.w;
		texcoord = vtexcoord;
		color = vcolor;
	}
}
