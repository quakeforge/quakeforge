#include <math.h>

#include "qwaq-ed.h"

#include "shader/planetary.h"

void
update_orrery (entity_t earth, double time)
{
	float ph = float(0.05 * (time - double (1ul<<32)));
	quaternion trot = { 0, 0, sin(ph), cos(ph) };
	quaternion vrot = '0 0 0 1';//{ 0, -sqrt(0.5f), 0, sqrt(0.5f) };

	auto xform = Entity_GetTransform (earth);
	Transform_SetLocalRotation(xform, vrot * trot);
	vec4 pos = Transform_GetWorldPosition (xform);

	ulong addr = Render_BufferAddress ("planetary");
	PlanetaryData planetary = {
		.numOpticalDepthPoints = 10,
		.numInScatteringPoints = 10,
		.scaleFactor = 1e-6,
		.bodies = addr + sizeof (PlanetaryData),
		.atmospheres = addr + sizeof (PlanetaryData) + sizeof (BodyParams) * 3,
	};
	BodyParams bodies[] = {
		{// sun
		.planetCenter = '-71987230000.0 -96000000020.0 90000000020.0',
		.planetRadius = 695700e3,
		},
		{// earth
		.planetCenter = pos.xyz,
		.planetRadius = 6370e3,
		},
		{// moon
		.planetCenter = '-171550000.0 -245760020.0 230400020.0',
		.planetRadius = 1738e3,
		},
	};
	AtmosphereParams atmospheres[] = {
		{// sun
		.atmosphereRadius = 13655700e3,
		.oceanRadius = 695700e3,
		.densityFalloff = 4e2,
		//.scatteringCoefficients = '0.10662224073302788 0.32444156446229333 0.6830134553650706',
		.scatteringCoefficients = '0.6830134553650706 0.5830134553650706 0.32444156446229333',
		},
		{// earth
		.atmosphereRadius = 6470e3,
		.oceanRadius = 6370e3,
		.densityFalloff = 4,
		.scatteringCoefficients = '0.10662224073302788 0.32444156446229333 0.6830134553650706',
		}
	};
	Render_UpdateBuffer ("planetary", 0, &planetary, sizeof (planetary));
	Render_UpdateBuffer ("planetary", sizeof(planetary), &bodies, sizeof (bodies));
	Render_UpdateBuffer ("planetary", sizeof(planetary)+sizeof(bodies), &atmospheres, sizeof (atmospheres));
}
