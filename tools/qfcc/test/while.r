#pragma bug die
int min;
int times;

void bar (int x)
{
	if (x < min)
		min = x;
}

void foo (int count)
{
	int safety = count;	// in case qfcc screws up (prevents infinite loop)
	min = count;
	while (count--) {	//XXX this is the code being tested
		bar (count);
		times++;
		safety--;
		if (safety < 0)
			break;
	}
}

int
main ()
{
	int c = 3;
	foo (c);
	return min || times != c;
}
