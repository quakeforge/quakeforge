void printf (string fmt, ...) = #0;

int
test_format ()
{
	int         fail = 0;
	quaternion  q = '1 2 3 4';
	vector      v = '-1 -2 -3';
	float       s = -4;

	if (q.x != 1 || q.y != 2 || q.z != 3 || q.w != 4) {
		printf ("q = '1 2 3 4' -> %q\n", q);
		fail = 1;
	}
	if (q.v != '1 2 3' || q.s != 4) {
		printf ("q = '1 2 3 4' -> %v, %g\n", q.v, q.s);
		fail = 1;
	}
	q = nil;
	if (q.x != 0 || q.y != 0 || q.z != 0 || q.w != 0) {
		printf ("q = nil -> %q\n", q);
		fail = 1;
	}
	if (q.v != '0 0 0' || q.s != 0) {
		printf ("q = nil -> %v, %g\n", q.v, q.s);
		fail = 1;
	}
	q = [1, [2, 3, 4]];
	if (q.x != 2 || q.y != 3 || q.z != 4 || q.w != 1) {
		printf ("q = [1, [2, 3, 4]] -> %q\n", q);
		fail = 1;
	}
	if (q.v != '2 3 4' || q.s != 1) {
		printf ("q = [1, [2, 3, 4]] -> %v, %g\n", q.v, q.s);
		fail = 1;
	}
	q = [[5, 6, 7], 8];
	if (q.x != 5 || q.y != 6 || q.z != 7 || q.w != 8) {
		printf ("q = [[5, 6, 7], 8] -> %q\n", q);
		fail = 1;
	}
	if (q.v != '5 6 7' || q.s != 8) {
		printf ("q = [[5, 6, 7], 8] -> %v, %g\n", q.v, q.s);
		fail = 1;
	}
	q = [s, v];
	if (q.x != v.x || q.y != v.y || q.z != v.z || q.w != s) {
		printf ("q = [s, v] -> %q (%v)\n", q, v);
		fail = 1;
	}
	if (q.v != v || q.s != s) {
		printf ("q = [s, v] -> %v, %g (%v)\n", q.v, q.s, v);
		fail = 1;
	}
	q = [v, s];
	if (q.x != v.x || q.y != v.y || q.z != v.z || q.w != s) {
		printf ("q = [v, s] -> %q (%v %s)\n", q, v, s);
		fail = 1;
	}
	if (q.v != v || q.s != s) {
		printf ("q = [v, s] -> %v, %g (%v %s)\n", q.v, q.s, v, s);
		fail = 1;
	}
	return fail;
}

int
main ()
{
	int         fail = 0;
	fail |= test_format ();
	return fail;
}
