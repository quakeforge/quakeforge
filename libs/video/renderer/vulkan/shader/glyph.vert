#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;
layout (set = 1, binding = 1) uniform textureBuffer glyph_data;

// per instance data
layout (location = 0) in uint glyph_index;
layout (location = 1) in vec4 glyph_color;
layout (location = 2) in vec2 glyph_position;

// rg -> offset x,y
// ba -> texture u,v

layout (location = 0) out vec2 uv;
layout (location = 1) out vec4 color;

void
main (void)
{
	vec4 glyph = texelFetch (glyph_data, int(glyph_index) * 4 + gl_VertexIndex);
	// offset stored in glyph components 0 and 1
	vec2 position = glyph_position + glyph.xy;
	gl_Position = Projection2d * vec4 (position.xy, 0.0, 1.0);
	// texture uv stored in glyph components 2 and 3
	uv = glyph.pq;
	color = glyph_color;
}
