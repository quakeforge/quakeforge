#define NIT_TESLA            32768  //The *real* bug zapper!
#define NIT_SECURITY_CAMERA  65536  //CH Security Camera
#define NIT_TELEPORTER       131072 //CH Teleporter
#define PC_ENGINEER_TF_ITEMS NIT_TESLA | NIT_SECURITY_CAMERA | NIT_TELEPORTER

//float messed_or = PC_ENGINEER_TF_ITEMS;
float foo = 1;
float bar = 1;
float snafu = 2;
float negative = -------2;

void () eek;

float () main =
{
	local float messed_or;
	local integer handle;
	local string buffer;

	traceon();
	messed_or = PC_ENGINEER_TF_ITEMS;
	handle = open ("main.qc", 0);
	if (handle == -1) {
		print (strerror (errno ()) + "\n");
		return 1;
	}
	do {
		buffer = read (handle, 1024);
		if (read_result == -1) {
			print (strerror (errno ()) + "\n");
			return 1;
		}
		print (buffer);
	} while (read_result == 1024);
	close (handle);
	eek ();
	return 0;
};

float () baz =
{
	return foo + bar;
};

void (float x) test =
{
	local string str;
	local integer urk = 0;

	if (urk) {
		urk = foo || snafu;
	} else {
		print ("owie");
	}
};

void () blarg =
{
	local float foo = -1;
	local float bar;
	local entity te;
	while (te) {
		if (bar)
			bar = 0;
	}
	do {
		foo = 1;
	} while (foo);
	if (bar = foo);
	foo = 2;
	while (foo) {
		bar = foo;
	}
};

float (float baz) test_int =
{
	return 1;
};

.entity owner;
entity sent;
void () eek =
{
	local string bop = "";
	local vector v = '1 2 3';

	traceon();
	if (sent && sent.owner != self )
		self.origin = '0 0 0';
	self.origin = self.origin + '1 2 3';
	test_int (1);
	if (bop)
		traceoff();
	printf ("%v\n", v);
	v = 2 * v;
	printf ("%v\n", v);
	v = v * 2;
	printf ("%v\n", v);
	v = -v;
	printf ("%v\n", v);
};

void () assign =
{
	local float foo;
	local float bar;

	foo = bar;
	foo = bar = 1;
	foo = bar = bar + 1;
	foo = self.nextthink = self.nextthink + 1.0;
	self.frame = self.nextthink = self.nextthink + 1.0;
	foo = self.nextthink = foo + 1.0;
	self.nextthink = foo = foo + 1.0;
	self.frame = self.nextthink = foo + 1.0;
	foo = self.nextthink = foo;
	self.frame = self.nextthink = foo;
};
