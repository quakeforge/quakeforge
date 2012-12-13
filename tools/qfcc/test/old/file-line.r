void printf (string fmt, ...) = #0;

@overload void test (integer x)
{
	printf ("\"%s\" %s %d %s\n",
			__PRETTY_FUNCTION__, __FUNCTION__, __LINE__, __FILE__);
}

@overload void test (void)
{
	printf ("\"%s\" %s %d %s\n",
			__PRETTY_FUNCTION__, __FUNCTION__, __LINE__, __FILE__);
}

void foo (void)
{
	printf ("\"%s\" %s %d %s\n",
			__PRETTY_FUNCTION__, __FUNCTION__, __LINE__, __FILE__);
}

integer main ()
{
	test (1);
	test ();
	foo ();
	return 0;
}
