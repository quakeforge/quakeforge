#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

layout (set = 0, binding = 0) buffer ShadowView {
	mat4x4      shadowView[];
};

layout (set = 0, binding = 1) buffer ShadowId {
	uint        shadowId[];
};

layout (push_constant) uniform PushConstants {
	uint MatrixBase;
};

#include "entity.h"

layout (set = 1, binding = 0) buffer Entities {
	Entity      entities[];
};

layout (location = 0) in vec4 vertex;
layout (location = 2) in uint entid;

layout (location = 0) out int InstanceIndex;

void
main (void)
{
	vec4        pos = vec4 (vertex * entities[entid].transform, 1);
	uint matid = shadowId[MatrixBase + gl_ViewIndex];
	gl_Position = shadowView[matid] * pos;
}
