void printf (string fmt, ...) = #0;
# define M_PI 3.14159265358979323846

union {
	double d;
	int    i[2];
} type_pun;

int
test_format ()
{
	int         fail = 0;
	type_pun.d = M_PI;
	printf ("%g %08x%08x\n", type_pun.d, type_pun.i[1], type_pun.i[0]);
	//printf ("%08x%08x\n", type_pun.i[1], type_pun.i[0]);
	return fail;
}

int
main ()
{
	int         fail = 0;
	fail |= test_format ();
	return fail;
}
