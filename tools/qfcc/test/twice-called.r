#pragma bug die
#include "test-harness.h"

int counter;

int
function (void)
{
	return ++counter;
}

int
main (void)
{
	int ret = 0;
	counter = 0;
	//function ();
	//if (counter != 1) {
		//printf ("discarded return not called only once\n");
	//	ret = 1;
	//}
	counter = 0;
	printf ("function: %d\n", function ());
	if (counter != 1) {
		//printf ("used return not called only once\n");
		ret = 1;
	}
	return ret;
}
