float foo = 1;
float bar = 1;
float snafu = 2;

float () main =
{
	local float handle;
	local string buffer;

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

float () baz =
{
	return foo + bar;
};

void (float x) test =
{
	local string str;
	local float urk = 0;

	if (urk) {
		urk = foo || snafu;
	} else {
		print ("owie");
	}
};
