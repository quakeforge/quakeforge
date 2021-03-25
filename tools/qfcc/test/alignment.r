int a;
double b;
int c;
double d;
void printf (string fmt, ...) = #0;

int main()
{
	int         fail = 0;
	void       *ap = &a;
	void       *bp = &b;
	void       *cp = &c;
	void       *dp = &d;
	int         aa = (int) ap;
	int         ba = (int) bp;
	int         ca = (int) cp;
	int         da = (int) dp;

	if (ba & 1) {
		printf ("double b is not aligned: %d\n", ba);
		fail |= 1;
	}
	if (da & 1) {
		printf ("double d is not aligned: %d\n", da);
		fail |= 1;
	}
	if (ca - aa != 1) {
		printf ("int c (%d) is not adjacant to int a (%d)\n", ca, aa);
		fail |= 1;
	}
	if (ba <= ca) {
		printf ("double b does not come after int c: %d %d\n", ba, ca);
		fail |= 1;
	}
	return fail;
}
