#pragma warn error

double a;
int main ()
{
	int b = a;
	return 1;	// test fails if compile succeeds
}
