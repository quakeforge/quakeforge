#define __GLSL_FRAGMENT__
#include <GLSL/general.h>
#include <GLSL/texture.h>

[uniform, set(3), binding(0)] @sampler(@image(float,2D,Array)) Texture;

[push_constant] @block PushConstants {
	vec4        orm;
	vec4        fog;
	float       time;
	float       alpha;
};

[in(0)] vec2 tex_st;
[in(1)] vec3 lmap_stp;
[in(2)] vec3 direction;
[in(3)] vec3 normal;
[in(4)] vec4 position;
[in(5)] vec4 color;

[out(0)] vec4 frag_color;
[out(1)] vec4 frag_orm;
[out(2)] vec4 frag_normal;
[out(3)] vec4 frag_emission;

[in("FragCoord")] vec4 gl_FragCoord;

[shader(Fragment)]
void
main (void)
{
	vec4        c = vec4 (0);
	vec4        e;
	vec3        t_st = vec3 (tex_st, 0);
	vec3        e_st = vec3 (tex_st, 1);

	c = texture (Texture, t_st) * color;
	e = texture (Texture, e_st) * 2;//FIXME
	frag_color = c;
	frag_orm = orm;
	frag_normal = vec4 (normal, 0);
	frag_emission = e;
}
