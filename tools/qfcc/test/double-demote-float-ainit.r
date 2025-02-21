#pragma bug die
#pragma warn error

double a;
int b[] = {1.0d};
int main ()
{
	return 1;	// test fails if compile succeeds
}
