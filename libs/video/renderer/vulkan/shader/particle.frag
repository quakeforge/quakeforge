#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "fog.finc"

#include "oit_store.finc"

layout (push_constant) uniform PushConstants {
	layout (offset = 64)
	vec4        fog;
};

layout (location = 0) in vec4 uv_tr;
layout (location = 1) in vec4 color;

layout(early_fragment_tests) in;

void
main (void)
{
	vec4        c = color;
	vec2        x = uv_tr.xy;

	float       a = 1 - dot (x, x);
	if (a <= 0) {
		discard;
	}
	//c = c * a;
	c = FogBlend (c * a, fog);
	StoreFrag (c, gl_FragCoord.z);
}
