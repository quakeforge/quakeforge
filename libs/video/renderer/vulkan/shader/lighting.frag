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
	float       light;	// doubles as radius for linear
	vec3        direction;
	float       cone;
};

#define StyleMask   0x07f
#define ModelMask   0x380
#define ShadowMask  0xc00

#define LM_LINEAR   (0 << 7)	// light - dist (or radius + dist if -ve)
#define LM_INVERSE  (1 << 7)	// distFactor1 * light / dist
#define LM_INVERSE2 (2 << 7)	// distFactor2 * light / (dist * dist)
#define LM_INFINITE (3 << 7)	// light
#define LM_AMBIENT  (4 << 7)	// light
#define LM_INVERSE3 (5 << 7)	// distFactor2 * light / (dist + distFactor2)**2

#define ST_NONE     (0 << 10)	// no shadows
#define ST_PLANE    (1 << 10)	// single plane shadow map (small spotlight)
#define ST_CASCADE  (2 << 10)	// cascaded shadow maps
#define ST_CUBE     (3 << 10)	// cubemap (omni, large spotlight)

layout (constant_id = 0) const int MaxLights = 256;

layout (set = 2, binding = 0) uniform sampler2DArrayShadow shadowCascade[MaxLights];
layout (set = 2, binding = 0) uniform sampler2DShadow shadowPlane[MaxLights];
layout (set = 2, binding = 0) uniform samplerCubeShadow shadowCube[MaxLights];

layout (set = 1, binding = 0) uniform Lights {
	vec4        intensity[17]; // 68 floats
	float       distFactor1;	// for inverse
	float       distFactor2;	// for inverse2 and inverse3
	int         lightCount;
	LightData   lights[MaxLights];
	mat4        shadowMat[MaxLights];
	vec4        shadowCascale[MaxLights];
};

layout (location = 0) out vec4 frag_color;

float
spot_cone (LightData light, vec3 incoming)
{
	float       spotdot = dot (incoming, light.direction);
	return smoothstep (spotdot, 1 - (1 - spotdot) * 0.995, light.cone);
}

float
diffuse (vec3 incoming, vec3 normal)
{
	float       lightdot = dot (incoming, normal);
	return clamp (lightdot, 0, 1);
}

float
light_linear (LightData light, float d)
{
	float       l = light.light;
	if (l < 0) {
		return min (l + d, 0);
	}  else {
		return max (l - d, 0);
	}
}

float
light_inverse (LightData light, float d)
{
	float       l = light.light;
	return l / (distFactor1 * d);
}

float
light_inverse2 (LightData light, float d)
{
	float       l = light.light;
	return l / (distFactor2 * d);
}

float
light_infinite (LightData light)
{
	return light.light;
}

float
light_ambient (LightData light)
{
	return light.light;
}

float
light_inverse3 (LightData light, float d)
{
	float       l = light.light;
	return l / (distFactor2 * d + 1);
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

	if (MaxLights > 0) {
		vec3        minLight = vec3 (0);
		for (int i = 0; i < lightCount; i++) {
			vec3        dist = lights[i].position - p;
			float       d = dot (dist, dist);
			int         model = lights[i].data & ModelMask;

			if (model != LM_INFINITE
				&& d > lights[i].light * lights[i].light) {
				continue;
			}

			float       l = 0;
			if (model == LM_LINEAR) {
				d = sqrt (d);
				l = light_linear (lights[i], d);
			} else if (model == LM_INVERSE) {
				d = sqrt (d);
				l = light_inverse (lights[i], d);
			} else if (model == LM_INVERSE2) {
				l = light_inverse2 (lights[i], d);
				d = sqrt (d);
			} else if (model == LM_INFINITE) {
				l = light_infinite (lights[i]);
				dist = lights[i].direction;
				d = -1;
			} else if (model == LM_AMBIENT) {
				l = light_ambient (lights[i]);
			} else if (model == LM_INVERSE3) {
				l = light_inverse3 (lights[i], d);
				d = sqrt (d);
			}

			int         style = lights[i].data & StyleMask;
			l *= intensity[style / 4][style % 4];

			int         shadow = lights[i].data & ShadowMask;
			if (shadow == ST_CASCADE) {
				l *= shadow_cascade (shadowCascade[i]);
			} else if (shadow == ST_PLANE) {
				l *= shadow_plane (shadowPlane[i]);
			} else if (shadow == ST_CUBE) {
				l *= shadow_cube (shadowCube[i]);
			}

			if (model == LM_AMBIENT) {
				minLight = max (l * lights[i].color, minLight);
			} else {
				vec3        incoming = dist / d;
				l *= spot_cone (lights[i], incoming) * diffuse (incoming, n);
				light += l * lights[i].color;
			}
		}
		light = max (light, minLight);
	}
	frag_color = vec4 (c * light + e, 1);
}
