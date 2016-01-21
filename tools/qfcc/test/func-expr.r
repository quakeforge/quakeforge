int foo (int i)
{
	return i;
}

int func (int a, int b)
{
	local int x;

	x = foo (a) + foo (b);
	return x;
};

int
main ()
{
	return func (1,3) != 1 + 3;
}
