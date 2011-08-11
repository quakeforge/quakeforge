float VAL1 = 4;
float VAL2 = 6;
vector VMIN = '-16 -16 -24';
vector VMAX = '16 16 32';
vector v;

void send (float to, ...) = #0;

void do_something ()
{
	vector a;

	a = VMIN;
	a = VMAX;
	send (VAL1, VAL2);
}
