#pragma traditional

void (...) printf = #0;
float foo (float a, float b)
{
	float c = a % b;
	return c;
}
float bar (float a, float b)
{
	float c;
	b &= b;
	a &= a;
	c = a - ((a / b) & -1) * b;
	return c;
}

float baz (float a, float b)
{
	float c = (a + b) % (a - b);
	return c;
}

float test (string name, float (func)(float a, float b),
			float a, float b, float c)
{
	float ret;

	ret = func (a, b);
	if (ret != c) {
		if (func == baz)
			printf ("%s: (%g + %g) %% (%g - %g): %g != %g\n",
					name, a, b, a, b, ret, c);
		else
			printf ("%s: %g %% %g: %g != %g\n",
					name, a, b, ret, c);
		return 1;
	}
	return 0;
}

float main (void)
{
	float res = 0;
	res |= test ("foo", foo, 5, 3, 2);
	res |= test ("bar", bar, 5, 3, 2);
	res |= test ("baz", baz, 5, 3, 0);

	res |= test ("foo", foo, -5, 3, -2);
	res |= test ("bar", bar, -5, 3, -2);
	res |= test ("baz", baz, -5, 3, -2);

	res |= test ("foo", foo, 5, -3, 2);
	res |= test ("bar", bar, 5, -3, 2);
	res |= test ("baz", baz, 5, -3, 2);

	res |= test ("foo", foo, -5, -3, -2);
	res |= test ("bar", bar, -5, -3, -2);
	res |= test ("baz", baz, -5, -3, 0);

	res |= test ("foo", foo, 5, 3.5, 1.5);
	res |= test ("foo", foo, -5, 3.5, -1.5);
	return res;
}
