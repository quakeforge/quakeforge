#pragma bug die
void printf (string fmt, ...) = #0;
# define M_PI 3.14159265358979323846

union {
	double d;
	int    i[2];
} type_pun;

bool
test_format ()
{
	bool        fail;
	type_pun.d = M_PI;
	printf ("%.17g %08x%08x\n", type_pun.d, type_pun.i[1], type_pun.i[0]);
	// this will fail on big-endian systems
	fail = type_pun.i[0] != 0x54442d18 || type_pun.i[1] != 0x400921fb;
	return fail;
}

lbool
test_constant ()
{
	lbool       fail = false;
	double      a, b, c, d, e;
	a = 1;
	b = 2.0;
	c = 3.2f;
	d = 3.2d;
	e = 3.2;
	printf ("%.17g %.17g %.17g %.17g %.17g\n", a, b, c, d, e);
	// this will fail on big-endian systems
	fail |= c == d; // 3.2 is not exactly representable, so must be different
	fail |= c == e; // 3.2 is not exactly representable, so must be different
	fail |= d != e;	// 3.2d and 3.2 are both double, so must be the same
	return fail;
}

double less = 3;
double greater_equal = 3;
double less_equal = 5;
double greater = 5;

long
test_copare ()
{
	lbool fail = 0;

	fail |= !(less < greater);
	fail |= (less > greater);
	fail |= !(less != greater);
	fail |= (less == greater);
	fail |= !(less <= greater);
	fail |= (less >= greater);

	fail |= (less_equal < greater);
	fail |= (less_equal > greater);
	fail |= !(less_equal == greater);
	fail |= (less_equal != greater);
	fail |= !(less_equal <= greater);
	fail |= !(less_equal >= greater);

	fail |= (greater < less);
	fail |= !(greater > less);
	fail |= !(greater != less);
	fail |= (greater == less);
	fail |= (greater <= less);
	fail |= !(greater >= less);

	fail |= (greater_equal < less);
	fail |= (greater_equal > less);
	fail |= !(greater_equal == less);
	fail |= (greater_equal != less);
	fail |= !(greater_equal <= less);
	fail |= !(greater_equal >= less);
	return fail;
}

lbool
test_ops ()
{
	lbool       fail = 0;
	double      a = 6.25, b = 2.375;
	double      c;

	c = a + b;
	fail |= c != 8.625;
	c = a - b;
	fail |= c != 3.875;
	c = a * b;
	fail |= c != 14.84375;
	c = a / b;
	fail |= c != 50d/19d;
	c = a % b;
	fail |= c != 1.5;
	return fail;
}

int
main ()
{
	lbool       fail = false;
	fail |= (lbool) test_format ();
	fail |= test_constant ();
	fail |= test_ops ();
	return fail;
}
