#version 450
layout (set = 0, binding = 2) uniform sampler2D Texture;
layout (set = 0, binding = 3) uniform sampler2D GlowMap;
layout (set = 0, binding = 4) uniform sampler2D ColorA;
layout (set = 0, binding = 5) uniform sampler2D ColorB;
/*
layout (set = 2, binding = 0) uniform sampler2D Texture;
layout (set = 2, binding = 1) uniform sampler2D GlowMap;
layout (set = 2, binding = 2) uniform sampler2D ColorA;
layout (set = 2, binding = 3) uniform sampler2D ColorB;
*/
struct LightData {
	vec3        color;
	float       dist;
	vec3        position;
	int         type;
	vec3        direction;
	float       cone;
};

layout (constant_id = 0) const int MaxLights = 8;
//layout (set = 1, binding = 0) uniform Lights {
layout (set = 0, binding = 1) uniform Lights {
	int         light_count;
	LightData   lights[MaxLights];
};

layout (push_constant) uniform PushConstants {
	layout (offset = 80)
	vec4        fog;
	vec4        color;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec3 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 frag_color;

vec3
calc_light (LightData light)
{
	if (light.type == 0) {
		vec3 dist = light.position - position;
		float dd = dot (dist, dist);
		float mag = max (0.0, dot (dist, normal));
		return light.color * mag * light.dist / dd;
	} else if (light.type == 1) {
	} else if (light.type == 2) {
		float mag = max (0.0, -dot (light.direction, normal));
		// position is ambient light
		return light.color * dot (light.direction, normal) + light.position;
	}
}

void
main (void)
{
	vec4        c;
	int         i;
	vec3        light = vec3 (0);
	c = texture (Texture, st);
	c += texture (ColorA, st);
	c += texture (ColorB, st);

	if (MaxLights > 0) {
		for (i = 0; i < light_count; i++) {
			light += calc_light (lights[i]);
		}
	}
	c *= vec4 (light, 1);

	c += texture (GlowMap, st);
	//frag_color = vec4((normal + 1)/2, 1);
	frag_color = c;//fogBlend (c);
}
