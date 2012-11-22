#if 0
#if 0
integer (void [] a, void [] b) cmp =
{
	return a == b;
};
#else
entity e;
.@va_list a;
.vector v;

integer (string fmt, @va_list args) vprintf =
{
	return args.count;
};

integer (string fmt, ...) printf =
{
	local @va_list s;
	local @va_list t;
	s = @args;
	//t = @args = s;
	s = e.a;
	e.a = s;
	e.a = s = @args;
	return vprintf (fmt, @args);
};
#endif

void () blah =
{
	printf ("", 0x1, 0x9, 0xA, 0xF, 0xa, 0xf, 0x12abCD);
};
#else
entity self;
.float keys;
float (float key) CL_KeyState =
{
	return (float)((self.keys & key) > 0);
};
#endif
