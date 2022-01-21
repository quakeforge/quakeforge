#include "test-harness.h"

int counter;

int
function (void)
{
	return counter++;
}

int
main (void)
{
	int ret = 0;
	counter = 0;
	function ();
	if (counter != 1) {
		printf ("counter != 1: %d\n", counter);
		ret = 1;
	}
	return ret;
}
