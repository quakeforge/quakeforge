#ifndef __planetary_h
#define __planetary_h

typedef struct BodyParams {
	vec3 planetCenter;// = vec3(12770e3, -20, 20);
	float planetRadius;// = 6370e3;
} BodyParams;

typedef struct AtmosphereParams {
	float atmosphereRadius;// = 6470e3;
	float oceanRadius;// = 6370e3;
	float rayleigh;
	float mie;

	float densityFalloff;// = 4;
	vec3 scatteringCoefficients;// = vec3(0.10662224073302788,0.32444156446229333,0.6830134553650706);
} AtmosphereParams;

typedef struct PlanetaryData {
	int numOpticalDepthPoints;// = 10;
	int numInScatteringPoints;// = 10;
	float scaleFactor;// = 1e-6;
#ifdef VULKAN //FIXME sort out CPU Ruamoko pointers (currently 32-bit)
	BodyParams       *bodies;
	AtmosphereParams *atmospheres;
#else
	ulong             bodies;
	ulong             atmospheres;
#endif
} PlanetaryData;

#endif//__planetary_h
