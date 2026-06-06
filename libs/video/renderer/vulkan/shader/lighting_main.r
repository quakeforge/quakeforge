#include "GLSL/general.h"

const float PI = 3.1415926536;	//FIXME math.h!

[buffer, readonly, set(0), binding(2)] @block
#include "matrices.h"
;

#define INPUT_ATTACH_SET 2
#include "input_attach.h"

INPUT_ATTACH(0) subpassInput color;
INPUT_ATTACH(1) subpassInput orm;
INPUT_ATTACH(2) subpassInput normal;
INPUT_ATTACH(3) subpassInput depth;

[in("FragCoord")] vec4 gl_FragCoord;
[flat, in("ViewIndex")] int gl_ViewIndex;
[in(0)] vec2 uv;
[out(0)] vec4 frag_color;

float
spot_cone (vec2 cone, vec3 axis, vec3 dir)
{
	float adot = axis • dir;
	return smoothstep (cone.x, mix (cone.x, 1, cone.y), -adot);
}

float DistributionGGC (vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(N • H, 0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX (float NdotV, float roughness)
{
	float r = (roughness + 1);
	float k = (r * r) / 8;

	float num = NdotV;
	float denom = NdotV * (1 - k) + k;

	return num / denom;
}

float GeometrySmith (vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max (N • V, 0);
	float NdotL = max (N • L, 0);
	float ggx2 = GeometrySchlickGGX (NdotV, roughness);
	float ggx1 = GeometrySchlickGGX (NdotL, roughness);

	return ggx1 * ggx2;
}


float
spec_factor (vec3 N, vec3 V, vec3 L, vec3 H, float roughness)
{
	float NDF = DistributionGGC (N, H, roughness);
	float G   = GeometrySmith (N, V, L, roughness);
	float numerator = NDF * G;
	float denominator = 4 * max (N • V, 0) * max (N • L, 0) + 1e-4;
	return numerator / denominator;
}

vec3 fresnelSchlick (float cosTheta, vec3 F0)
{
	return F0 + (1 - F0) * pow (clamp (1 - cosTheta, 0, 1), 5);
}

vec3
fresnel (vec3 albedo, float metalic, float HdotV)
{
	vec3 F0 = vec3 (0.04);
	F0 = mix (F0, albedo, metalic);
	vec3 F = fresnelSchlick (HdotV, F0);
	return F;
}

#include "fog.finc"

[shader(Fragment)]
[capability(MultiView)]
void
main (void)
{
	vec3        alb = subpassLoad (color).rgb;
	vec3        o_r_m = subpassLoad (orm).rgb;
	float       roughness = o_r_m.g;
	float       metalic = o_r_m.b;
	vec3        n = normalize(subpassLoad (normal).rgb);
	vec3        light = vec3 (0);
	float       d = subpassLoad (depth).x;
	vec4        invp = vec4 (
					near_plane/Projection3d[0][0],
					near_plane/Projection3d[1][1],
					near_plane,
					1);
	vec4        p = vec4 ((2 * uv - 1), 1, d) * invp;
	p = InvView[gl_ViewIndex] * p;
	if (!p.w) {
		frag_color = vec4 (light, 1);
		return;
	}
	p /= p.w;
	vec3 V = normalize (InvView[gl_ViewIndex][3].xyz - p.xyz);
	p.w = d;

	uint        start = queue.start;
	uint        end = start + queue.count;

	for (uint i = start; i < end; i++) {
		uint        id = lightIds[i];
		LightData   l = lights[id];
		vec3        dir = l.position.xyz - l.position.w * p.xyz;
		float       r2 = dir • dir;
		vec4        a = l.attenuation;

		if (l.position.w * a.w * a.w * r2 >= 1) {
			continue;
		}
		vec4        r = vec4 (r2, sqrt(r2), 1, 0);
		vec3        incoming = dir / r.y;
		float       I = (1 - a.w * r.y) / (a • r);

		auto rd = renderer[id];
		I *= shadow (rd.map_id, rd.layer, rd.mat_id, p, n, l.position.xyz);

		float NdotL = max (n • incoming, 0);

		I *= spot_cone (unpackSnorm2x16 (l.cone), l.axis, incoming);
		I *= NdotL;

		//float       namb = l.axis • l.axis;
		//I = mix (1, I, namb);

		vec4 col = l.color;
		if (rd.no_style == 0) {
			col *= style[renderer[id].style];
		}
		if (fog.w > 0) {
			col = FogTransmit (col, fog, r[1]);
		}
#ifdef DEBUG_SHADOW
		col = DEBUG_SHADOW(p, l.position.xyz, rd.mat_id);
#endif
		vec3 H = normalize (V + incoming);

		float HdotV = max (H • V, 0);
		vec3 kS = fresnel (alb, metalic, HdotV);
		vec3 kD = (1 - kS);

		vec3 specular = kS * spec_factor (n, V, incoming, H, roughness) * spec;
		vec3 diffuse = kD * (1 - metalic) * diff;
		light += (diffuse * alb / PI + specular) * I * col.w * col.xyz;
	}
	//light = max (light, minLight);

	frag_color = vec4 (light, 1);
}
