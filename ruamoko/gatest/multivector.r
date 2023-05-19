#include <string.h>
#include "algebra.h"
#include "basisblade.h"
#include "basislayout.h"
#include "multivector.h"
#include "util.h"

@implementation MultiVector
static MultiVector *new_mv (Algebra *algebra, BasisLayout *layout)
{
	MultiVector *mv = [[[MultiVector alloc] init] autorelease];
	mv.algebra = [algebra retain];
	layout = layout ? layout : [algebra layout];
	mv.layout = [layout retain];
	mv.num_components = [layout num_components];
	mv.components = obj_malloc (mv.num_components * sizeof (double));
	return mv;
}

+(MultiVector *) new:(Algebra *) algebra
{
	MultiVector *mv = new_mv (algebra, nil);
	for (int i = 0; i < mv.num_components; i++) {
		mv.components[i] = 0;
	}
	return mv;
}

+(MultiVector *) new:(Algebra *) algebra values:(double *) values
{
	MultiVector *mv = new_mv (algebra, nil);
	for (int i = 0; i < mv.num_components; i++) {
		mv.components[i] = values[i];
	}
	return mv;
}

+(MultiVector *) new:(Algebra *) algebra group:(BasisGroup *) group
{
	BasisLayout *layout = [BasisLayout new:1 groups:&group];
	MultiVector *mv = new_mv (algebra, layout);
	for (int i = 0; i < mv.num_components; i++) {
		mv.components[i] = 0;
	}
	return mv;
}

+(MultiVector *) new:(Algebra *) algebra group:(BasisGroup *) group values:(double *) values
{
	BasisLayout *layout = [BasisLayout new:1 groups:&group];
	MultiVector *mv = new_mv (algebra, layout);
	for (int i = 0; i < mv.num_components; i++) {
		mv.components[i] = values[i];
	}
	return mv;
}

+(MultiVector *) copy:(MultiVector *) src
{
	MultiVector *mv = new_mv (src.algebra, src.layout);
	for (int i = 0; i < mv.num_components; i++) {
		mv.components[i] = src.components[i];
	}
	return mv;
}

-(void)dealloc
{
	[algebra release];
	[layout release];
	obj_free (components);
	[super dealloc];
}

-(string)describe
{
	string str = nil;

	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		BasisBlade *b = [layout bladeAt:i];
		str = sprintf ("%s%s%g%s", str, str ? " + " : "", components[i], [b name]);
	}
	if (!str) {
		str = "0";
	}
	return str;
}

-(MultiVector *) product:(MultiVector *) rhs
{
	MultiVector *prod = new_mv (algebra, nil);
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		double lc = components[i];
		BasisBlade *lb = [layout bladeAt:i];
		for (int j = 0; j < rhs.num_components; j++) {
			if (!rhs.components[j]) {
				continue;
			}
			double rc = rhs.components[j];
			BasisBlade *rb = [rhs.layout bladeAt:j];
			BasisBlade *b = [lb geometricProduct:rb metric:[algebra metric]];
			double s = [b scale];
			if (!s) {
				continue;
			}
			int ind = [prod.layout bladeIndex:[b mask]];
			prod.components[ind] += s * lc * rc;
		}
	}
	return prod;
}

-(MultiVector *) wedge:(MultiVector *) rhs
{
	MultiVector *prod = new_mv (algebra, nil);
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		double lc = components[i];
		BasisBlade *lb = [layout bladeAt:i];
		for (int j = 0; j < rhs.num_components; j++) {
			if (!rhs.components[j]) {
				continue;
			}
			double rc = rhs.components[j];
			BasisBlade *rb = [rhs.layout bladeAt:j];
			BasisBlade *b = [lb outerProduct:rb];
			double s = [b scale];
			if (!s) {
				continue;
			}
			int ind = [prod.layout bladeIndex:[b mask]];
			prod.components[ind] += s * lc * rc;
		}
	}
	return prod;
}

-(MultiVector *) dot:(MultiVector *) rhs
{
	MultiVector *prod = new_mv (algebra, nil);
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		double lc = components[i];
		BasisBlade *lb = [layout bladeAt:i];
		int    lg = [lb grade];
		for (int j = 0; j < rhs.num_components; j++) {
			if (!rhs.components[j]) {
				continue;
			}
			double rc = rhs.components[j];
			BasisBlade *rb = [rhs.layout bladeAt:j];
			int         rg = [rb grade];
			BasisBlade *b = [lb geometricProduct:rb];
			int         g = [b grade];
			if ((lg <= rg) && (g != rg - lg)) {
				continue;
			}
			if ((lg > rg) && (g != lg - rg)) {
				continue;
			}
			double s = [b scale];
			if (!s) {
				continue;
			}
			int ind = [prod.layout bladeIndex:[b mask]];
			prod.components[ind] += s * lc * rc;
		}
	}
	return prod;
}

-(MultiVector *) dual
{
	MultiVector *dual = new_mv (algebra, nil);
	unsigned dual_mask = (1 << [algebra dimension]) - 1;
	printf ("dual: %x %d\n", dual_mask, [algebra dimension]);
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		double lc = components[i];
		BasisBlade *lb = [layout bladeAt:i];
		unsigned mask = [lb mask] ^ dual_mask;
		double s = 1;
		int ind = [layout bladeIndex:mask];
		printf ("    : %x %d\n", mask, ind);
		dual.components[ind] += s * lc;
	}
	return dual;
}
@end
