void () test_str =
{
	local string a,b,c,d;
	a = "testing ";
	b = "temp ";
	c = "strings ";
	d = "\n";
	print (a + b + c + d);
};

integer (integer argc, string []argv) main =
{
	local integer i;
	local SEL sel;
	for (i = 0; i < argc; i++) {
		print (argv[i]);
		print ("\n");
	}
	local id foo = [[Foo alloc] init];
	[foo run];
	sel = sel_get_uid ("run");
	if (sel) {
		print ("found selector for `run'\n");
		if ([foo respondsToSelector:sel])
			print ("foo responds to `run'\n");
		else
			print ("foo does not repond to `run'\n");
	} else
		print ("did not find selector for `run'\n");
	sel = sel_get_uid ("alloc");
	if (sel) {
		print ("found selector for `alloc'\n");
		if ([Object instancesRespondToSelector:sel])
			print ("Object instances respond to `alloc'\n");
		else
			print ("Object instances do not repond to `alloc'\n");
	} else
		print ("did not find selector for `alloc'\n");
	sel = sel_get_uid ("run:with:me:");
	if (sel) {
		print ("found selector for `run:with:me:'\n");
		if ([Object instancesRespondToSelector:sel])
			print ("Object instances respond to `run:with:me:'\n");
		else
			print ("Object instances do not repond to `run:with:me:'\n");
	} else
		print ("did not find selector for `run:with:me:'\n");
	test_str ();
	return 0;
};
