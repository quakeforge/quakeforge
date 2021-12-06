#version 450

layout (constant_id = 0) const int MaxTextures = 256;

layout (set = 0, binding = 1) uniform texture2DArray skins[MaxTextures];
layout (set = 0, binding = 2) uniform sampler samp;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        base_color;
	uint        colorA;
	uint        colorB;
	vec4        fog;
	vec4        color;
	int         texind;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_emission;
layout (location = 2) out vec4 frag_normal;
layout (location = 3) out vec4 frag_position;

void
main (void)
{
	vec4        c;
	vec4        e;
	int         i;
	vec3        light = vec3 (0);
	c = texture (sampler2DArray(skins[texind], samp), vec3 (st, 0)) * unpackUnorm4x8(base_color);
	c += texture (sampler2DArray(skins[texind], samp), vec3 (st, 1)) * unpackUnorm4x8(colorA);
	c += texture (sampler2DArray(skins[texind], samp), vec3 (st, 2)) * unpackUnorm4x8(colorB);
	e = texture (sampler2DArray(skins[texind], samp), vec3 (st, 3));

	frag_color = c;
	frag_emission = e;
	frag_normal = vec4(normal, 1);
	frag_position = position;
}
