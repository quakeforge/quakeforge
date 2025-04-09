#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "infplane.finc"

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

struct PlaneData {
	mat3x4      p;
	vec4        scolor;
	vec4        tcolor;
};

layout (set = 1, binding = 0) readonly buffer Planes {
	int         numPlanes;
	PlaneData   planes[];
};

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 frag_color;

const float N = 128.0; // grid ratio
vec3 gridTextureGradBox( in vec2 p, in vec2 ddx, in vec2 ddy )
{
	// filter kernel
	vec2 w = max(abs(ddx), abs(ddy)) + 0.01;

	// analytic (box) filtering
	vec2 a = p + 0.5*w + vec2(0.5/N);
	vec2 b = p - 0.5*w + vec2(0.5/N);
	vec2 i = (floor(a)+min(fract(a)*N,1.0)-
			  floor(b)-min(fract(b)*N,1.0))/(N*w);
	//pattern
	return vec3(1 - (1.0-i.x)*(1.0-i.y), i.x, i.y);
}


void
main (void)
{
	mat4 p = inverse (View[gl_ViewIndex]);
	mat3 cam = mat3(vec3(p[0]) / Projection3d[0][0],
					vec3(p[1]) / Projection3d[1][1],
					vec3(p[2]));
	vec4 c = vec4(0);
	vec2 UV = 2 * uv - vec2(1,1);
	for (int i = 0; i < numPlanes; i++) {
		vec4    pst = plane_st (mat3(planes[i].p), cam, vec3(UV, 1));
		vec2    st = pst.xy;
		vec2 ddx_st = dFdx (st);
		vec2 ddy_st = dFdy (st);
		bool d = pst.w * planes[i].p[0][3] > 0;
		if (!d) continue;
		vec3 g = gridTextureGradBox(st, ddx_st, ddy_st);
		float c1 = abs(st.y) < 0.5 ? g[2] : 0;
		float c2 = abs(st.x) < 0.5 ? g[1] : 0;
		float c0 = g[0] * (1 - c1) * (1 - c2);
		c += vec4(1) * c0 + planes[i].scolor * c1 + planes[i].tcolor * c2;
		//st = fract(st);
		//st = st - st * st;
		//float s = st.x * st.y;
		//s = 20 * (0.05 - s);
		//s = max (s, 0);
		//c += pst.w * planes[i][0][3] > 0 ? s * vec4(1,1,1,0.4): vec4(0);
	}
	frag_color = c;
}
