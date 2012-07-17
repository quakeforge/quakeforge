uniform samplerCube sky;
uniform vec4 fog;

varying vec3 direction;

float
sqr (float x)
{
	return x * x;
}

vec4
fogBlend (vec4 color)
{
	float       f;
	vec4        fog_color = vec4 (fog.rgb, 1.0);

	f = exp (-sqr (fog.a * gl_FragCoord.z / gl_FragCoord.w));
	return vec4 (mix (fog_color.rgb, color.rgb, f), color.a);
}

void
main (void)
{
	vec3        dir = direction;

	dir *= inversesqrt (dot (dir, dir));

	// NOTE: quake's world and GL's world are rotated relative to each other
	// quake has x right, y in, z up. gl has x right, y up, z out
	// The textures are loaded with GL's z (quake's y) already negated, so
	// all that's needed here is to swizzle y and z.
	gl_FragColor = fogBlend (textureCube(sky, dir.xzy));
}
