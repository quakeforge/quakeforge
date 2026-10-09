#version 450

layout (set = 1, binding = 0) uniform sampler2DArray Texture;

layout (location = 0) in vec3 st;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, st);
	frag_color = pix * color;
}
