vector
t1()
{
	return [1, 2, 3];
}

vector
t2(float x)
{
	return [x, x, x];
}

vector
t3(float x)
{
	return [x, t2(9).z, x] * 2;
}

int
main ()
{
	return t3(5) == [10, 18, 10] ? 0 : 1;
}
