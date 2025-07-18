#include "general.h"
#include "integer.h"
#include "fppack.h"

#define INPUT_ATTACH_SET 2
#include "input_attach.h"

[in("FragCoord")] vec4 gl_FragCoord;
[out(0)] vec4 frag_color;

float
spot_cone (vec2 cone, vec3 axis, vec3 dir)
{
	float adot = axis • dir;
	return smoothstep (cone.x, mix (cone.x, 1, cone.y), -adot);
}

float
diffuse (vec3 incoming, vec3 normal)
{
	return clamp (incoming • normal, 0, 1);
}

#include "fog.finc"

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	vec3        c = subpassLoad (color).rgb;
	vec3        e = subpassLoad (emission).rgb;
	vec3        n = subpassLoad (normal).rgb;
	vec4        p = subpassLoad (position);
	vec3        light = vec3 (0);

	uint        start = bitfieldExtract (queue, 0, 16);
	uint        end = start + bitfieldExtract (queue, 16, 16);

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

		uint        id_data = renderer[id].id_data;
		uint        mat_id = bitfieldExtract (id_data, 0, 14);
		uint        map_id = bitfieldExtract (id_data, 14, 5);
		uint        layer = bitfieldExtract (id_data, 19, 11);

		I *= shadow (map_id, layer, mat_id, p, n, l.position.xyz);

		float       namb = l.axis • l.axis;
		I *= spot_cone (unpackSnorm2x16 (l.cone), l.axis, incoming);
		I *= diffuse (incoming, n);
		I = mix (1, I, namb);
		vec4 col = l.color;
		if (bitfieldExtract(id_data, 31, 1) == 0) {
			col *= style[renderer[id].style];
		}
		if (fog.w > 0) {
			col = FogTransmit (col, fog, r[1]);
		}
#ifdef DEBUG_SHADOW
		col = DEBUG_SHADOW(p);
#endif
		light += I * col.w * col.xyz;
	}
	//light = max (light, minLight);

	frag_color = vec4 (light, 1);
}
