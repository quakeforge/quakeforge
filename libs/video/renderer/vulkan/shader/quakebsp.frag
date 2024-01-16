#version 450

layout (set = 3, binding = 0) uniform sampler2DArray Texture;
layout (set = 4, binding = 0) uniform sampler2D Lightmap;

layout (push_constant) uniform PushConstants {
	vec4        fog;
	float       time;
	float       alpha;
};

layout (location = 0) in vec4 tl_st;
layout (location = 1) in vec3 direction;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 position;
layout (location = 4) in vec4 color;

layout (location = 0) out vec4 frag_color;

vec4
fogBlend (vec4 color)
{
	float       az = fog.a * gl_FragCoord.z / gl_FragCoord.w;
	vec3        fog_color = fog.rgb;
	float       fog_factor = exp (-az * az);

	return vec4 (mix (fog_color.rgb, color.rgb, fog_factor), color.a);
}

void
main (void)
{
	vec3        t_st = vec3 (tl_st.xy, 0);
	vec3        e_st = vec3 (tl_st.xy, 1);
	vec2        l_st = vec2 (tl_st.zw);

	vec4        c = texture (Texture, t_st) * color;
	vec4        e = texture (Texture, e_st);
	vec4        l = texture (Lightmap, l_st);

	frag_color = c * l + e;//fogBlend (c);
}
