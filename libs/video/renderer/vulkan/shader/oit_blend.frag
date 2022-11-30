#version 450
#extension GL_GOOGLE_include_directive : enable
#define OIT_SET 0
#include "oit.h"

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	#define MAX_FRAGMENTS 64
	FragData    frags[MAX_FRAGMENTS];
	int         numFrags = 0;
	ivec2       coord = ivec2(gl_FragCoord.xy);
	int         index = imageLoad (heads, coord).r;

	//FIXME use a heap and prioritize closer fragments
	while (index != -1 && numFrags < MAX_FRAGMENTS) {
		frags[numFrags] = fragments[index];
		numFrags++;
		index = fragments[index].next;
	}
	//insertion sort
	for (int i = 1; i < numFrags; i++) {
		FragData    toInsert = frags[i];
		int         j = i;
		while (j > 0 && toInsert.depth > frags[j - 1].depth) {
			frags[j] = frags[j - 1];
			j--;
		}
		frags[j] = toInsert;
	}

	vec4        color = vec4 (0, 0, 0, 0);
	for (int i = 0; i < numFrags; i++) {
		color = mix (color, frags[i].color,
					 clamp (frags[i].color.a, 0.0f, 1.0f));
	}
	frag_color = color;
}
