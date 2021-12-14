#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (set = 1, binding = 0) buffer Vertices {
	vec4 xyuv[];
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	int  frame;
};

layout (location = 0) out vec2 st;

void
main (void)
{
	vec4        v = xyuv[frame * 4 + gl_VertexIndex];

	vec4        pos = vec4 (0, 0, 0, 1);
	pos += v.x * Model[1] + v.y * Model[2];

	gl_Position = Projection3d * (View * pos);
	st = v.zw;
}
