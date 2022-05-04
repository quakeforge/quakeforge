#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput depth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput color;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput emission;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput normal;
layout (input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput position;

struct LightData {
	vec4        color;		// .a is intensity
	vec4        position;	// .w = 0 -> directional, .w = 1 -> point/cone
	vec4        direction;	// .w = -cos(cone_angle/2) (1 for omni/dir)
	vec4        attenuation;
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
	LightData   lights[MaxLights];
	int         lightCount;
	//mat4        shadowMat[MaxLights];
	//vec4        shadowCascale[MaxLights];
};

layout (location = 0) out vec4 frag_color;

float
spot_cone (LightData light, vec3 incoming)
{
	vec3        dir = light.direction.xyz;
	float       cone = light.direction.w;
	float       spotdot = dot (incoming, dir);
	return 1 - smoothstep (cone, cone + 0.02, spotdot);
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

	if (MaxLights > 0) {
		vec3        minLight = vec3 (0);
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
		light = max (light, minLight);
	}
	frag_color = vec4 (c * light + e, 1);
}
