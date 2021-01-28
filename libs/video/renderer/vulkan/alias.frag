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

struct LightData {
	vec3        color;
	float       dist;
	vec3        position;
	int         type;
	vec3        direction;
	float       cone;
};

layout (constant_id = 0) const int MaxLights = 8;
layout (set = 1, binding = 0) uniform Lights {
	int         light_count;
	LightData   lights[MaxLights];
};
*/
layout (push_constant) uniform PushConstants {
	layout (offset = 80)
	vec4        fog;
	vec4        color;
};

layout (location = 0) in vec2 st;
layout (location = 1) in vec3 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        c;
	int         i;
	vec3        light = vec3 (0);
	c = texture (Texture, st);
	c += texture (ColorA, st);
	c += texture (ColorB, st);
/*
	if (MaxLights > 0) {
		for (i = 0; i < light_count; i++) {
			vec3 dist = lights[i].position - position;
			float dd = dot (dist, dist);
			float mag = max (0.0, dot (dist, normal));
			light += lights[i].color * mag * lights[i].dist / dd;
		}
	}
	c *= vec4 (light, 1);
*/
	c += texture (GlowMap, st);
	//frag_color = vec4((normal + 1)/2, 1);
	frag_color = c;//fogBlend (c);
}
