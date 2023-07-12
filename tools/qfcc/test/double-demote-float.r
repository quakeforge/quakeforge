#pragma warn error

double a;
float b;
int main ()
{
	b = a;
	return 1;	// test fails if compile succeeds
}
