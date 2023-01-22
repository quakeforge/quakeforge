#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
layout (location = 0) in vec4 v_tl_st[];
layout (location = 1) in vec3 v_direction[];
layout (location = 2) in vec4 v_color[];

layout (location = 0) out vec4 tl_st;
layout (location = 1) out vec3 direction;
layout (location = 2) out vec3 normal;
layout (location = 3) out vec4 position;
layout (location = 4) out vec4 color;

void
main()
{
	vec3        a = gl_in[0].gl_Position.xyz;
	vec3        b = gl_in[1].gl_Position.xyz;
	vec3        c = gl_in[2].gl_Position.xyz;

	vec3        n = normalize (cross (c - a, b - a));

	for (int vert = 0; vert < 3; vert++) {
		vec4        p = gl_in[vert].gl_Position;
		gl_Position = Projection3d * (View * (p));
		tl_st = v_tl_st[vert];
		direction = v_direction[vert];
		color = v_color[vert];
		normal = n;
		position = p;
		EmitVertex ();
	}
	EndPrimitive ();
}
