#include "metric.h"

@implementation Metric
+(Metric *)R:(int)p, int m, int z
{
	Metric     *metric = [[[Metric alloc] init] autorelease];
	metric.plus = (1 << p) - 1;
	metric.minus = ((1 << m) - 1) << p;
	metric.zero = ((1 << z) - 1) << (p + m);
	return metric;
}

static double
count_minus (unsigned minus)
{
	double      s = 1;
	while (minus) {
		if (minus & 1) {
			s = -s;
		}
		minus >>= 1;
	}
	return s;
}

-(double)apply:(unsigned) a, unsigned b
{
	// find all the squared elements
	unsigned    c = a & b;
	// any elements that square to 0 result in 0
	if (c & zero) {
		return 0;
	}
	return count_minus (c & minus);
}

@end
