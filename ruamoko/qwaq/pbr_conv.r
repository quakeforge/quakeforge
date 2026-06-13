#include "GLSL/general.h"
#include "GLSL/texture.h"

[in(0)] vec2 uv;
[out(0)] vec4 frag_color;
[flat, in("ViewIndex")] int gl_ViewIndex;

typedef @sampler(@image(float,Cube)) CubeS;
typedef @image(float,Cube,Sampled) Cube;

[uniform, set(0), binding(0)] @sampler samp;
[uniform, set(0), binding(0)] Cube envMap;

const float PI = 3.1415926536;

@overload
vec4 texture(CubeS samp, vec3 dir, float lod)
	= SPV(OpImageSampleExplicitLod) [samp, dir, =ImageOperands.Lod, lod];

typedef enum [namespace] Dist {
	GGX,
	Charlie,
	Lambertian,
} dist_t;

[push_constant] @block constants {
	uvec3 miploop_base;
	uint FIXME;
	uvec3 miploop_size;
	uint miploop_level;
	uvec2 miploop_range;
	uvec2 conv_size;
	uint sampleCount;
	dist_t distribution;
};

vec3 uvToXYZ (uint face, vec2 uv)
{
	switch (face) {
	case 0: return vec3 (    1,-uv.y,-uv.x);
	case 1: return vec3 (   -1,-uv.y, uv.x);
	case 2: return vec3 ( uv.x,    1, uv.y);
	case 3: return vec3 ( uv.x,   -1,-uv.y);
	case 4: return vec3 ( uv.x,-uv.y,    1);
	case 5: return vec3 (-uv.x,-uv.y,   -1);
	}
	return vec3(0);
}

typedef struct MicrofacetDistributionSample {
	float pdf;
	float cosTheta;
	float sinTheta;
	float phi;
} MDS;

vec2 hammersley2d(uint i, uint N)
{
	uint bits = bitfieldReverse (i);
	float rdi = float(bits) * (0.25f/(1<<30));
	return vec2(float(i) / float (N), rdi);
}

mat3 generateTBN (vec3 normal)
{
	auto bitangent = vec3 (0, 1, 0);
	float NdotUp = dot (normal, bitangent);
	if (1 - abs(NdotUp) < 1e-6) {
		bitangent = NdotUp > 0 ? vec3 (0, 0, 1) : vec3 (0, 0, -1);
	}
	vec3 tangent = normalize (cross (bitangent, normal));
	bitangent = cross (normal, tangent);
	return mat3(tangent, bitangent, normal);
}

float D_GGX(float NdotH, float roughness)
{
	float a = NdotH * roughness;
	float k = roughness / (1.0 - NdotH * NdotH + a * a);
	return k * k * (1.0 / PI);
}

float D_Charlie(float sheenRoughness, float NdotH)
{
	sheenRoughness = max(sheenRoughness, 1e-6);
	float invR = 1 / sheenRoughness;
	float cos2h = NdotH * NdotH;
	float sin2h = 1 - cos2h;
	return (2 + invR) * pow (sin2h, invR * 0.5) / (2 * PI);
}

MDS GGX (vec2 xi, float roughness)
{
	MDS ggx;
	float a = roughness * roughness;
	ggx.cosTheta = clamp (sqrt ((1 - xi.y) / (1 + (a * a - 1) * xi.y)), 0, 1),
	ggx.sinTheta = sqrt (1 - ggx.cosTheta * ggx.cosTheta);
	ggx.phi = 2 * PI * xi.x;
	ggx.pdf = D_GGX (ggx.cosTheta, a) / 4;
	return ggx;
}

MDS Charlie (vec2 xi, float roughness)
{
	MDS charlie;
	float a = roughness * roughness;
	charlie.sinTheta = pow (xi.y, a / (2 * a + 1));
	charlie.cosTheta = sqrt (1 - charlie.sinTheta * charlie.sinTheta);
	charlie.phi = 2 * PI * xi.x;
	charlie.pdf = D_Charlie (a, charlie.cosTheta) / 4;
	return charlie;
}

MDS Lambertian (vec2 xi, float roughness)
{
	MDS lambertian;
	lambertian.cosTheta = sqrt (1 - xi.y);
	lambertian.sinTheta = sqrt(xi.y);
	lambertian.phi = 2 * PI * xi.x;
	lambertian.pdf = lambertian.cosTheta / PI;
	return lambertian;
}

vec4 getImportanceSample (uint sampleIndex, vec3 N, float roughness)
{
	vec2 xi = hammersley2d (sampleIndex, sampleCount);
	MDS importanceSample;

	switch (distribution) {
	case Dist.Lambertian:
		importanceSample = Lambertian (xi, roughness);
		break;
	case Dist.GGX:
		importanceSample = GGX (xi, roughness);
		break;
	case Dist.Charlie:
		importanceSample = Charlie (xi, roughness);
		break;
	}
	vec3 localSpaceDirection = normalize (vec3 (
		importanceSample.sinTheta * cos (importanceSample.phi),
		importanceSample.sinTheta * sin (importanceSample.phi),
		importanceSample.cosTheta
	));
	mat3 TBN = generateTBN (N);
	vec3 direction = TBN * localSpaceDirection;
	return vec4 (direction, importanceSample.pdf);
}

float computeLod (float pdf)
{
	auto w = float (conv_size.x);
	auto h = float (conv_size.y);
	auto sc = float (sampleCount);
	return 0.5 * log2 (6 * 2 * h / (sc * pdf));
}

vec3 filterColor (vec3 N, float roughness)
{
	vec3 color = vec3(0);
	float weight = 0;
	for (uint i = 0; i < sampleCount; i++) {
		vec4 importanceSample = getImportanceSample (i, N, roughness);
		vec3 H = importanceSample.xyz;
		float pdf = importanceSample.w;
		float lod = computeLod (pdf);
		if (distribution == Dist.Lambertian) {
			//vec3 lambertian = texture(envMap, H, lod).xyz;
			vec3 lambertian = texture(@sampler(envMap, samp), H, lod).xyz;
			color += lambertian;
		} else if (distribution == Dist.GGX || distribution == Dist.Charlie) {
			vec3 V = N;
			vec3 L = normalize (reflect (-V, H));
			float NdotL = dot(N, L);
			if (NdotL > 0) {
				if (roughness == 0) lod = 0;
				vec3 sampleColor = texture (@sampler(envMap, samp), L, lod).xyz;
				color += sampleColor * NdotL;
				weight += NdotL;
			}
		}
	}
	color /= (weight != 0) ? weight : sampleCount;
	return color;
}

[shader(Fragment)]
[capability(MultiView)]
void main ()
{
	vec2 newUV = uv * 2 - vec2(1);
	vec3 scan = uvToXYZ (gl_ViewIndex, newUV);
	vec3 direction = normalize (scan);
	float roughness = float (miploop_level) / float (miploop_range.y);
	frag_color = vec4(filterColor(direction, roughness), 1);
}
