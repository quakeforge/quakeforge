#version 450

layout (set = 1, binding = 1) uniform sampler2DArray Texture;

layout (push_constant) uniform PushConstants {
	layout (offset = 64)
	int  frame;
	int  spriteind;
	// two slots
	vec4        fog;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 normal;

layout(early_fragment_tests) in;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        pix;

	pix = texture (Texture, vec3 (st, frame));
	if (pix.a < 0.5) {
		discard;
	}
	frag_color = pix;
}
