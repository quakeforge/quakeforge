float
gety(vector v, vector z)
{
	return (v + z).y;
}

int
main ()
{
	vector a = [1, 2, 3];
	vector b = [1, 2, 6];
	return gety (a, b) != 4;
}
