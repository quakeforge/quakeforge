#include "general.h"
#include "integer.h"
#include "fppack.h"

[buffer, readonly, set(0), binding(2)] @block
#include "matrices.h"
;

#define INPUT_ATTACH_SET 2
#include "input_attach.h"

INPUT_ATTACH(0) subpassInput normal;
INPUT_ATTACH(1) subpassInput depth;

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
	vec3        n = subpassLoad (normal).rgb;
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

		float       namb = l.axis • l.axis;
		I *= spot_cone (unpackSnorm2x16 (l.cone), l.axis, incoming);
		I *= diffuse (incoming, n);
		I = mix (1, I, namb);
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
		light += I * col.w * col.xyz;
	}
	//light = max (light, minLight);

	frag_color = vec4 (light, 1);
}
