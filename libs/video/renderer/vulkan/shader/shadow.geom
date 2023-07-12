#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (constant_id = 0) const int MaxLights = 128;

layout (set = 0, binding = 0) uniform UBO {
	mat4 vp[MaxLights];
} ubo;

layout (location = 0) in int InstanceIndex[];

void
main (void)
{
	int index = InstanceIndex[0];

	for (int i = 0; i < gl_in.length(); i++) {
		gl_Layer = index;
		gl_Position = ubo.vp[index] * gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
}
