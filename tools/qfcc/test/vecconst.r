#pragma bug die
vector mins = '-16 -16 -24';
vector maxs = '16 16 32';
ivec4 i4 = '1 2 -3 4';
vec4 v4 = '1 2 -3 4';
lvec4 l4 = '1 2 -3 4';
uivec4 ui4 = '1 2 -3 4';
dvec4 d4 = '1 2 -3 4';
ulvec4 ul4 = '1 2 -3 4';

int
check_v (vector v, float x, float y, float z)
{
	return v.x != x || v.y != y || v.z != z;
}

int check_ivec4 (ivec4 v, int x, int y, int z, int w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int check_vec4 (vec4 v, float x, float y, float z, float w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int check_lvec4 (lvec4 v, long x, long y, long z, long w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int check_uivec4 (uivec4 v, unsigned x, unsigned y, unsigned z, unsigned w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int check_dvec4 (dvec4 v, double x, double y, double z, double w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int check_ulvec4 (ulvec4 v, unsigned long x, unsigned long y, unsigned long z,
				  unsigned long w)
{
	return v.x != x || v.y != y || v.z != z || v.w != w;
}

int
main ()
{
	int ret = 0;
	ret |= check_v (mins, -16, -16, -24);
	ret |= check_v (maxs, 16, 16, 32);
	ret |= check_ivec4 (i4, 1, 2, -3, 4);
	ret |= check_vec4 (v4, 1, 2, -3, 4);
	ret |= check_lvec4 (l4, 1, 2, -3, 4);
	ret |= check_uivec4 (ui4, 1, 2, -3, 4);
	ret |= check_dvec4 (d4, 1, 2, -3, 4);
	ret |= check_ulvec4 (ul4, 1, 2, -3, 4);
	return ret;
}
