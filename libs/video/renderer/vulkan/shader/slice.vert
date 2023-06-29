#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

// rg -> offset x,y
// ba -> texture u,v
layout (set = 1, binding = 1) uniform textureBuffer glyph_data;

// per instance data
layout (location = 0) in uint glyph_index;
layout (location = 1) in vec4 glyph_color;
layout (location = 2) in vec2 glyph_position;
layout (location = 3) in vec2 glyph_offset;	// for 9-slice

layout (location = 0) out vec2 uv;
layout (location = 1) out vec4 color;
layout (location = 2) out uint id;

void
main (void)
{
	vec2 offset = vec2 ((gl_VertexIndex & 4) >> 2, (gl_VertexIndex & 8) >> 3);
	vec4 glyph = texelFetch (glyph_data, int(glyph_index + gl_VertexIndex));
	// offset stored in glyph components 0 and 1
	vec2 position = glyph_position + glyph.xy + offset * glyph_offset;
	gl_Position = Projection2d * vec4 (position.xy, 0.0, 1.0);
	// texture uv stored in glyph components 2 and 3
	uv = glyph.pq;
	color = glyph_color;
	id = gl_InstanceIndex;
}
