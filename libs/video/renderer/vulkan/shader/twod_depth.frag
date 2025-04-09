#version 450

layout (set = 1, binding = 0) uniform sampler2D Texture;

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 color;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, st);
	if (pix.a * color.a < 1) {
		discard;
	}
}
