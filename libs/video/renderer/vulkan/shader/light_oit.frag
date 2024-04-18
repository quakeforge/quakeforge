#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "oit_store.finc"

layout (location = 0) flat in uint light_index;

layout(early_fragment_tests) in;

void
main (void)
{
	vec4 c = !gl_FrontFacing ? vec4 (1, 0, 1, 0.05) : vec4 (0, 1, 1, 0.05);
	StoreFrag (c, gl_FragCoord.z);
}
