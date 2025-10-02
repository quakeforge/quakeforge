#pragma bug die
#include "test-harness.h"


int
main()
{
	struct {
		uvec2 x;
	} foo = {'1 2'};
	auto ptr = &foo;
	ptr.x[0] = 3;
	//FIXME conversion of bvec always uses |
	return foo.x != '3 2' ? 1 : 0;
}
