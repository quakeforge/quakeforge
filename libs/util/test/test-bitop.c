#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/math/bitop.h"

struct {
	uint32_t     value;
	uint32_t     expect;
} tests [] = {
	{BITOP_RUP(1), 1},
	{BITOP_RUP(2), 2},
	{BITOP_RUP(3), 4},
	{BITOP_RUP(4), 4},
	{BITOP_RUP(5), 8},
	{BITOP_RUP(7), 8},
	{BITOP_RUP(8), 8},
	{BITOP_RUP(0x40000000), 0x40000000},
	{BITOP_RUP(0x40000001), 0x80000000},
	{BITOP_LOG2(1), 0},
	{BITOP_LOG2(2), 1},
	{BITOP_LOG2(3), 2},
	{BITOP_LOG2(4), 2},
	{BITOP_LOG2(5), 3},
	{BITOP_LOG2(7), 3},
	{BITOP_LOG2(8), 3},
	{BITOP_LOG2(9), 4},
	{BITOP_LOG2(15), 4},
	{BITOP_LOG2(16), 4},
	{BITOP_LOG2(17), 5},
	{BITOP_LOG2(31), 5},
	{BITOP_LOG2(32), 5},
	{BITOP_LOG2(33), 6},
	{BITOP_LOG2(0x40000000), 30},
	{BITOP_LOG2(0x40000001), 31},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_tests; i++) {
		if (tests[i].value != tests[i].expect) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %8x\n", tests[i].expect);
			printf ("got   : %8x\n", tests[i].value);
		}
	}
	return res;
}
