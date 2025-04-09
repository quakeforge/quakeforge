#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "fog.finc"

#include "oit_store.finc"

layout (constant_id = 0) const bool doSkyBox = false;
layout (constant_id = 1) const bool doSkySheet = false;

layout (set = 3, binding = 0) uniform sampler2DArray SkySheet;
layout (set = 4, binding = 0) uniform samplerCube SkyBox;

layout (push_constant) uniform PushConstants {
	vec4        fog;
	float       time;
	float       alpha;
	float       turb_scale;
};

layout (location = 0) in vec4 tl_st;
layout (location = 1) in vec3 direction;
layout (location = 2) in vec4 color;

layout(early_fragment_tests) in;
//layout (location = 0) out vec4 frag_color;

const float SCALE = 189.0 / 64.0;

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
sky_box (vec3 dir, float time)
{
	// NOTE: quake's world is right-handed with Z up and X forward, but
	// Vulkan's cube maps are left-handed with Y up and Z forward. The
	// rotation to X foward is done by the Sky matrix so all that's left
	// to do here is swizzle the Y and Z coordinates
	dir = normalize(dir);
	//return vec4(dir.xyz, 1) * 0.5 + vec4(0.5);
	return texture (SkyBox, dir.xzy);
}

vec4
sky_color (vec3 dir, float time)
{
	if (!doSkySheet) {
		return sky_box (dir, time);
	} if (!doSkyBox) {
		return sky_sheet (dir, time);
	} else {
		// can see through the sheet (may look funny when looking down)
		// maybe have 4 sheet layers instead of 2?
		vec4        c1 = sky_sheet (dir, time);
		vec4        c2 = sky_box (dir, time);
		return vec4 (mix (c2.rgb, c1.rgb, c1.a), max (c1.a, c2.a));
		//return vec4 (1, 0, 1, 1);
	}
}

void
main (void)
{
	vec4        c;

	if (doSkyBox || doSkySheet) {
		c = sky_color (direction, time);
	} else {
		c = vec4 (0, 0, 0, 1);
	}
	c = FogBlend (c, fog);
	StoreFrag (c, gl_FragCoord.z);
}
