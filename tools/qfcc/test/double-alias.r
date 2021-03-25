void printf (string fmt, ...) = #0;
# define M_PI 3.14159265358979323846

union {
	double d;
	int    i[2];
} type_pun;

int alias_printf (string fmt, ...);

int
test_alias ()
{
	int         fail = 0;
	type_pun.d = M_PI;
	fail = alias_printf ("%g %08x%08x\n", type_pun.d,
						 type_pun.i[1], type_pun.i[0]);
	return fail;
}

int
alias_printf (string fmt, ...)
{
	int         fail = 0;
	// this will fail on big-endian systems
	fail = (@args.list[2].integer_val != 0x54442d18
			|| @args.list[1].integer_val != 0x400921fb);
	printf ("%g %08x%08x\n",
			@args.list[0].integer_val,
			@args.list[2].integer_val,
			@args.list[1].integer_val);
	return fail;
}

int
main ()
{
	int         fail = 0;
	fail |= test_alias ();
	return fail;
}
