#version 450

layout (constant_id = 0) const int MaxTextures = 256;

layout (set = 2, binding = 0) uniform sampler samp;
layout (set = 2, binding = 1) uniform texture2DArray sprites[MaxTextures];

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

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_emission;
layout (location = 2) out vec4 frag_normal;
layout (location = 3) out vec4 frag_position;

void
main (void)
{
	vec4        pix;

	pix = texture (sampler2DArray(sprites[spriteind], samp), vec3 (st, frame));
	if (pix.a < 0.5) {
		discard;
	}
	frag_color = vec4(0,0,0,1);
	frag_emission = pix;
	frag_normal = vec4(normal, 1);
	frag_position = position;
}
