#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput depth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput color;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput emission;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput normal;
layout (input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput position;

struct LightData {
	vec3        color;
	int         data;// bits 0-6: intensity key (however, values 0-66)
	vec3        position;
	float       radius;
	vec3        direction;
	float       cone;
};

#define MaxLights 256

layout (set = 2, binding = 0) uniform sampler2DArrayShadow shadowCascade[MaxLights];
layout (set = 2, binding = 0) uniform sampler2DShadow shadowPlane[MaxLights];
layout (set = 2, binding = 0) uniform samplerCubeShadow shadowCube[MaxLights];

layout (set = 1, binding = 0) uniform Lights {
	vec4        intensity[17]; // 68 floats
	ivec4       lightCounts;
	LightData   lights[MaxLights];
	mat4        shadowMat[MaxLights];
	vec4        shadowCascale[MaxLights];
};

layout (location = 0) out vec4 frag_color;

vec3
calc_light (LightData light, vec3 position, vec3 normal)
{
	float       r = light.radius;
	vec3        dist = light.position - position;
	float       d = sqrt (dot (dist, dist));
	if (d > r) {
		// the light is too far away
		return vec3 (0);
	}

	vec3        incoming = dist / d;
	float       spotdot = dot (incoming, light.direction);
	float       lightdot = dot (incoming, normal);

	int         style = light.data & 0x7f;
	// deliberate array index error: access intensity2 as well
	float       i = intensity[style / 4][style % 4];
	i *= step (d, r) * clamp (lightdot, 0, 1);
	i *= smoothstep (spotdot, 1 - (1 - spotdot) * 0.995, light.cone);
	return light.color * i * (r - d);
}

vec3
shadow_cascade (sampler2DArrayShadow map)
{
	return vec3(1);
}

vec3
shadow_plane (sampler2DShadow map)
{
	return vec3(1);
}

vec3
shadow_cube (samplerCubeShadow map)
{
	return vec3(1);
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

	if (MaxLights > 0) {
		int         i = 0;
		while (i < lightCounts.x) {
			shadow_cascade (shadowCascade[i]);
			i++;
		}
		while (i < lightCounts.y) {
			shadow_plane (shadowPlane[i]);
			i++;
		}
		while (i < lightCounts.z) {
			shadow_cube (shadowCube[i]);
			i++;
		}
		while (i < lightCounts.w) {
			light += calc_light (lights[i], p, n);
			i++;
		}
	}
	frag_color = vec4 (c * light + e, 1);
}
