#define NIT_TESLA            32768  //The *real* bug zapper!
#define NIT_SECURITY_CAMERA  65536  //CH Security Camera
#define NIT_TELEPORTER       131072 //CH Teleporter
#define PC_ENGINEER_TF_ITEMS NIT_TESLA | NIT_SECURITY_CAMERA | NIT_TELEPORTER

struct foo = {
	integer result;
	string buffer;
};

void (foo bar) eek;
float (float a, float b) boing;

string [6] dot = {
	"gfx/menudot0.lpm",
	"gfx/menudot1.lpm",
	"gfx/menudot2.lpm",
	"gfx/menudot3.lpm",
	"gfx/menudot4.lpm",
	"gfx/menudot5.lpm",
};

float () main =
{
	local float messed_or;
	local integer handle;
	local string buffer;

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
	return 0;
};
