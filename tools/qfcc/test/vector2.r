vector a;
entity e;
.vector v;

float foo (void)
{
	a.z += 3;
	a_z += 3;
	e.v = a;
	e.v.y = 2;
	e.v_y = 2;
	return e.v.z;
}
