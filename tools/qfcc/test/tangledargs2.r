#include "test-harness.h"

string sprintf (string fmt, ...) = #0;
void str_free (string str) = #0;
string str_hold (string str) = #0;

typedef struct bisector_s {
	uint        next, prev, twin;
	uint        index;
} bisector_t;

bisector_t bisector_pool[] = {
	{ 1, 3, 6, 4 },
};

string result;

mat3
RootBisectorVertices (uint i)
{
	return mat3 (1, 0, 0, 0, 1, 0, 0, 0, 1);
}

int
main ()
{
	for (uint i = 0; i < countof (bisector_pool); i++) {
		auto b = &bisector_pool[i];
		auto v = RootBisectorVertices (i);
		result = sprintf ("%2d: %3d %3d %3d %3d %v %v %v", i,
						  b.index, b.next, b.prev, b.twin,
						  v[0], v[1], v[2]);
	}
	printf ("%s\n", result);
	return result != " 0:   4   1   3   6 '1 0 0' '0 1 0' '0 0 1'";
}
