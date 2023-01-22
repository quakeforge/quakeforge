#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "entity.h"

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) buffer Entities {
	Entity      entities[];
};

layout (location = 0) in vec4 vertex;
layout (location = 1) in vec4 tl_uv;
layout (location = 2) in uint entid;

layout (location = 0) out vec4 tl_st;
layout (location = 1) out vec3 direction;
layout (location = 2) out vec4 color;

void
main (void)
{
	vec3        vert = vertex * entities[entid].transform;
	gl_Position = Projection3d * (View[gl_ViewIndex] * vec4 (vert, 1));
	direction = (Sky * vertex).xyz;
	tl_st = tl_uv;
	color = entities[entid].color;
}
