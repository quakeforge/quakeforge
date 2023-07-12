#version 450
#extension GL_GOOGLE_include_directive : enable

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
	vec3        vert = vertex * entities[entid].transform;
	gl_Position = vec4 (vert, 1);;
	InstanceIndex = gl_InstanceIndex;
}
