#pragma bug die
@param
foo (string str)
{
	@param ret;
	ret = nil;
	ret.string_val = str;
	return ret;
}

string
bar (string str)
{
	@param params[8];
	@va_list va_list = { 0, params };
	int j;

	for (j = 0; j < 1; j++)
		va_list.list[j] = foo (str);
	return va_list.list[0].string_val;
}

int
main ()
{
	return bar ("snafu") != "snafu";
}
