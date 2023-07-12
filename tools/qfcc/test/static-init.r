#include "test-harness.h"

int count_down ()
{
	static int count = 2;
	count--;
	return count > 0;
}

int main()
{
	int         ret = 0;
	count_down ();
	if (count_down ()) {
		printf ("did not reach 0\n");
		ret |= 1;
	}
	return ret;
}
