#version 450

layout (set = 1, binding = 0) uniform sampler2D Palette;
layout (set = 2, binding = 0) uniform sampler2DArray Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        colors;
	float       ambient;
	float       shadelight;
	vec4        lightvec;
	vec4        base_color;
	vec4        fog;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        c = texture (Skin, vec3 (st, 0)) * base_color;
	vec4        e = texture (Skin, vec3 (st, 1));
	vec4        rows = unpackUnorm4x8(colors);
	vec4        cmap = texture (Skin, vec3 (st, 2));
	c += texture (Palette, vec2 (cmap.x, rows.x)) * cmap.y;
	c += texture (Palette, vec2 (cmap.z, rows.y)) * cmap.w;

	float       light = ambient;
	float       d = dot (normal, lightvec.xyz);
	d = min (d, 0.0);
	light -= d * shadelight;
	light = max (light, 0.0) / 255;

	frag_color = light * c + e;//fogBlend (c);
}
