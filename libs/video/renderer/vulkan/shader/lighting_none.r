#define __GLSL_FRAGMENT__
#include "GLSL/general.h"
#include "GLSL/texture.h"

const float PI = 3.1415926536;	//FIXME math.h!

[buffer, readonly, set(0), binding(2)] @block
#include "matrices.h"
;

#include "lighting.h"

typedef @sampler(@image(float,Cube,Array)) LightProbe;
typedef @sampler(@image(float,2D)) BRDF_LUT;
[uniform, set(3), binding(0)] LightProbe light_probes[32];
[uniform, set(3), binding(1)] BRDF_LUT brdf_lut;

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

vec3 fresnelSchlickRoughness(float NdotV, vec3 F0, float roughness)
{
	float a = clamp(1 - NdotV, 0, 1);
	a = (a * a) * (a * a) * a;
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * a;
}

[shader(Fragment)]
[capability(MultiView)]
void
main (void)
{
	vec3        albedo = subpassLoad (color).rgb;
	vec3        o_r_m = subpassLoad (orm).rgb;
	float       roughness = o_r_m.g;
	float       metalic = o_r_m.b;
	vec3        N = normalize(subpassLoad (normal).rgb);
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

	int max_lod = 7;//FIXME unhardcode

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

		auto rd = renderer[id];

		//float       namb = l.axis • l.axis;
		//I = mix (1, I, namb);

		vec3 F0 = mix (vec3 (0.04), albedo, metalic);
		vec3 kS = fresnelSchlickRoughness (N • V, F0, roughness);
		vec3 kD = 1 - kS;
		vec3 irradiance = texture(light_probes[0], vec4(N.xzy, 2)).rgb;

		vec3 R = reflect (-V, N);
		vec3 prefilteredColor = textureLod (light_probes[0], vec4(R.xzy, 0),
											roughness * max_lod).rgb;
		vec2 envBRDF = texture (brdf_lut, vec2 (N • V, roughness)).rg;
		vec3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

		auto color = l.color;
		if (rd.no_style == 0) {
			color *= style[renderer[id].style];
		}
		irradiance *= color.w * color.xyz;
		vec3 diffuse = irradiance * albedo;
		vec3 ambient = (kD * diffuse + specular) * o_r_m.r;
		light += ambient;
	}
	//light = max (light, minLight);

	frag_color = vec4 (light, 1);
}
