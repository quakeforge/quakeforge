vector
foo (float x, float y, float z)
{
	vector v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

float w;
float h;

vector
bar (void)
{
	vector      pos;

	pos.x = w;
	pos.y = h;
	pos.z = 0;
	return pos;
}

int
main ()
{
	return 0;
}
