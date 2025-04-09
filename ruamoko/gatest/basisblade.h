#ifndef __basisblade_h
#define __basisblade_h
#include <Object.h>

@class Metric;

@interface BasisBlade : Object
{
	unsigned    mask;
	double      scale;
}
+(BasisBlade *) scalar:(double) scale;
+(BasisBlade *) zero;
+(BasisBlade *) basis:(unsigned) mask;
+(BasisBlade *) basis:(unsigned) mask scale:(double) scale;
-(BasisBlade *) product:(BasisBlade *) b isOuter:(int)outer metric:(Metric *) m;
-(BasisBlade *) outerProduct:(BasisBlade *) b;
-(BasisBlade *) geometricProduct:(BasisBlade *) b metric:(Metric *) m;
-(BasisBlade *) geometricProduct:(BasisBlade *) b;
-(int) grade;
-(unsigned) mask;
-(double) scale;
-(string) name;
-(int) commutesWith:(BasisBlade *) b;
-(int) commutesWith:(BasisBlade *) b metric:(Metric *)m;
@end

#endif//__basisblade_h
