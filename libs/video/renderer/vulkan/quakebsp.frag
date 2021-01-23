#version 450

layout (set = 0, binding = 1) uniform sampler2D Texture;
layout (set = 0, binding = 2) uniform sampler2D GlowMap;
layout (set = 0, binding = 3) uniform sampler2D LightMap;
//layout (set = 0, binding = 4) uniform sampler2DArray SkySheet;
//layout (set = 0, binding = 5) uniform samplerCube SkyCube;

layout (push_constant) uniform PushConstants {
	layout (offset = 64)
	float       time;
	vec4        fog;
};

layout (location = 0) in vec4 tl_st;
layout (location = 1) in vec3 direction;

layout (location = 0) out vec4 frag_color;

layout (constant_id = 0) const bool doWarp = false;
layout (constant_id = 1) const bool doLight = true;
layout (constant_id = 2) const bool doSkyCube = false;
layout (constant_id = 3) const bool doSkySheet = false;

const float PI = 3.14159265;
const float SPEED = 20.0;
const float CYCLE = 128.0;
const float FACTOR = PI * 2.0 / CYCLE;
const vec2 BIAS = vec2 (1.0, 1.0);
const float SCALE = 8.0;

vec2
warp_st (vec2 st, float time)
{
	vec2        angle = st.ts * CYCLE / 2.0;
	vec2        phase = vec2 (time, time) * SPEED;
	return st + (sin ((angle + phase) * FACTOR) + BIAS) / SCALE;
}

vec4
fogBlend (vec4 color)
{
	float       az = fog.a * gl_FragCoord.z / gl_FragCoord.w;
	vec3        fog_color = fog.rgb;
	float       fog_factor = exp (-az * az);

	return vec4 (mix (fog_color.rgb, color.rgb, fog_factor), color.a);
}
/*
vec4
sky_sheet (vec3 dir, float time)
{
	float       len;
	vec2        flow = vec2 (1.0, 1.0);
	vec2        base;
	vec3        st1, st2;
	vec4        c1, c2, c;

	dir.z *= 3.0;
	len = dot (dir, dir);
	len = SCALE * inversesqrt (len);
	base = dir.yx * vec2(1.0, -1.0) * len;

	st1 = vec3 (base + flow * time / 8.0, 0);
	st2 = vec3 (base + flow * time / 16.0, 1);

	c1 = texture (SkySheet, st1);
	c2 = texture (SkySheet, st2);

	c = vec4 (mix (c2.rgb, c1.rgb, c1.a), max (c1.a, c2.a));

	return c;
}

vec4
sky_cube (vec3 dir, float time)
{
	// NOTE: quake's world is right-handed with Z up and X forward, but
	// Vulkan's cube maps are left-handed with Y up and Z forward. The
	// rotation to X foward is done by the Sky matrix so all that's left
	// to do here is swizzle the Y and Z coordinates
	return texture (SkyCube, dir.xzy);
}

vec4
sky_color (vec3 dir, float time)
{
	if (!doSkySheet) {
		return vec4 (1, 0, 1, 1);
		//return sky_cube (dir, time);
	} if (!doSkyCube) {
		return sky_sheet (dir, time);
	} else {
		// can see through the sheet (may look funny when looking down)
		// maybe have 4 sheet layers instead of 2?
		vec4        c1 = sky_sheet (dir, time);
		vec4        c2 = sky_cube (dir, time);
		return vec4 (mix (c2.rgb, c1.rgb, c1.a), max (c1.a, c2.a));
		return vec4 (1, 0, 1, 1);
	}
}
*/
void
main (void)
{
	vec4        c;
	vec2        t_st = tl_st.xy;
	vec2        l_st = tl_st.zw;

	if (doWarp) {
		t_st = warp_st (t_st, time);
	}
	/*if (doSkyCube || doSkySheet) {
		c = sky_color (direction, time);
	} else {
		c = texture (Texture, t_st);
	}*/
	c = texture (Texture, t_st);
	if (doLight) {
		c *= vec4 (texture (LightMap, l_st).xyz, 1);
	}
	//c += texture (GlowMap, t_st);
	//c = vec4(texture (LightMap, l_st).xyz, 1);
	//c = vec4(texture (Texture, t_st).xyz, 1);
	frag_color = c;//fogBlend (c);
}
