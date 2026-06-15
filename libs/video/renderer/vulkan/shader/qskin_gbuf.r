#define __GLSL_FRAGMENT__
#include "GLSL/general.h"
#include "GLSL/texture.h"

[uniform, set(1), binding(0)] @sampler(@image(float,2D)) Palette;
[uniform, set(3), binding(0)] @sampler(@image(float,2D,Array)) Skin;

[push_constant] @block PushConstants {
	[offset(76)]
	uint        colors;
	vec4        base_color;
	vec4        orm;
	float       time;
};

[in(0)] vec2 st;
[in(1)] vec4 position;
[in(2)] vec3 normal;
[in(3)] vec4 color;

[out(0)] vec4 frag_color;
[out(1)] vec4 frag_orm;
[out(2)] vec4 frag_normal;
[out(3)] vec4 frag_emission;

[shader(Fragment)]
void
main (void)
{
	vec4        c;
	vec4        e;
	c = texture (Skin, vec3 (st, 0)) * base_color;
	e = texture (Skin, vec3 (st, 1)) * orm.a;
	vec4        rows = unpackUnorm4x8(colors);
	vec4        cmap = texture (Skin, vec3 (st, 2));
	c += texture (Palette, vec2 (cmap.x, rows.x)) * cmap.y;
	c += texture (Palette, vec2 (cmap.z, rows.y)) * cmap.w;

	frag_color = c * color;
	frag_orm = orm;
	frag_normal = vec4(normalize(normal), 1);
	frag_emission = e;
}
