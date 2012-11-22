void foo (vector x)
{
	entity *ep;
	int d;

	//d = (int) x.y;
	//ep = (entity *) d;
	ep = (entity *) (int) x.y;
}
