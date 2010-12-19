#if 1
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
#endif
float baz (float a, float b)
{
	float c = (a + b) % (a - b);
	return c;
}
#if 1
float main (void)
{
	printf ("foo: 5 %% 3: %f\n", foo (5, 3));
	printf ("bar: 5 %% 3: %f\n", bar (5, 3));

	printf ("foo: -5 %% 3: %f\n", foo (-5, 3));
	printf ("bar: -5 %% 3: %f\n", bar (-5, 3));

	printf ("foo: 5 %% -3: %f\n", foo (5, -3));
	printf ("bar: 5 %% -3: %f\n", bar (5, -3));

	printf ("foo: -5 %% -3: %f\n", foo (-5, -3));
	printf ("bar: -5 %% -3: %f\n", bar (-5, -3));
	return 0;
}
#endif
