void printf (string fmt, ...) = #0;

int
test_fordecl ()
{
	int         fail = 1;
	int         count = 5;
	int         ti = -1, tj = -1;

	for (int i = 3, j = 5; count-- > 0; ) {
		i += 2;
		j += 3;
		ti = i;
		tj = j;
	}
	if (ti == 13 && tj == 20) {
		fail = 0;
	}

	return fail;
}

int
main ()
{
	int         fail = 0;
	fail |= test_fordecl ();
	return fail;
}
