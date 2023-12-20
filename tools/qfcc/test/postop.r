int array[3] = { -1, -1, -1 };

int array_index (void)
{
	int         i = 0;

	for (int c = 0; c < 3; c++) {
		array[i++] = 0;
	}
	return i == 3 && !(array[0] | array[1] | array[2]);
}

int iter_check (int count)
{
	int i = count;
	int iters = 0;
	while (i-- > 0) {
		iters++;
	}
	return iters == count;
}

int
main ()
{
	int         ret = 0;
	ret |= !array_index ();
	ret |= !iter_check (1);
	return ret;
}
