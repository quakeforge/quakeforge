#pragma bug die
@param foo (int r)
{
	@param ret;
	ret = nil;
	ret.int_val = r;
	return ret;
}

int main ()
{
	@param ret;
	int i;

	for (i = 0; i < 5; i++) {
		ret = foo (i);
		if (ret.int_val != i)
			return 1;
	}
	return 0;
}
