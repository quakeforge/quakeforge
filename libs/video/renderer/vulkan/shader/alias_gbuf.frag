#version 450

layout (set = 1, binding = 0) uniform sampler2D Palette;
layout (set = 2, binding = 0) uniform sampler2DArray Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        colors;
	vec4        base_color;
	vec4        fog;
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
	c = texture (Skin, vec3 (st, 0)) * base_color;
	e = texture (Skin, vec3 (st, 1));
	vec4        rows = unpackUnorm4x8(colors);
	vec4        cmap = texture (Skin, vec3 (st, 2));
	c += texture (Palette, vec2 (cmap.x, rows.x)) * cmap.y;
	c += texture (Palette, vec2 (cmap.z, rows.y)) * cmap.w;

	frag_color = c;
	frag_emission = e;
	frag_normal = vec4(normalize(normal), 1);
	frag_position = position;
}
