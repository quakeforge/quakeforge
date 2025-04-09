#pragma bug die
vector
bar (float *v)
{
	return [v[0], v[1], v[2]];
}

vector
foo (float x, float y, float z)
{
	float v[3] = { x, y, z };
	return bar (&v[0]);
}

int
main ()
{
	int ret = 0;
	ret |= foo(1,2,3) != [1, 2, 3];
	return ret;
}
