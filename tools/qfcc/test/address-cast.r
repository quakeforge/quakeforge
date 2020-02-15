int a;
double b;
int c;
double d;
void printf (string fmt, ...) = #0;

int main()
{
	int         di = (int) &d;
	int         fail = 0;

	if (!di) {
		printf ("address cast fail: %d\n", di);
		fail |= 1;
	}
	return fail;
}
