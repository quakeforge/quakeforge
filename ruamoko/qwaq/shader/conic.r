#include "GLSL/general.h"
#include "GLSL/fragment.h"

[uniform, readonly, set(1), binding(0)] @block
#include "../../libs/video/renderer/vulkan/shader/matrices.h"
;

#include "common.h"

INPUT_ATTACH(0) compo;
INPUT_ATTACH(1) depth;

#include "conic.h"

[push_constant] @block PushConstants {
	conic_t    *conics;
	uint        num_conics;
};

[in("FragCoord")] vec4 gl_FragCoord;
[flat, in("ViewIndex")] int gl_ViewIndex;
[in(0)] vec2 uv;
[out(0)] vec4 frag_color;

[shader(Fragment)]
[capability(MultiView)]
void
main ()
{
	if (!conics || !num_conics) {
		frag_color = vec4(0);
		return;
	}
	// asumes non-shearing camera
	auto view = View[gl_ViewIndex];
	auto p = transpose (mat3 (view[0].xyz, view[1].xyz, view[2].xyz));
	auto cam = mat3 (p[0] / Projection3d[0][0],
					 p[1] / Projection3d[1][1],
					 p[2]);
	auto eye = point_t (-p * view[3].xyz, 1);
	vec2 UV = 2 * uv - vec2(1,1);
	auto vec = vec3 (UV, 1);
	vec /= sqrt (vec • vec);
	auto ray = eye ∨ point_t (cam * vec, 0);

	float d = subpassLoad (depth).x;
	auto PV = Projection3d * View[gl_ViewIndex];

	auto color = vec4(0);
	for (uint i = 0; i < num_conics; i++) {
		@algebra (PGA) {
			auto c = conics[i];
			auto n = c.point ∨ c.s ∨ c.t;
			point_t p = ray ∧ n;
			float t = ⋆(p * (n ∧ eye)) * e321;
			if (t >= 0) {
				continue;
			}
			float vn = ⋆(e0 ∧ p);
			p /= vn;
			vec4 proj_p = PV * vec4 (p);
			// Assumes s and t are orthogonal and unit
			auto uv = vec2 (-(c.point ∨ p) • (c.point ∨ c.s),
							-(c.point ∨ p) • (c.point ∨ c.t));
			vec2 duvdx = dFdx(uv);
			vec2 duvdy = dFdy(uv);
			// Check depth *late* in order to keep uv smooth and thus the
			// derivatives more accurate.
			if (proj_p.z < proj_p.w * d) {
				continue;
			}
			float l = c.slr;
			float e = c.ecc;
			float x = uv.x;
			float y = uv.y;
			float F = (1 - e*e)*x*x + 2*l*e*x + y*y - l*l;
			auto delF = 2*vec2 ((1 - e*e)*x + l*e, y);
			auto df = vec2 (duvdx • delF, duvdy • delF);
			float dd = F * F / (df • df);
			float alpha = c.width - sqrt (dd) / (abs(vn));
			alpha = max (min(alpha, 1), 0);
			color += vec4(1)*alpha;
		}
	}
	frag_color = color;
}
