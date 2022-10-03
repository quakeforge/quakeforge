#version 450

layout (set = 1, binding = 1) uniform sampler2D Texture;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, uv).rrrr;
	frag_color = pix * color;
}
