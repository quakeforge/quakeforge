#version 450

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
};

layout (location = 0) in vec4 vertexa;
layout (location = 1) in vec3 normala;
layout (location = 2) in vec4 vertexb;
layout (location = 3) in vec3 normalb;

layout (location = 0) out int InstanceIndex;

void
main (void)
{
	vec4        vertex;

	vertex = mix (vertexa, vertexb, blend);
	gl_Position = Model * vertex;
	InstanceIndex = gl_InstanceIndex;
}
