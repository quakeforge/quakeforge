#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (push_constant) uniform PushConstants {
	mat4 Model;
};

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 velocity;
layout (location = 2) in vec4 color;
layout (location = 3) in vec4 ramp;

layout (location = 0) out vec4 o_velocity;
layout (location = 1) out vec4 o_color;
layout (location = 2) out vec4 o_ramp;

void
main (void)
{
	// geometry shader will take care of Projection and View
	gl_Position = Model * position;
	o_velocity = Model * velocity;
	o_color = color;
	o_ramp = ramp;
}
