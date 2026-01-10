#define SHADER "../../libs/video/renderer/vulkan/shader/"

[uniform, readonly, set(1), binding(0)] @block
#include "../../libs/video/renderer/vulkan/shader/matrices.h"
;

#include "../../libs/video/renderer/vulkan/shader/general.h"

#include "../../libs/video/renderer/vulkan/shader/subpassInput.h"
#define INPUT_ATTACH(ind) \
	[uniform, input_attachment_index(ind), set(0), binding(ind)] \
	@image(float, SubpassData)
INPUT_ATTACH(0) compo;
INPUT_ATTACH(1) depth;

#include "planetary.h"

[push_constant] @block PushConstants {
	PlanetaryData *planetary;
	float near_plane;
};

typedef struct HitInfo {
	float distance;		// 0 if inside
	float thickness;
} HitInfo;

HitInfo
raySphere (vec3 sphereCenter, float sphereRadius, vec3 rayOrigin, vec3 rayDir)
{
	vec3 offset = rayOrigin - sphereCenter;
	float a = 1; // Set to rayDir • rayDir if rayDir might not be normalized
	float b = 2 * offset • rayDir;
	float c = offset • offset - sphereRadius * sphereRadius;
	float d = b * b - 4 * a * c; // Discriminat from quadratic formula

	// Number of intersections: 0 when d < 0; 1 when d = 0; 2 when d > 0
	if (d > 0) {
		float s = sqrt(d);
		float dstToSphereNear = max(0, (-b - s) / (2 * a));
		float dstToSphereFar = (-b + s) / (2 * a);

		// Ignore intersections that occur behid the ray
		if (dstToSphereFar >= 0) {
			return {dstToSphereNear, dstToSphereFar - dstToSphereNear};
		}
	}
	// Ray did not intersect sphere
	return {__INFINITY__, 0f};//FIXME no promote on init
}

float
densityAtPoint(BodyParams *B, AtmosphereParams *A, vec3 densitySamplePoint) {
	float heightAboveSurface = length(densitySamplePoint - B.planetCenter) - B.planetRadius;
	float height01 = heightAboveSurface / (A.atmosphereRadius - B.planetRadius);
	float localDensity = exp(-height01 * A.densityFalloff) * (1 - height01);
	return localDensity;
}

float
opticalDepth(PlanetaryData *P, BodyParams *B, AtmosphereParams *A,
			 vec3 rayOrigin, vec3 rayDir, float rayLength)
{
	vec3 densitySamplePoint = rayOrigin;
	float stepSize = rayLength / (P.numOpticalDepthPoints - 1);
	float opticalDepth = 0;

	for (int i = 0; i < P.numOpticalDepthPoints; i++) {
		float localDensity = densityAtPoint (B, A, densitySamplePoint);
		opticalDepth += localDensity * stepSize * P.scaleFactor;
		densitySamplePoint += rayDir * stepSize;
	}
	return opticalDepth;
}

vec3
calculateLight (PlanetaryData *P, BodyParams *B, AtmosphereParams *A,
				vec3 rayOrigin, vec3 rayDir, float rayLength, vec3 originalCol,
				vec3 dirToSun) {
	vec3 inScatterPoint = rayOrigin;
	float stepSize = rayLength / (P.numInScatteringPoints - 1);
	vec3 inScatteredLight = vec3(0);
	float viewRayOpticalDepth = 0;
	vec3 scatter = A.scatteringCoefficients * P.scaleFactor;

	for (int i = 0; i < P.numInScatteringPoints; i++) {
		float sunRayLength = raySphere(B.planetCenter, A.atmosphereRadius, inScatterPoint, dirToSun).thickness;
		float sunRayOpticalDepth = opticalDepth(P, B, A, inScatterPoint, dirToSun, sunRayLength);
		viewRayOpticalDepth = opticalDepth(P, B, A, inScatterPoint, -rayDir, stepSize * i);
		vec3 transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth) * scatter);
		float localDensity = densityAtPoint(B, A, inScatterPoint);

		inScatteredLight += localDensity * transmittance * scatter * stepSize;
		inScatterPoint += rayDir * stepSize;
	}
	float originalColTransmittance = exp(-viewRayOpticalDepth);
	return originalCol * originalColTransmittance + inScatteredLight;
}

[in("FragCoord")] vec4 gl_FragCoord;
[flat, in("ViewIndex")] int gl_ViewIndex;
[in(0)] vec2 uv;
[out(0)] vec4 frag_color;

