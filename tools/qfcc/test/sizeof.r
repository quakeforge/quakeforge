#pragma bug die
#include "test-harness.h"

@class Snafu;

int
main()
{
	int s = sizeof (Snafu *);
	printf ("sizeof (Snafu *) == %d\n", s);
	return s != 4;
}
