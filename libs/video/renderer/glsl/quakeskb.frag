uniform samplerCube sky;

varying vec3 direction;

void
main (void)
{
	vec3        dir = direction;

	dir *= inversesqrt (dot (dir, dir));
	gl_FragColor =  textureCube(sky, dir);
}
