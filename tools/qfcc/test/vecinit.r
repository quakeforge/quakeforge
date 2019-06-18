vector
foo (float x, float y, float z)
{
	vector v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

float w = 2;
float h = 4;

vector
bar (void)
{
	vector      pos;

	pos.x = w;
	pos.y = h;
	pos.z = 0;
	return pos;
}

vector
baz (float w, float h)
{
	vector p = [w, h, 0] / 2;
	return p;
}

int
main ()
{
	int ret = 0;
	ret |= foo(1,2,3) != [1, 2, 3];
	ret |= bar() != [2, 4, 0];
	ret |= baz(5, 6) != [2.5, 3, 0];
	return ret;
}
