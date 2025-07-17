#include "general.h"
#include "fppack.h"

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

#include "lighting.h"

[in(0)] uint light_index;
[in(1)] float light_radius;
[in(2)] vector splat_vert;
[out(0)] uint light_index_out;

quaternion	// assumes a and b are unit vectors
from_to_rotation (vector a, vector b)
{
	float d = (a + b) @dot (a + b);
	float qc = sqrt (d);
	vec3  qv = d > 1e-6 ? (a @cross b) / qc : vec3 (1, 0, 0);
	return quaternion (qv, qc * 0.5);
}

[out("Position")] vec4 gl_Position;
[in("ViewIndex")] int gl_ViewIndex;
[in("InstanceIndex")] int gl_InstanceIndex;

[shader("Vertex")]
[capability("MultiView")]
void
main (void)
{
	LightData l = lights[light_index];
	float sz = light_radius;
	vec2 cone = unpackSnorm2x16 (l.cone);
	float c = cone.x;
	float sxy = sz * (c < 0 ? (sqrt (1 - c*c) / -c) : 1);
	vec3 scale = vec3 (sxy, sxy, sz);

	quaternion q = from_to_rotation (vec3 (0, 0, -1), l.axis);
	vec4 pos = vec4 ((q * splat_vert) * scale, 0);

	gl_Position = Projection3d * (View[gl_ViewIndex] * (pos + l.position));
	light_index_out = light_index;
}
