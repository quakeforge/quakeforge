int a;
double b;
int c = (int) &b;
double d;
void printf (string fmt, ...) = #0;

int main()
{
	int         di = (int) &d;
	int         fail = 0;

	if (!c) {
		printf ("global init address cast fail: %d\n", c);
		fail |= 1;
	}
	if (!di) {
		printf ("local init address cast fail: %d\n", di);
		fail |= 1;
	}
	return fail;
}
