void printf (string fmt, ...) = #0;
void traceon (void) = #0;

int x, y;

int
gcd (int a, int b)
{
	if (b == 0) return a;
	else return gcd (b, a % b);
}

int
main (int argc, string *argv)
{
	x = 130;
	y = 120;
	printf ("%d\n", gcd (x, y));
	return 0;
}
