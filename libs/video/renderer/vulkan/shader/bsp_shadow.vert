#version 450

layout (push_constant) uniform PushConstants {
	mat4 Model;
};

layout (location = 0) in vec4 vertex;

layout (location = 0) out int InstanceIndex;

void
main (void)
{
	gl_Position = Model * vertex;
	InstanceIndex = gl_InstanceIndex;
}
