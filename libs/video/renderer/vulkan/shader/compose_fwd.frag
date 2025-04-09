#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#define OIT_SET 1
#include "oit_blend.finc"

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec3        c;
	vec3        l;
	vec3        e;
	vec3        o;

	c = subpassLoad (color).rgb;
	o = max(BlendFrags (vec4 (c, 1)).xyz, vec3(0));
	o = pow (o, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (o, 1);
}
