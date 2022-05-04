#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (set = 3, binding = 0) buffer Bones {
	// NOTE these are transposed, so v * m
	mat3x4      bones[];
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
};

layout (location = 0) in vec3 vposition;
layout (location = 1) in ivec4 vbones;
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

void
main (void)
{
	mat3x4      m = bones[vbones.x] * vweights.x;
	m += bones[vbones.y] * vweights.y;
	m += bones[vbones.z] * vweights.z;
	m += bones[vbones.w] * vweights.w;
	vec4        pos = vec4 (Model * vec4(vposition, 1) * m, 1);
	gl_Position = Projection3d * (View * pos);
	position = pos;
	mat3 adjTrans = mat3 (cross(m[1].xyz, m[2].xyz), cross(m[2].xyz, m[0].xyz),
						  cross(m[0].xyz, m[1].xyz));
	normal = mat3 (Model) * vnormal * adjTrans;
	tangent = mat3 (Model) * vtangent.xyz * adjTrans;
	bitangent = cross (normal, tangent) * vtangent.w;
	texcoord = vtexcoord;
	color = vcolor;
}
