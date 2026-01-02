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

//[push_constant] @block PushConstants {
	const float       near_plane = 0.1;
//};

//[buffer, readonly, set(1), binding(2)] @block Params {
const vec3 planetCenter = vec3(12770e3, -20, 20);
const float planetRadius = 6370e3;
const float atmosphereRadius = 6470e3;
const float oceanRadius = 6370e3;
const float densityFalloff = 4;
const int numOpticalDepthPoints = 10;
const int numInScatteringPoints = 10;
const float scaleFactor = 1e-6;
const vec3 scatteringCoefficients = vec3(0.10662224073302788,0.32444156446229333,0.6830134553650706) * scaleFactor;
const vec3 dirToSun = vec3(-0.48,-0.64,0.6);
//};

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
densityAtPoint(vec3 densitySamplePoint) {
	float heightAboveSurface = length(densitySamplePoint - planetCenter) - planetRadius;
	float height01 = heightAboveSurface / (atmosphereRadius - planetRadius);
	float localDensity = exp(-height01 * densityFalloff) * (1 - height01);
	return localDensity;
}

float
opticalDepth(vec3 rayOrigin, vec3 rayDir, float rayLength)
{
	vec3 densitySamplePoint = rayOrigin;
	float stepSize = rayLength / (numOpticalDepthPoints - 1);
	float opticalDepth = 0;

	for (int i = 0; i < numOpticalDepthPoints; i++) {
		float localDensity = densityAtPoint (densitySamplePoint);
		opticalDepth += localDensity * stepSize * scaleFactor;
		densitySamplePoint += rayDir * stepSize;
	}
	return opticalDepth;
}

vec3
calculateLight (vec3 rayOrigin, vec3 rayDir, float rayLength, vec3 originalCol) {
	vec3 inScatterPoint = rayOrigin;
	float stepSize = rayLength / (numInScatteringPoints - 1);
	vec3 inScatteredLight = vec3(0);
	float viewRayOpticalDepth = 0;

	for (int i = 0; i < numInScatteringPoints; i++) {
		float sunRayLength = raySphere(planetCenter, atmosphereRadius, inScatterPoint, dirToSun).thickness;
		float sunRayOpticalDepth = opticalDepth(inScatterPoint, dirToSun, sunRayLength);
		viewRayOpticalDepth = opticalDepth(inScatterPoint, -rayDir, stepSize * i);
		vec3 transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth) * scatteringCoefficients);
		float localDensity = densityAtPoint(inScatterPoint);

		inScatteredLight += localDensity * transmittance * scatteringCoefficients * stepSize;
		inScatterPoint += rayDir * stepSize;
	}
	float originalColTransmittance = exp(-viewRayOpticalDepth);
	return originalCol * originalColTransmittance + inScatteredLight;
}

[in("FragCoord")] vec4 gl_FragCoord;
[flat, in("ViewIndex")] int gl_ViewIndex;
[in(0)] vec2 uv;
[out(0)] vec4 frag_color;

[shader("Fragment")]
[capability("MultiView")]
void
main ()
{
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

	vec4 originalCol = subpassLoad (compo);

	float sceneDepth = length (p.xyz);

	vec3 rayOrigin = InvView[gl_ViewIndex][3].xyz;
	vec3 rayDir = normalize(dir);

	float dstToOcean = raySphere(planetCenter, oceanRadius, rayOrigin, rayDir).distance;
	float dstToSurface = sceneDepth < p.w * dstToOcean ? sceneDepth / p.w : dstToOcean;

	auto hitInfo = raySphere(planetCenter, atmosphereRadius, rayOrigin, rayDir);
	float dstToAtmosphere = hitInfo.distance;
	float dstThroughAtmosphere = min(hitInfo.thickness, dstToSurface - dstToAtmosphere);

	if (dstThroughAtmosphere > 0) {
		const float epsilon = 0.0001;
		vec3 pointInAtmosphere = rayOrigin + rayDir * (dstToAtmosphere + epsilon);
		vec3 light = calculateLight(pointInAtmosphere, rayDir, dstThroughAtmosphere - epsilon * 2, originalCol.rgb);
		frag_color = vec4(light, originalCol.a);
	} else {
		frag_color = originalCol;
	}
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
