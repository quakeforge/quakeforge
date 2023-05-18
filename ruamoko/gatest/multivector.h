#ifndef __multivector_h
#define __multivector_h
#include <Object.h>

@class Algebra;
@class BasisLayout;

@interface MultiVector : Object
{
	double     *components;
	Algebra    *algebra;
	BasisLayout *layout;
	int         num_components;
}
+(MultiVector *) new:(Algebra *) algebra;
// NOTE: values must have the same layout as algebra
+(MultiVector *) new:(Algebra *) algebra values:(double *) values;
+(MultiVector *) new:(Algebra *) algebra group:(BasisGroup *) group;
// NOTE: values must have the same layout as group
+(MultiVector *) new:(Algebra *) algebra group:(BasisGroup *) group values:(double *) values;
+(MultiVector *) copy:(MultiVector *) src;
-(MultiVector *) product:(MultiVector *) rhs;
-(MultiVector *) wedge:(MultiVector *) rhs;
-(MultiVector *) dot:(MultiVector *) rhs;
-(MultiVector *) dual;
@end

#endif//__multivector_h
