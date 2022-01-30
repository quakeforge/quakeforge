void printf (string fmt, ...) = #0;

#if __RUAMOKO__ > 1
#define dot @dot
#define X .y
#else
#define dot *
#define X
#endif

void forcelive (float z)
{
}

float foo (vector _v, float _z)
{
	vector v = _v;
	float  z = _z;
	_v = nil;
	_z = _z - _z;
	forcelive (z);
	return (v dot *(vector*)(&v.y))X;
}

int
main (int argc, string *argv)
{
	vector v = [1, 2, 3];
	vector w = [2, 3, 4];
	float f;
	printf ("%v %g %g %g\n", v, v dot v, v dot w, f=foo (v, 4));
	return f != (v dot w)X;
}
