#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;
layout (set = 1, binding = 0) uniform sampler2D Palette;

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
	// geometry shader will take care of Projection
	gl_Position = View * (Model * position);
	o_velocity = View * (Model * velocity);
	uint        c = floatBitsToInt (color.x);
	uint        x = c & 0x0f;
	uint        y = (c >> 4) & 0x0f;
	o_color = texture (Palette, vec2 (x, y) / 15.0);
	o_ramp = ramp;
}
