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

layout (push_constant) uniform PushConstants {
	vec4        orm;
	vec4        fog;
	float       time;
	float       alpha;
	float       turb_scale;
	uint        control;
};

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 vnormal;
layout (location = 2) in vec2 t_uv;
layout (location = 3) in vec3 l_uvw;
layout (location = 4) in uint entid;

layout (location = 0) out vec2 t_st;
layout (location = 1) out vec3 l_stp;
layout (location = 2) out vec3 direction;
layout (location = 3) out vec4 color;

void
main (void)
{
	if (control & 8) {
		vec4        v = vec4 (gl_VertexIndex & 1, gl_VertexIndex & 2, 0, 1);
		v = vec4 (4, 2, 0, 1) * v - vec4 (1, 1, 0, 0);
		gl_Position = v;
		v.z = 1;
		vec3        d = vec3 (Projection3d[0][0], Projection3d[1][1], 1);
		vec3        vertex = v.xyz / d;
		vec3        dir = vec3 (dot(vertex, View[gl_ViewIndex][0].xyz),
								dot(vertex, View[gl_ViewIndex][1].xyz),
								dot(vertex, View[gl_ViewIndex][2].xyz));
		direction = (Sky * vec4(dir,0)).xyz;
	} else {
		vec3        vert = vec4(vertex,1) * entities[entid].transform;
		gl_Position = Projection3d * (View[gl_ViewIndex] * vec4 (vert, 1));
		direction = (Sky * vec4(vertex,1)).xyz;
	}
	t_st = t_uv;
	l_stp = l_uvw;
	color = entities[entid].color;
}
