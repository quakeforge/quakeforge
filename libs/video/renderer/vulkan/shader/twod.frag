#version 450

layout (set = 1, binding = 0) uniform sampler2D Texture;

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 color;
layout (location = 2) in flat uint id;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out uint entid;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, st);
	frag_color = pix * color;
	entid = id;
}
