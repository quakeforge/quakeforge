integer (integer argc, string []argv) main =
{
	local integer i;
	for (i = 0; i < argc; i++) {
		print (argv[i]);
		print ("\n");
	}
	local id foo = [[Foo alloc] init];
	[foo run];
	return 0;
};
