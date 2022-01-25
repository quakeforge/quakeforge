#pragma warn error

double a;
int b;
int main ()
{
	b = a;
	return 1;	// test fails if compile succeeds
}
