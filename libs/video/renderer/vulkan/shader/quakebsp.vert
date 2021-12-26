#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
};

layout (location = 0) in vec4 vertex;
layout (location = 1) in vec4 tl_uv;

layout (location = 0) out vec4 tl_st;
layout (location = 1) out vec3 direction;

void
main (void)
{
	gl_Position = Projection3d * (View * (Model * vertex));
	direction = (Sky * vertex).xyz;
	tl_st = tl_uv;
}
