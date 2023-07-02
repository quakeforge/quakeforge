#version 450

layout (set = 1, binding = 0) uniform sampler2D Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        colorA;
	uint        colorB;
	vec4        base_color;
	vec4        fog;
};

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 fnormal;
layout (location = 3) in vec3 ftangent;
layout (location = 4) in vec3 fbitangent;
layout (location = 5) in vec4 color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        c;

	c = texture (Skin, texcoord);// * color;

	frag_color = c;
}
