#include "metric.h"
#include "util.h"

@implementation Metric
+(Metric *)R:(int)p, int m, int z
{
	Metric     *metric = [[[Metric alloc] init] autorelease];
	metric.plus = ((1 << p) - 1) << z;
	metric.minus = ((1 << m) - 1) << (z + p);
	metric.zero = (1 << z) - 1;
	return metric;
}

static double
count_minus (unsigned minus)
{
	return count_bits (minus) & 1 ? -1 : 1;
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
