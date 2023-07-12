@extern integer bar (void);
@extern integer baz (void);
id obj;

integer foo (void)
{
	[obj message];
	return bar () + baz ();
}
