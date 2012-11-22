void printf (string fmt, ...) = #0;

quaternion q;
vector v;

int
main ()
{
	quaternion tq;
	q = '0.6 0.48 0.64 0';
	v = '1 2 3';
	tq.s = 0;
	tq.v = v;
	printf ("%v %q\n", q * v, q * tq * ~q);
	return 0;
}
