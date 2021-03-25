#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection;
	mat4 View;
	mat4 Sky;
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
};

layout (location = 0) in vec4 vertex;

void
main (void)
{
	gl_Position = Projection * (View * (Model * vertex));
}
