#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput depth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput color;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput normal;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput position;

struct LightData {
	vec3        color;
	float       intensity;
	vec3        position;
	int         radius;
	vec3        direction;
	float       cone;
};

layout (constant_id = 0) const int MaxLights = 128;
layout (set = 1, binding = 0) uniform Lights {
	int         lightCount;
	LightData   lights[MaxLights];
};

layout (location = 0) out vec4 frag_color;

vec3
calc_light (LightData light, vec3 position, vec3 normal)
{
	vec3        dist = light.position - position;
	vec3        incoming = normalize (dist);
	float       spotdot = -dot (incoming, light.direction);
	float       lightdot = dot (incoming, normal);
	float       d = dot (dist, dist);
	float       r = light.radius;

	float       intensity = light.intensity * step (d, r * r);
	intensity *= step (spotdot, light.cone) * clamp (lightdot, 0, 1);
	return light.color * intensity;
}

void
main (void)
{
	//float       d = subpassLoad (depth).r;
	vec3        c = subpassLoad (color).rgb;
	vec3        n = subpassLoad (normal).rgb;
	vec3        p = subpassLoad (position).rgb;
	vec3        light = vec3 (0);

	if (MaxLights > 0) {
		for (int i = 0; i < lightCount; i++) {
			light += calc_light (lights[i], p, n);
		}
	}
	frag_color = vec4 (c * light, 1);
}
