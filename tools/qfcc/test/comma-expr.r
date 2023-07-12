void printf (string fmt, ...) = #0;

int a = 0;
int b = 0;

int
return_comma()
{
	return a = 3, 5;
}

int
test_for_comma ()
{
	int         fail = 1;
	int         count = 5;
	int         i = -1;
	float       j = -1;

	for (i = 3, j = 5; count-- > 0; ) {
		i += 2;
		j += 3;
	}
	if (i == 13 && j == 20) {
		fail = 0;
	}

	return fail;
}

int
test_comma ()
{
	int fail = 1;
	if (return_comma() == 5) {
		if (a == 3) {
			fail = 0;
		}
	}
	return fail;
}

int
main ()
{
	int         fail = 0;
	fail |= test_comma ();
	fail |= test_for_comma ();
	return fail;
}
