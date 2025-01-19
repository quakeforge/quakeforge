float project (vector a, vector b, @out vector p)
{
	float d = a @dot b / b @dot b;
	p = a - d * b;
	return d;
}

void printf (string fmt, ...) = #0;

float
main ()
{
	vector a = '1 3 2';
	vector b = '4 0 3';
	vector c;
	float d = project (a, b, c) != 0.4;
	printf ("%.9v\n", c);
	if (c != '-0.6 3 0.799999952')
		d = 1;
	return d;
}
