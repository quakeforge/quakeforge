#define NIT_TESLA            32768  //The *real* bug zapper!
#define NIT_SECURITY_CAMERA  65536  //CH Security Camera
#define NIT_TELEPORTER       131072 //CH Teleporter
#define PC_ENGINEER_TF_ITEMS NIT_TESLA | NIT_SECURITY_CAMERA | NIT_TELEPORTER

struct foo_struct = {
	integer result;
	string buffer;
};

void (foo_struct bar) eek;
float (float a, float b) boing;

string [6] dot = {
	"gfx/menudot0.lpm",
	"gfx/menudot1.lpm",
	"gfx/menudot2.lpm",
	"gfx/menudot3.lpm",
	"gfx/menudot4.lpm",
	"gfx/menudot5.lpm",
};

void (string fmt, ...) test_argv =
{
traceon ();
	local vector v;
	local integer i;
	local string f = fmt;

	for (i = 0; i < @argc; i++)
		v = @argv[i];
traceoff ();
};

float () main =
{
	local float messed_or;
	local integer handle;
	local string buffer;

	test_argv ("hi there", 1, "123", '1', '1 2 3');
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
	for (handle = 0; handle < 6; handle++)
		print (dot[handle] + "\n");

	local id ffoo = [[Foo alloc] init];
	[ffoo run];
	return 0;
};
