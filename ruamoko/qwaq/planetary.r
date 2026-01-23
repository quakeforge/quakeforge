#include "GLSL/general.h"
#include "GLSL/texture.h"
#include "GLSL/fragment.h"

#include "planetary.h"

[uniform, set(1), binding(0)] @sampler(@image(float,2D)) Palette;
[uniform, set(3), binding(0)] @sampler(@image(float,2D,Array)) SurfMap;

[push_constant] @block PushConstants {
	mat4  Model;
	uint  enabled_mask;
	float blend;
	uint  debug_bone;
	uint  colors;
	vec4  base_color;
	vec4  fog;
	PlanetaryData *planetary;
	float near_plane;
};

vec4
surf_map (vec3 dir, int layer)
{
	const float pi = 3.14159265358979;
	// equirectangular images go from left to right, top to bottom, but
	// the computed angles go right to left, bottom to top, so need to flip
	// vertical for the maps to be the right way round.
	const vec2 conv = vec2(1/(2*pi), -1/pi);

	vec3 rid = -dir;
	float x1 = atan (dir.y, dir.x);
	float x2 = atan (rid.y, rid.x);
	float y = atan (dir.z, length(dir.xy));
	vec2 uv1 = vec2 (x1, y) * conv + vec2(0.5, 0.5);
	vec2 uv2 = vec2 (x2, y) * conv + vec2(0.0, 0.5);
	//FIXME mipmaps mess this up
	vec2 uv = fwidth(uv1.x) > 0.5 ? uv2: uv1;
	return texture (SurfMap, vec3(uv, layer));
}

[in(0)] vec2 uv;
[in(1)] vec4 position;
[in(2)] vec3 normal;
[in(3)] vec4 color;

[out(0)] vec4 frag_color;
//[out(1)] vec4 frag_emission;
//[out(2)] vec4 frag_normal;

[shader("Fragment")]
[capability("MultiView")]
void
main ()
{
	auto dir = (position - Model[3]).xyz;
	auto local_dir = vec3 (
		dir • normalize(Model[0].xyz),
		dir • normalize(Model[1].xyz),
		dir • normalize(Model[2].xyz)
	);
	auto sun = planetary.bodies;
	float l = 1;
	if (sun) {
		auto moon = planetary.bodies + 2;
		auto p = position.xyz;
		auto s = sun.planetCenter;
		auto R = sun.planetRadius;
		auto b = moon.planetCenter;
		auto r = moon.planetRadius;

		float x = (p - b) • normalize(b - s);
		if (x > 0) {
			float y = length ((p - b) - x * normalize (b - s));
			float D = length (b - s);
			float mo = (R + r) / D;
			float mi = (R - r) / D;
			float ho = r / mo;
			float hi = r / mi;
			float yo = mo * (x + ho);
			float yi = mi * (x - hi);
			float lo = (y - abs(yi))/(yo - abs(yi));
			float R1 = r*(D + x)/(R * x);
			float li = yi > 0 ? 1 - R1 * R1 : 0;
			l = mix (li, 1, clamp(lo, 0, 1));
		}

		auto d = position.xyz - sun.planetCenter;
		l *= max(0, normalize(dir) • -normalize(d));
	}

	vec4 c, e;
	c = surf_map(local_dir, 0);
	e = surf_map(local_dir, 1) * 2;
	//vec4 rows = unpackUnorm4x8(colors);
	//vec4        cmap = surf_map (dir, 2);
	//c += texture (Palette, vec2 (cmap.x, rows.x)) * cmap.y;
	//c += texture (Palette, vec2 (cmap.z, rows.y)) * cmap.w;
	frag_color = l * c * color + e * 2;
	//frag_emission = e;
	//frag_normal = vec4(normalize(dir), 1);
}
