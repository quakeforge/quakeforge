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

-(double *) components
{
	return components;
}

-(int)indexFor:(unsigned)mask
{
	return [layout bladeIndex:mask];
}

-(double *) componentFor:(BasisBlade *) blade
{
	return &components[[layout bladeIndex:[blade mask]]];
}

-(MultiVector *) product:(MultiVector *) rhs
{
	MultiVector *prod = [MultiVector new:algebra];
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

-(MultiVector *) divide:(MultiVector *) rhs
{
	MultiVector *sqr = [rhs product:rhs];
	double      smag = sqr.components[[sqr indexFor:0]];
	MultiVector *div = [self product:rhs];
	for (int i = 0; i < div.num_components; i++) {
		div.components[i] /= smag;
	}
	return div;
}

-(MultiVector *) wedge:(MultiVector *) rhs
{
	MultiVector *prod = [MultiVector new:algebra];
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

-(MultiVector *) antiwedge:(MultiVector *) rhs
{
	return [[[self dual] wedge:[rhs dual]] undual];
}

-(MultiVector *) dot:(MultiVector *) rhs
{
	MultiVector *prod = [MultiVector new:algebra];
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
			BasisBlade *b = [lb geometricProduct:rb metric:[algebra metric]];
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

-(MultiVector *) plus:(MultiVector *) rhs
{
	MultiVector *plus = [MultiVector new:algebra];
	for (int i = 0; i < num_components; i++) {
		double c = components[i];
		if (!c) {
			continue;
		}
		BasisBlade *b = [layout bladeAt:i];
		int ind = [plus.layout bladeIndex:[b mask]];
		plus.components[ind] += c;
	}
	for (int i = 0; i < rhs.num_components; i++) {
		double c = rhs.components[i];
		if (!c) {
			continue;
		}
		BasisBlade *b = [rhs.layout bladeAt:i];
		int ind = [plus.layout bladeIndex:[b mask]];
		plus.components[ind] += c;
	}
	return plus;
}

-(MultiVector *) minus:(MultiVector *) rhs
{
	MultiVector *minus = [MultiVector new:algebra];
	for (int i = 0; i < num_components; i++) {
		double c = components[i];
		if (!c) {
			continue;
		}
		BasisBlade *b = [layout bladeAt:i];
		int ind = [minus.layout bladeIndex:[b mask]];
		minus.components[ind] += c;
	}
	for (int i = 0; i < rhs.num_components; i++) {
		double c = rhs.components[i];
		if (!c) {
			continue;
		}
		BasisBlade *b = [rhs.layout bladeAt:i];
		int ind = [minus.layout bladeIndex:[b mask]];
		minus.components[ind] -= c;
	}
	return minus;
}

-(MultiVector *) dual:(int) undual
{
	MultiVector *dual = [MultiVector new:algebra];
	unsigned I_mask = (1 << [algebra dimension]) - 1;
	int dim = [algebra dim];
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		BasisBlade *blade = [layout bladeAt:i];
		unsigned d_mask = I_mask ^ [blade mask];
		int flips = undual ? count_flips (d_mask, [blade mask])
						   : count_flips ([blade mask], d_mask);
		double s = flips & 1 ? -1 : 1;

		double ls = [blade scale];
		int dual_ind = [dual.layout bladeIndex:d_mask];
		dual.components[dual_ind] += s * components[i];
	}
	return dual;
}

-(MultiVector *) dual
{
	return [self dual:0];
}

-(MultiVector *) undual
{
	return [self dual:1];
}

-(MultiVector *) reverse
{
	MultiVector *reverse = [MultiVector new:algebra];
	for (int i = 0; i < num_components; i++) {
		if (!components[i]) {
			continue;
		}
		double c = components[i];
		BasisBlade *b = [layout bladeAt:i];
		int         g = [b grade];
		unsigned    mask = [b mask];
		double s = g & 2 ? -1 : 1;//FIXME do in BasisBlade?
		int ind = [reverse.layout bladeIndex:mask];
		reverse.components[ind] += s * c;
	}
	return reverse;
}
@end
