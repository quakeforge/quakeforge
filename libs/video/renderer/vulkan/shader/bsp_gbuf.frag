#version 450

layout (constant_id = 0) const int MaxTextures = 256;

layout (set = 1, binding = 0) uniform sampler samp;
layout (set = 1, binding = 1) uniform texture2DArray textures[MaxTextures];

layout (push_constant) uniform PushConstants {
	layout (offset = 64)
	vec4        fog;
	float       time;
	int         texind;
};

layout (location = 0) in vec4 tl_st;
layout (location = 1) in vec3 direction;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 position;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_emission;
layout (location = 2) out vec4 frag_normal;
layout (location = 3) out vec4 frag_position;

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
	vec4        c = vec4 (0);
	vec4        e;
	vec3        t_st = vec3 (tl_st.xy, 0);
	vec3        l_st = vec3 (tl_st.zw, 1);

	c = texture (sampler2DArray (textures[texind], samp), t_st);
	e = texture (sampler2DArray (textures[texind], samp), t_st);
	frag_color = c;//fogBlend (c);
	frag_emission = e;
	frag_normal = vec4 (normal, 0);
	frag_position = position;
}
