float VAL1 = 4;
float VAL2 = 6;

void send (float to, ...) = #0;

void do_something ()
{
	send (VAL1, VAL2);
}
