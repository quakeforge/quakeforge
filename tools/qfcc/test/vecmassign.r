#pragma bug die
#include "test-harness.h"


int
main()
{
	struct {
		uivec2 x;
	} foo = {'1 2'};
	auto ptr = &foo;
	ptr.x[0] = 3;
	return foo.x != '3 2';
}
