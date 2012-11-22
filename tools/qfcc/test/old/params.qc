void printf (string, ...) = #0;
integer main (integer argc, string *argv)
{
    integer i;

	for (i = 0; i < argc; i++)
		printf ("%s\n", argv[i]);
	return 0;
}
