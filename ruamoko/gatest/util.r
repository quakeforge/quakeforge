#include <runtime.h>
#include "util.h"

void printf(string fmt, ...) = #0;
void traceon(void) = #0;
void traceoff(void) = #0;

int *
binomial (int n)
{
	int        *coef = obj_malloc ((n + 1) * sizeof (int));
	int         c = 1;
	for (int i = 0; i < n + 1; i++) {
		coef[i] = c;
		c = (c * (n - i)) / (i + 1);
	}
	return coef;
}

int
count_bits (unsigned v)
{
	int         c = 0;
	for (; v; c++) {
		v &= v - 1u; //XXX bug in qfcc: just 1 results in extra temp
	}
	return c;
}

int
count_flips (unsigned a, unsigned b)
{
	int         c = 0;
	a >>= 1;
	while (a) {
		c += count_bits (a & b);
		a >>= 1;
	}
	return c;
}
