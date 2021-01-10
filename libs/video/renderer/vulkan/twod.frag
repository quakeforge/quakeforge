#version 450

layout (set = 0, binding = 1) uniform sampler2D Texture;

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, st);
	if (pix.a < 0.5)
		discard;
	frag_color = pix * color;
}
