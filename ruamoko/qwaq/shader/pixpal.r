#include <GLSL/texture.h>

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
	vec4 a = texture (Skin, vec3 (st, 2));
	auto uv = vec2(st.x, st.y - time * a.b);
	vec4 c = texture (Skin, vec3 (uv, 0));
	vec4 e = texture (Skin, vec3 (uv, 1));

	frag_color = c * color * base_color;
	frag_orm = vec4(orm.r, orm.g * (1 - a.g), orm.b * a.r, 1);
	frag_normal = vec4(normal, 0);
	frag_emission = e * orm.a;
}
