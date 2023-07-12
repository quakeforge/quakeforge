vector bar (float *v);
vector snafu (float *v);

vector
foo (float x, float y, float z)
{
	float v[3] = { x, y, z };
	float w[3] = { x, y, z };
	snafu (v);
	return bar (w);
}

vector snafu (float *v)
{
	return nil;
}

vector
bar (float *v)
{
	return [v[0], v[1], v[2]];
}

int
main ()
{
	int ret = 0;
	ret |= foo(1,2,3) != [1, 2, 3];
	return ret;
}
