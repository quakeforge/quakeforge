#ifndef __metric_h
#define __metric_h

#include <Object.h>

@interface Metric : Object
{
	unsigned    plus;	// mask of elements that square to +1
	unsigned    minus;	// mask of elements that square to -1
	unsigned    zero;	// mask of elements that square to 0
}
+(Metric *)R:(int)p, int m, int z;
-(double)apply:(unsigned) a, unsigned b;
@end

#endif//__metric_h
