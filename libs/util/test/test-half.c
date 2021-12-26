#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/mathlib.h"

struct {
	int16_t     half;
	float       flt;
} tests [] = {
	{0x3c00, 1.0f},
	{0x3c01, 1.0009765625f},
	{0xc000, -2.0f},
	{0x7bff, 65504.0f},
	{0x0400, 6.103515625e-05f},
	{0x03ff, 6.0975551605224609375e-05f},
	{0x0001, 5.9604644775390625e-08f},
	{0x0000, 0.0f},
	{0x8000, -0.0f},
#if defined(__GNUC_PREREQ)
# if __GNUC_PREREQ(3,3)
	{0x7c00, __builtin_huge_val ()},
	{0xfc00, -__builtin_huge_val ()},
# endif
#endif
	{0x3555, 0.333251953125f},
	{0x3e00, 1.5f},
	{0x0000, 0.0f},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_tests; i++) {
		float       flt;
		int16_t     half;

		flt = HalfToFloat (tests[i].half);
		half = FloatToHalf (tests[i].flt);

		if (flt != tests[i].flt || half != tests[i].half) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %4x %.20g\n", tests[i].half, tests[i].flt);
			printf ("got   : %4x %.20g\n", half, flt);
		}
	}
	return res;
}
