#ifndef __multivector_h
#define __multivector_h
#include <Object.h>

@class Algebra;
@class BasisLayout;
@class BasisBlase;

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

-(double *) components;
-(int)indexFor:(unsigned)mask;
-(double *) componentFor:(BasisBlade *) blade;

-(MultiVector *) product:(MultiVector *) rhs;
-(MultiVector *) divide:(MultiVector *) rhs;
-(MultiVector *) wedge:(MultiVector *) rhs;
-(MultiVector *) antiwedge:(MultiVector *) rhs;
-(MultiVector *) dot:(MultiVector *) rhs;
-(MultiVector *) plus:(MultiVector *) rhs;
-(MultiVector *) minus:(MultiVector *) rhs;
-(MultiVector *) dual;
-(MultiVector *) undual;
-(MultiVector *) reverse;
@end

#endif//__multivector_h
