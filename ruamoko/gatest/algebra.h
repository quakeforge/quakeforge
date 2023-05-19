#ifndef __algebra_h
#define __algebra_h

#include "Object.h"

@class Metric;
@class BasisGroup;
@class BasisLayout;
@class MultiVector;

@interface Algebra : Object
{
	Metric     *metric;
	BasisGroup **grades;
	BasisLayout *layout;
	int         num_components;
	int         dimension;
}
+(Algebra *) R:(int)p, int m, int z;
+(Algebra *) PGA:(int)n;
-(void) print;
-(BasisGroup *)grade:(int)k;
-(BasisLayout *)layout;
-(Metric *) metric;
-(int)count;
-(int)dimension;

-(MultiVector *) group:(int)group;
-(MultiVector *) group:(int)group values:(double *)values;
-(MultiVector *) ofGrade:(int)grade;
-(MultiVector *) ofGrade:(int)grade values:(double *)values;
@end

#endif//__algebra_h
