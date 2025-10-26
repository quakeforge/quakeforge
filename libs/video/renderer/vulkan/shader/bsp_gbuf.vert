#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "entity.h"

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) readonly buffer Entities {
	Entity      entities[];
};

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec4 tl_uv;
layout (location = 3) in uint entind;

layout (location = 0) out vec4 tl_st;
layout (location = 1) out vec3 direction;
layout (location = 2) out vec3 normal;
layout (location = 3) out vec4 position;
layout (location = 4) out vec4 color;

void
main (void)
{
	vec4        vert = vec4 (vec4(vertex, 1) * entities[entind].transform, 1);

	position = vert;
	gl_Position = Projection3d * (View[gl_ViewIndex] * vert);
	direction = (Sky * vec4 (vertex, 0)).xyz;
	normal = (vec4(vnormal, 0) * entities[entind].transform).xyz;
	tl_st = tl_uv;
	color = entities[entind].color;
}
