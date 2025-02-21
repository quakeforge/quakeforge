#pragma bug die
#pragma warn error

double a;
int main ()
{
	float b = a;
	return 1;	// test fails if compile succeeds
}
