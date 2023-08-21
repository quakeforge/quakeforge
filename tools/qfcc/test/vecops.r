#include "test-harness.h"

vector a = { 1, 0, 0 };
vector b = { 0, 1, 0 };
vector c = { 0, 0, 1 };

int
main ()
{
	vector v = a × b;
	if (v != c) {
		printf ("cross product failed\n");
		return 1;
	};
	if (v • c != [1, 1, 1]) {
		printf ("dot product failed\n");
		return 1;
	}
	return 0;
}
