#define NIT_TESLA            32768  //The *real* bug zapper!
#define NIT_SECURITY_CAMERA  65536  //CH Security Camera
#define NIT_TELEPORTER       131072 //CH Teleporter
#define PC_ENGINEER_TF_ITEMS NIT_TESLA | NIT_SECURITY_CAMERA | NIT_TELEPORTER

void () eek;
float (float a, float b) boing;

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
	return 0;
};
