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

[in(0)] vec4 tl_st;
[in(1)] vec3 direction;
[in(2)] vec3 normal;
[in(4)] vec4 color;

[out(0)] vec4 frag_color;
[out(1)] vec4 frag_orm;
[out(2)] vec4 frag_normal;
[out(3)] vec4 frag_emission;

[in("FragCoord")] vec4 gl_FragCoord;

vec4
fogBlend (vec4 color)
{
	float       az = fog.a * gl_FragCoord.z / gl_FragCoord.w;
	vec3        fog_color = fog.rgb;
	float       fog_factor = exp (-az * az);

	return vec4 (mix (fog_color.rgb, color.rgb, fog_factor), color.a);
}

[shader(Fragment)]
void
main (void)
{
	vec4        c = vec4 (0);
	vec4        e;
	vec3        t_st = vec3 (tl_st.xy, 0);
	vec3        e_st = vec3 (tl_st.xy, 1);
	vec2        l_st = vec2 (tl_st.zw);

	c = texture (Texture, t_st) * color;
	e = texture (Texture, e_st) * 2;//FIXME
	frag_color = c;//fogBlend (c);
	frag_orm = orm;
	frag_normal = vec4 (normal, 0);
	frag_emission = e;
}
