#include "GLSL/general.h"

[push_constant] @block constants {
	uvec2 brdf_size;
	uint *data;
};

const float PI = 3.1415926536;

[in("GlobalInvocationId")] uvec3 GlobalInvocationId;
[constant_id(0)] const uint NUM_SAMPLES = 1024;

vec2 hammersley2d(uint i, uint N)
{
	uint bits = bitfieldReverse (i);
	float rdi = float(bits) * (0.25f/(1<<30));
	return vec2(float(i) / float (N), rdi);
}

float random(vec2 co) {
	const float a = 12.9898;
	const float b = 78.233;
	const float c = 43758.5453;
	float dt = dot (co, vec2 (a, b));
	float sn = mod (dt, 3.14);
	return fract (sin (sn) * c);
}

vec3 spherical (const vec2 csTheta, const float phi, const vec3 normal)
{
	auto csPhi = vec2 (cos(phi), sin(phi));
	auto H = vec3(csTheta.y * csPhi, csTheta.x);

	vec3 up = abs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
	vec3 tangentX = normalize(cross(up, normal));
	vec3 tangentY = normalize(cross(normal, tangentX));
	return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

vec3 importanceSample_GGX (vec2 Xi, float roughness, vec3 normal)
{
	float alpha = roughness * roughness;
	float phi = 2 * PI * Xi.x + random (normal.xz) * 0.1;
	float cosTheta = sqrt ((1 - Xi.y) / (1 + (alpha * alpha - 1) * Xi.y));
	float sinTheta = sqrt (1 - cosTheta * cosTheta);

	return spherical (vec2 (cosTheta, sinTheta), phi, normal);
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float k = (roughness * roughness) / 2;
	float GL = dotNL / (dotNL * (1 - k) + k);
	float GV = dotNV / (dotNV * (1 - k) + k);
	return GL * GV;
}

float V_Ashikhmin(float NdotL, float NdotV)
{
	return clamp (1/(4 * (NdotL + NdotV - NdotL * NdotV)), 0, 1);
}

float D_Charlie(float sheenRoughness, float NdotH)
{
	sheenRoughness = max(sheenRoughness, 1e-6);
	float invR = 1 / sheenRoughness;
	float cos2h = NdotH * NdotH;
	float sin2h = 1 - cos2h;
	return (2 + invR) * pow (sin2h, invR * 0.5) / (2 * PI);
}

vec3 importanceSample_Charlie (vec2 Xi, float roughness, vec3 normal)
{
	float alpha = roughness * roughness;
	float phi = 2 * PI * Xi.x;
	float sinTheta = pow(Xi.y, alpha / (2 * alpha + 1));
	float cosTheta = sqrt (1 - sinTheta * sinTheta);

	return spherical (vec2 (cosTheta, sinTheta), phi, normal);
}

vec3 BRDF (float NoV, float roughness)
{
	const vec3 N = vec3(0,0,1);
	vec3 V = vec3 (sqrt(1 - NoV * NoV), 0, NoV);
	vec3 LUT = vec3(0);
	for (uint i = 0; i < NUM_SAMPLES; i++) {
		vec2 Xi = hammersley2d (i, NUM_SAMPLES);
		vec3 H = importanceSample_GGX (Xi, roughness, N);
		vec3 L = 2 * dot(V, H) * H - V;
		float dotNL = max(dot(N, L), 0);
		float dotNV = max(dot(N, V), 0);
		float dotVH = max(dot(V, H), 0);
		float dotNH = max(dot(N, H), 0);
		if (dotNL > 0) {
			float G = G_SchlicksmithGGX (dotNL, dotNV, roughness);
			float G_Vis = (G * dotVH) / (dotNH * dotNV);
			float Fc = pow (1 - dotVH, 5);
			LUT.rg += vec2((1 - Fc), Fc) * G_Vis;
		}
	}
	for (uint i = 0; i < NUM_SAMPLES; i++) {
		vec2 Xi = hammersley2d (i, NUM_SAMPLES);
		vec3 H = importanceSample_Charlie (Xi, roughness, N);
		vec3 L = 2 * dot(V, H) * H - V;
		float dotNL = max(dot(N, L), 0);
		float dotNV = max(dot(N, V), 0);
		float dotVH = max(dot(V, H), 0);
		float dotNH = max(dot(N, H), 0);
		if (dotNL > 0) {
			float sheenDistribution = D_Charlie (roughness, dotNH);
			float sheenVisibility   = V_Ashikhmin (dotNL, dotNV);
			LUT.b += sheenVisibility * sheenDistribution * dotNL * dotVH;
		}
	}
	return LUT / NUM_SAMPLES;
}

[shader(GLCompute, LocalSize=[16,16,1])]
void main ()
{
	vec2 uv = (vec2(GlobalInvocationId.xy) + 0.5) / vec2 (brdf_size);
	vec3 v = BRDF (uv.x, uv.y);
	uint offset = GlobalInvocationId.y * brdf_size.x + GlobalInvocationId.x;
	//FIXME implement float16_t etc
	data[offset * 2 + 0] = packHalf2x16 (v.xy);
	data[offset * 2 + 1] = packHalf2x16 (vec2(v.z, 0));
}
