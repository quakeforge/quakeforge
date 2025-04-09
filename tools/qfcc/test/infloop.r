#pragma bug die
void exit (int code) = #0;

int foo;

int calc_foo (void)
{
	return 1;
}

void check_foo (void)
{
	if (foo)
		exit (0);
	exit (1);
}

int main ()
{
	while (1) {
		foo = calc_foo ();
		check_foo ();
	}
}
