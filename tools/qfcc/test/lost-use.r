void printf (string fmt, ...) = #0;
int getval(void)
{
	return 42;
}

void magic (void)
{
}

void storeval (int *p)
{
	int x = getval ();
	magic ();
	*p = x;
}

int val;

int
main(void)
{
	storeval (&val);
	if (val != 42) {
		printf ("val is dead: %d\n", val);
		return 1;
	}
	return 0;
}
