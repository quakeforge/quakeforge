#include "test-harness.h"
vector
t1()
{
	return [1, 2, 3];
}

vector
t2(float x)
{
	return [x, x, x];
}

vector
t3(float x)
{
	return [x, t2(9).z, x] * 2;
}

int
main ()
{
	int ret = 0;
	vector v;

	v = t2(5);
	if (v != [5, 5, 5]) {
		printf("t2(5) = %v\n", v);
		ret |= 1;
	}
	v = t3 (5);
	if (v != [10, 18, 10]) {
		printf("t3(5) = %v\n", v);
		ret |= 1;
	}
	return ret;
}
