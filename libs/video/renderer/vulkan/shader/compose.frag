#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#define OIT_SET 1
#include "oit_blend.finc"

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput opaque;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec3        o;
	vec3        c;

	o = subpassLoad (opaque).rgb;
	c = BlendFrags (vec4 (o, 1)).xyz;
	c = pow (c, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (c, 1);
}
