#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
};

layout (location = 0) in vec4 vertexa;
layout (location = 1) in vec3 normala;
layout (location = 2) in vec4 vertexb;
layout (location = 3) in vec3 normalb;

void
main (void)
{
	vec4        vertex;
	vec4        pos;

	vertex = mix (vertexa, vertexb, blend);
	pos = (Model * vertex);
	gl_Position = Projection3d * (View * pos);
}
