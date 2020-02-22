string read_file (string filename)
{
	local QFile file;
	local string data = nil, d;
	file = Qopen (filename, "rtz");
	if (!file) {
		printf ("Can't open %s for reading."
				" Probably a bad hardcoded filename:)\n", filename);
		return nil;
	}
	while ((d = Qgetline (file)))
		data = data + d;
	//FIXME can't read to a string, can't convert a pointer to a string ...
	//Qread (file, data, 1023);
	Qclose (file);
	d = str_new ();
	str_copy (d, data);
	return d;
}

void test_plist (void)
{
	local string data, l;
	local plitem_t pl, item, i;

	data = read_file ("/home/bill/.quakeforge/gamedir.conf");
	pl = PL_GetPropertyList ("{" + data + "}");
	l = PL_WritePropertyList (pl);
	printf ("%s", data);
	printf ("%s", l);
	i = PL_ObjectForKey (pl, "QF");
	item = PL_RemoveObjectForKey (pl, "QF");
	l = PL_WritePropertyList (item);
	printf ("%s", l);
	PL_Free (item);
	//Because i and item both point to the same plitem, and item has been,
	//freed, freeing i will generate an error
	//l = PL_WritePropertyList (i);
	PL_Free (pl);
	str_free (data);
}

void test_script (void)
{
	local script_t script;
	local string token;
	local string data;

	data = read_file ("progs.src");
	script = Script_New ();
	token = Script_Start (script, "progs.src", data);
	while (Script_GetToken (script, 1)) {
		printf ("%s\n", token);
	}
	Script_Delete (script);
	str_free (data);
}

void () test_str =
{
	local string a,b,c,d;
	a = "testing ";
	b = "temp ";
	c = "strings ";
	d = "\n";
	print (a + b + c + d);
	printf ("%i \"%.5s\" %3.4f %v\n", 14, "hi there", 3.1415926, '4 1 3');
};
void (...) dprint = #0;
int main (int argc, string *argv)
{
	local int i;
	local SEL sel;
	dprint ("foo", "bar\n");
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
	test_script ();
	test_plist ();
	test_types ();
	test_xdefs ();
	return 0;
};
