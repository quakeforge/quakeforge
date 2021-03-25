void printf (string fmt, ...) = #0;

float foo (vector v, float z)
{
	return v * *(vector*)(&v.y);
}

int
main (int argc, string *argv)
{
	vector v = [1, 2, 3];
	vector w = [2, 3, 4];
	float f;
	printf ("%v %g %g %g\n", v, v*v, v*w, f=foo (v, 4));
	return f != v*w;
}
