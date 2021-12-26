#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;
layout (location = 0) in vec4 velocity[];
layout (location = 1) in vec4 color[];
layout (location = 2) in vec4 ramp[];

layout (location = 0) out vec4 uv_tr;
layout (location = 1) out vec4 o_color;

void
main()
{
	vec4        pos = gl_in[0].gl_Position;
	vec4        tr = vec4 (0, 0, ramp[0].xy);
	float       s = ramp[0].z;
	vec4        d, p;
	vec4        c = color[0];

	d = vec4 (-1, 1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (-1, -1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (1, 1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (1, -1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	EndPrimitive ();
}