vec4
atmosphere_color (vec4 originalCol, uint body)
{
	auto P = planetary;
	auto B = planetary.bodies + body;
	auto A = planetary.atmospheres + body;
	auto sun = planetary.bodies;

	vec3        light = vec3 (0);
	float       d = subpassLoad (depth).x;
	vec4        invp = vec4 (
					near_plane/Projection3d[0][0],
					near_plane/Projection3d[1][1],
					near_plane,
					1);
	vec4        p = vec4 ((2 * uv - 1), 1, d) * invp;
	vec3 dir = (InvView[gl_ViewIndex] * vec4(p.xyz, 0)).xyz;
	p = InvView[gl_ViewIndex] * p;

	vec3 dirToSun = normalize (sun.planetCenter - p.xyz);
	//if (gl_FragCoord.x == 0.5 && gl_FragCoord.y == 0.5) {
	//	printf ("dirToSun: %v3f", dirToSun);
	//}

	float sceneDepth = length (p.xyz);

	vec3 rayOrigin = InvView[gl_ViewIndex][3].xyz;
	vec3 rayDir = normalize(dir);

	float dstToOcean = raySphere(B.planetCenter, A.oceanRadius, rayOrigin, rayDir).distance;
	float dstToSurface = sceneDepth < p.w * dstToOcean ? sceneDepth / p.w : dstToOcean;

	auto hitInfo = raySphere(B.planetCenter, A.atmosphereRadius, rayOrigin, rayDir);
	float dstToAtmosphere = hitInfo.distance;
	float dstThroughAtmosphere = min(hitInfo.thickness, dstToSurface - dstToAtmosphere);

	if (dstThroughAtmosphere > 0) {
		const float epsilon = 0.0001;
		vec3 pointInAtmosphere = rayOrigin + rayDir * (dstToAtmosphere + epsilon);
		vec3 light = calculateLight(P, B, A, pointInAtmosphere, rayDir, dstThroughAtmosphere - epsilon * 2, originalCol.rgb, dirToSun);
		return vec4(light, originalCol.a);
	} else {
		return originalCol;
	}
}

[shader("Fragment")]
[capability("MultiView")]
void
main ()
{
	vec4 color = subpassLoad (compo);

	//if (gl_FragCoord.x == 0.5 && gl_FragCoord.y == 0.5) {
	//	printf ("%p %p %p", planetary, planetary.bodies, planetary.atmospheres);
	//}
	if ((!(uvec2)planetary.bodies) || (!(uvec2)planetary.atmospheres)) {
		frag_color = color;
		frag_color = vec4(1,0,1,1);
		return;
	}

	color = atmosphere_color (color, 0);
	color = atmosphere_color (color, 1);
	frag_color = color;
}

#if 0
float2 rayBoxDst(float3 boundsMin, float3 boundsMax, float3 rayOrigin, float3 rayDir) {
	// Adapted from: http://jcgt.org/published/0007/03/04/
	float3 t0 = (boundsMin - rayOrigin) / rayDir;
	float3 t1 = (boundsMin - rayOrigin) / rayDir;
	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);

	float dstA = max(max(tmin.x, tmin.y), tmin.z);
	float dstB = min(tmax.x, min(tmax.y, tmax.z));

	// CASE 1: ray intersects box from outside (0 <= dstA <= dstB)
	// dstA is dst to nearest intersectino, dstB dst to far intersection

	// CASE 2: ray intersects box from inside (dstA < 0 < dstB)
	// dstA is the dst to intersection behind the ray, dstB is dst to forward intersection

	// CASE 3: ray misses box (dstA > dstB)
	float dstToBox = max(0, dstA);
	float dstInsideBox = max(0, dstB - dstToBox);
	return float2(dstToBox, dstInsideBox);
}

[CreateAssetMenu (menuName = "Celestial Body/Atmosphere")]
public class AtmosphereSettings : ScriptableObject {
	public Shader atmosphereShader;
	public int inScatteringPoints = 10;
	public int opticalDepthPoints = 10;
	public float densityFalloff = 4;
	public float atmosphereScale = 1;
	public Vector3 wavelengths = new Vector3(700, 530, 440);
	public float scatteringStrength = 1;

	public void SetProperties (Material material, float bodyRadius) {
		float scatterR = Pow(400 / wavelengths.x, 4) * scatteringStrength;
		float scatterG = Pow(400 / wavelengths.y, 4) * scatteringStrength;
		float scatterB = Pow(400 / wavelengths.z, 4) * scatteringStrength;
		Vector3 scatteringCoefficients = new Vector3 (scatterR, scatterG, scatterB);

		material.SetVector("scatteringCoefficients", scatteringCoefficients);
		material.SetInt("numInScatteringPoints", inScatteringPoints);
		material.SetInt("numOpticalDepthPoints", opticalDepthPoints);
		material.SetFloat("atmosphereRadius", atmosphereRadius);
		material.SetFloat("planetRadius", planetRadius);
		material.SetFloat("densityFalloff", densityFalloff);
	}
}
//momentsingraphics.de/BlueNoise.html
#endif
