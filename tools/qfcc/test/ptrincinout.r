#include "test-harness.h"

int PL_NewNumber (double val)
{
	return val;
}

int
s_float_func (@inout void *ptr)
{
	auto p = (float *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

float data[2] = { 2.5, 4.75 };

int
main ()
{
	void *ptr = data;
	int a = -1, b = -1;
	if ((a = s_float_func (ptr)) != 2
		|| (b = s_float_func (ptr)) != 4) {
		printf ("got %d %d\n", a, b);
		return 1;
	}
	return 0;
}
