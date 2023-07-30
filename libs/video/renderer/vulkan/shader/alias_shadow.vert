#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

layout (set = 0, binding = 0) buffer ShadowView {
	mat4x4      shadowView[];
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
	uint MatrixBase;
};

layout (location = 0) in vec4 vertexa;
layout (location = 1) in vec3 normala;
layout (location = 2) in vec4 vertexb;
layout (location = 3) in vec3 normalb;

void
main (void)
{
	vec4 pos = mix (vertexa, vertexb, blend);
	gl_Position = shadowView[MatrixBase + gl_ViewIndex] * (Model * pos);
}
