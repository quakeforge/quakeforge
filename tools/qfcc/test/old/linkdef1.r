@extern integer foo (void);
@extern integer baz (void);

integer bar (void)
{
	return foo () + baz ();
}
