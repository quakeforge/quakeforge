#version 450
layout (set = 0, binding = 1) uniform sampler2DArray Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        base_color;
	uint        colorA;
	uint        colorB;
	vec4        fog;
	vec4        color;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec3 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_normal;

void
main (void)
{
	vec4        c;
	int         i;
	vec3        light = vec3 (0);
	c = texture (Skin, vec3 (st, 0)) * unpackUnorm4x8(base_color);
	c += texture (Skin, vec3 (st, 1)) * unpackUnorm4x8(colorA);
	c += texture (Skin, vec3 (st, 2)) * unpackUnorm4x8(colorB);

	frag_color = c;
	frag_normal = vec4(normal, 1);
}
