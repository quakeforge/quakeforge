#pragma bug die
#pragma warn error

double a;
int b;
int main ()
{
	int x = a == b;
	return 1;	// test fails if compile succeeds
}
