int iter_check (unsigned count)
{
	unsigned i = count;
	int iters = 0;
	while (i-- > 0) {
		iters++;
	}
	return (unsigned) iters == count;
}

int
main ()
{
	int         ret = 0;
	ret |= !iter_check (1);
	return ret;
}
