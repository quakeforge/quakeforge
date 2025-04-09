#pragma bug die
#if !defined(__RUAMOKO__) || __RUAMOKO__ < 2
#  define test_traditional
#endif

void printf (string ftm, ...) = #0;

float snafu (float a, float b)
{
	float c = a % b;
	return c;
}

int imodulo (int a, int b)
{
	return a %% b;
}

float fmodulo (float a, float b)
{
	return a %% b;
}

double dmodulo (double a, double b)
{
	return a %% b;
}

#ifdef test_traditional
#pragma traditional
float foo (float a, float b)
{
	float c = a % b;
	return c;
}
#pragma advanced

float bar (float a, float b)
{
	float c;
	b &= b;
	a &= a;
	c = a - ((a / b) & -1) * b;
	return c;
}

#pragma traditional
float baz (float a, float b)
{
	float c = (a + b) % (a - b);
	return c;
}
#pragma advanced
#endif

@overload int
test (string name, string op, int (func)(int a, int b), int a, int b, int c)
{
	int ret;

	ret = func (a, b);
	if (ret != c) {
#ifdef test_traditional
		if (func == baz)
			printf ("%s: (%d + %d) %% (%d - %d): %d != %d\n",
					name, a, b, a, b, ret, c);
		else
#endif
			printf ("%s: %d %s %d: %d != %d\n",
					name, a, op, b, ret, c);
		return 1;
	}
	return 0;
}

@overload int
test (string name, string op, float (func)(float a, float b),
	  float a, float b, float c)
{
	float ret;

	ret = func (a, b);
	if (ret != c) {
#ifdef test_traditional
		if (func == baz)
			printf ("%s: (%g + %g) %% (%g - %g): %g != %g\n",
					name, a, b, a, b, ret, c);
		else
#endif
			printf ("%s: %g %s %g: %g != %g\n",
					name, a, op, b, ret, c);
		return 1;
	}
	return 0;
}

@overload int
test (string name, string op, double (func)(double a, double b),
	  double a, double b, double c)
{
	double ret;

	ret = func (a, b);
	if (ret != c) {
#ifdef test_traditional
		if (func == baz)
			printf ("%s: (%g + %g) %% (%g - %g): %g != %g\n",
					name, a, b, a, b, ret, c);
		else
#endif
			printf ("%s: %g %s %g: %g != %g\n",
					name, a, op, b, ret, c);
		return 1;
	}
	return 0;
}
#ifdef test_traditional
typedef float restype;
#else
typedef int restype;
#endif

restype main (void)
{
	restype res = 0;
#ifdef test_traditional
	res |= test ("foo", "%", foo, 5, 3, 2);
	res |= test ("bar", "%", bar, 5, 3, 2);
	res |= test ("baz", "%", baz, 5, 3, 0);
#endif
	res |= test ("snafu", "%", snafu, 5, 3, 2);
#ifdef test_traditional
	res |= test ("foo", "%", foo, -5, 3, -2);
	res |= test ("bar", "%", bar, -5, 3, -2);
	res |= test ("baz", "%", baz, -5, 3, -2);
#endif
	res |= test ("snafu", "%", snafu, -5, 3, -2);
#ifdef test_traditional
	res |= test ("foo", "%", foo, 5, -3, 2);
	res |= test ("bar", "%", bar, 5, -3, 2);
	res |= test ("baz", "%", baz, 5, -3, 2);
#endif
	res |= test ("snafu", "%", snafu, 5, -3, 2);
#ifdef test_traditional
	res |= test ("foo", "%", foo, -5, -3, -2);
	res |= test ("bar", "%", bar, -5, -3, -2);
	res |= test ("baz", "%", baz, -5, -3, 0);
#endif
	res |= test ("snafu", "%", snafu, -5, -3, -2);
#ifdef test_traditional
	res |= test ("foo", "%", foo, 5, 3.5, 1.5);
	res |= test ("foo", "%", foo, -5, 3.5, -1.5);
#endif
	res |= test ("snafu", "%", snafu, 5, 3.5, 1.5);
	res |= test ("snafu", "%", snafu, -5, 3.5, -1.5);

	res |= test ("int modulo", "%%", imodulo, 5, 3, 2);
	res |= test ("int modulo", "%%", imodulo, -5, 3, 1);
	res |= test ("int modulo", "%%", imodulo, 5, -3, -1);
	res |= test ("int modulo", "%%", imodulo, -5, -3, -2);
	res |= test ("int modulo", "%%", imodulo, 6, 3, 0);
	res |= test ("int modulo", "%%", imodulo, -6, 3, 0);
	res |= test ("int modulo", "%%", imodulo, 6, -3, 0);
	res |= test ("int modulo", "%%", imodulo, -6, -3, 0);

	res |= test ("float modulo", "%%", fmodulo, 5, 3, 2);
	res |= test ("float modulo", "%%", fmodulo, -5, 3, 1);
	res |= test ("float modulo", "%%", fmodulo, 5, -3, -1);
	res |= test ("float modulo", "%%", fmodulo, -5, -3, -2);
	res |= test ("float modulo", "%%", fmodulo, 6, 3, 0);
	res |= test ("float modulo", "%%", fmodulo, -6, 3, 0);
	res |= test ("float modulo", "%%", fmodulo, 6, -3, 0);
	res |= test ("float modulo", "%%", fmodulo, -6, -3, 0);

	res |= test ("double modulo", "%%", dmodulo, 5, 3, 2);
	res |= test ("double modulo", "%%", dmodulo, -5, 3, 1);
	res |= test ("double modulo", "%%", dmodulo, 5, -3, -1);
	res |= test ("double modulo", "%%", dmodulo, -5, -3, -2);
	res |= test ("double modulo", "%%", dmodulo, 6, 3, 0);
	res |= test ("double modulo", "%%", dmodulo, -6, 3, 0);
	res |= test ("double modulo", "%%", dmodulo, 6, -3, 0);
	res |= test ("double modulo", "%%", dmodulo, -6, -3, 0);
	return res;
}
