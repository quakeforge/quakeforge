typedef @algebra(float(3,0,0)) VGA;
typedef VGA.group_mask(0x0c) quat_t;
typedef VGA.vec plane_t;

quat_t mirror_quat (quat_t q, plane_t m)
{
	return m * q * ~m;
}


plane_t rotate_mirror (quat_t q, plane_t m)
{
	return ~q * m * q;
}

void printf (string fmt, ...) = #0;

int main()
{
	quat_t q = {.bvec = (VGA.bvec)'0.5 0.5 0.5', .scalar = 0.5};
	plane_t m = (VGA.vec)'1 0 0';
	auto rotated_m = rotate_mirror (q, m);
	auto mirrored_q = mirror_quat (q, m);
	printf ("%v %q\n", rotated_m, mirrored_q);
	return rotated_m != (VGA.vec)'0 1 0' ? 1 : 0;
}
