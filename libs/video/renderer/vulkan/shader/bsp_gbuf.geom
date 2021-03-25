#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection;
	mat4 View;
	mat4 Sky;
};

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
layout (location = 0) in vec4 v_tl_st[];
layout (location = 1) in vec3 v_direction[];

layout (location = 0) out vec4 tl_st;
layout (location = 1) out vec3 direction;
layout (location = 2) out vec3 normal;
layout (location = 3) out vec4 position;

void
main()
{
	vec3        a = gl_in[0].gl_Position.xyz;
	vec3        b = gl_in[1].gl_Position.xyz;
	vec3        c = gl_in[2].gl_Position.xyz;

	vec3        n = normalize (cross (c - a, b - a));

	for (int vert = 0; vert < 3; vert++) {
		vec4        p = gl_in[vert].gl_Position;
		gl_Position = Projection * (View * (p));
		tl_st = v_tl_st[vert];
		direction = v_direction[vert];
		normal = n;
		position = p;
		EmitVertex ();
	}
	EndPrimitive ();
}
