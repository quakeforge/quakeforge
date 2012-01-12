uniform samplerCube sky;

varying vec3 direction;

void
main (void)
{
	vec3        dir = direction;

	dir *= inversesqrt (dot (dir, dir));

	// NOTE: quake's world and GL's world are rotated relative to each other
	// quake has x right, y in, z up. gl has x right, y up, z out
	// The textures are loaded with GL's z (quake's y) already negated, so
	// all that's needed here is to swizzle y and z.
	gl_FragColor =  textureCube(sky, dir.xzy);
}
