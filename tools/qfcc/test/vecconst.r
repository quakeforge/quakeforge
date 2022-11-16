vector mins = '-16 -16 -24';
vector maxs = '16 16 32';

int
check (vector v, float x, float y, float z)
{
	return v.x != x || v.y != y || v.z != z;
}

int
main ()
{
	int ret = 0;
	ret |= check (mins, -16, -16, -24);
	ret |= check (maxs, 16, 16, 32);
	return ret;
}
