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

layout (constant_id = 0) const int MaxLights = 128;
layout (set = 1, binding = 0) uniform Lights {
	vec4        intensity[16]; // 64 floats
	vec3        intensity2;
	int         lightCount;
	LightData   lights[MaxLights];
};

layout (location = 0) out vec4 frag_color;

vec3
calc_light (LightData light, vec3 position, vec3 normal)
{
	vec3        dist = light.position - position;
	float       d = sqrt (dot (dist, dist));
	vec3        incoming = dist / d;
	float       spotdot = dot (incoming, light.direction);
	float       lightdot = dot (incoming, normal);
	float       r = light.radius;

	int         style = light.data & 0x7f;
	// deliberate array index error: access intensity2 as well
	float       i = intensity[style / 4][style % 4];
	i *= step (d, r) * clamp (lightdot, 0, 1);
	i *= smoothstep (spotdot, 1 - (1 - spotdot) * 0.995, light.cone);
	return light.color * i * (r - d);
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
		for (int i = 0; i < lightCount; i++) {
			light += calc_light (lights[i], p, n);
		}
	}
	frag_color = vec4 (c * light + e, 1);
}
