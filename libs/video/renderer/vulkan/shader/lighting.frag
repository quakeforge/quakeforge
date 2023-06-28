#version 450
#extension GL_GOOGLE_include_directive : enable

#include "lighting.h"

layout (input_attachment_index = 0, set = 2, binding = 0) uniform subpassInput depth;
layout (input_attachment_index = 1, set = 2, binding = 1) uniform subpassInput color;
layout (input_attachment_index = 2, set = 2, binding = 2) uniform subpassInput emission;
layout (input_attachment_index = 3, set = 2, binding = 3) uniform subpassInput normal;
layout (input_attachment_index = 4, set = 2, binding = 4) uniform subpassInput position;

layout (set = 3, binding = 0) uniform sampler2DArrayShadow shadowCascade[MaxLights];
layout (set = 3, binding = 0) uniform sampler2DShadow shadowPlane[MaxLights];
layout (set = 3, binding = 0) uniform samplerCubeShadow shadowCube[MaxLights];

layout (location = 0) out vec4 frag_color;

float
spot_cone (LightData light, vec3 incoming)
{
	vec3        dir = light.direction.xyz;
	float       cone = light.direction.w;
	float       spotdot = dot (incoming, dir);
	return 1 - smoothstep (cone, .995 * cone + 0.005, spotdot);
}

float
diffuse (vec3 incoming, vec3 normal)
{
	float       lightdot = dot (incoming, normal);
	return clamp (lightdot, 0, 1);
}

float
shadow_cascade (sampler2DArrayShadow map)
{
	return 1;
}

float
shadow_plane (sampler2DShadow map)
{
	return 1;
}

float
shadow_cube (samplerCubeShadow map)
{
	return 1;
}

void
main (void)
{
	//float       d = subpassLoad (depth).r;
	vec3        c = subpassLoad (color).rgb;
	vec3        e = subpassLoad (emission).rgb;
	vec3        n = subpassLoad (normal).rgb;
	vec3        p = subpassLoad (position).rgb;
	vec3        light = vec3 (0);

	//vec3        minLight = vec3 (0);
	for (int i = 0; i < lightCount; i++) {
		LightData   l = lights[i];
		vec3        dir = l.position.xyz - l.position.w * p;
		float       r2 = dot (dir, dir);
		vec4        a = l.attenuation;

		if (l.position.w * a.w * a.w * r2 >= 1) {
			continue;
		}
		vec4        r = vec4 (r2, sqrt(r2), 1, 0);
		vec3        incoming = dir / r.y;
		float       I = (1 - a.w * r.y) / dot (a, r);

		/*int         shadow = lights[i].data & ShadowMask;
		if (shadow == ST_CASCADE) {
			I *= shadow_cascade (shadowCascade[i]);
		} else if (shadow == ST_PLANE) {
			I *= shadow_plane (shadowPlane[i]);
		} else if (shadow == ST_CUBE) {
			I *= shadow_cube (shadowCube[i]);
		}*/

		float       namb = dot(l.direction.xyz, l.direction.xyz);
		I *= spot_cone (l, incoming) * diffuse (incoming, n);
		I = mix (1, I, namb);
		light += I * l.color.w * l.color.xyz;
	}
	//light = max (light, minLight);

	frag_color = vec4 (light, 1);
}
