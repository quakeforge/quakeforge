void printf (string fmt, ...) = #0;

#if __RUAMOKO__ > 1
#define dot @dot
#else
#define dot *
#endif

void forcelive (float z)
{
}

float foo (vector _v, float _z)
{
	vector v = _v;
	float  z = _z;
	_v = nil;
	_z = 0;
	forcelive (_z);
	forcelive (z);
	return v dot *(vector*)(&v.y);
}

int
main (int argc, string *argv)
{
	vector v = [1, 2, 3];
	vector w = [2, 3, 4];
	float f;
	printf ("%v %g %g %g\n", v, v dot v, v dot w, f=foo (v, 4));
	return f != v dot w;
}
