#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#define OIT_SET 1
#include "oit_blend.finc"

#include "fog.finc"

layout (push_constant) uniform PushConstants {
	vec4        fog;
	vec4        camera;
};

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput light;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput emission;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput position;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec3        c = subpassLoad (color).rgb;
	vec3        l = subpassLoad (light).rgb;
	vec3        e = subpassLoad (emission).rgb;
	vec3        p = subpassLoad (position).xyz;
	vec3        o = max(BlendFrags (vec4 (c * l + e, 1)).xyz, vec3(0));

	float d = length (p - camera.xyz);
	o = FogBlend (vec4(o,1), fog, d).xyz;
	o = pow (o, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (o, 1);
}
