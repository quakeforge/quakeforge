#include <stdlib.h>
#include <string.h>

#include "algebra.h"
#include "metric.h"
#include "basisblade.h"
#include "basisgroup.h"
#include "basislayout.h"
#include "multivector.h"
#include "util.h"

@implementation Algebra
+(Algebra *) R:(int)p, int m, int z
{
	Algebra    *a = [[[Algebra alloc] init] autorelease];
	a.metric = [[Metric R:p, m, z] retain];

	a.plus = p;
	a.minus = m;
	a.zero = z;
	int         d = p + m + z;
	a.dimension = d;
	a.num_components = 1 << d;
	BasisBlade **blades = obj_malloc (a.num_components * sizeof (BasisBlade *));
	int        *counts = binomial (d);
	int        *indices = obj_malloc ((d + 1) * sizeof (int));

	indices[0] = 0;
	for (int i = 0; i < d; i++) {
		indices[i + 1] = counts[i];
	}
	prefixsum (indices, d + 1);
	for (int i = 0; i < a.num_components; i++) {
		int         grade = count_bits (i);
		int         ind = indices[grade]++;
		unsigned    mask = i;
		if (!z) {
			// e0 is best for the null vector, but this geometry has no null
			// vectors, so skip it;
			mask <<= 1;
		}
		blades[ind] = [BasisBlade basis:i];
	}

	a.grades = obj_malloc ((d + 1) * sizeof (BasisGroup *));
	for (int i = 0; i < d + 1; i++) {
		int         c = counts[i];
		int         ind = indices[i];
		a.grades[i] = [[BasisGroup new:c basis:blades + ind - c] retain];
	}

	if (p == 3 && m == 0 && z == 1) {
		// 3d PGA (w squares to 0, x y z square to +1):
		// : x   y   z   w
		// : yz  zx  xy  1
		// : wx  wy  wz  wxyz
		// : wzy wxz wyx xyz
		BasisBlade *pga_blades[16] = {
			blades[2],  blades[3],  blades[4],  blades[1],
			blades[10], blades[9],  blades[7],  blades[0],
			blades[5],  blades[6],  blades[8],  blades[15],
			blades[13], blades[12], blades[11], blades[14],
		};
		BasisGroup *pga_groups[4] = {
			[BasisGroup new:4 basis:pga_blades +  0],
			[BasisGroup new:4 basis:pga_blades +  4],
			[BasisGroup new:4 basis:pga_blades +  8],
			[BasisGroup new:4 basis:pga_blades + 12],
		};
		a.layout = [[BasisLayout new:4 groups: pga_groups] retain];
	} else if (p == 2 && m == 0 && z == 1) {
		// 2d PGA (w squares to 0, x y square to +1):
		// : 1  xy  wx  wy
		// : x  y   w   wxy
		BasisBlade *pga_blades[8] = {
			blades[0], blades[6], blades[4], blades[5],
			blades[2], blades[3], blades[1], blades[7],
		};
		BasisGroup *pga_groups[2] = {
			[BasisGroup new:4 basis:pga_blades + 0],
			[BasisGroup new:4 basis:pga_blades + 4],
		};
		a.layout = [[BasisLayout new:2 groups: pga_groups] retain];
	} else {
		// just use the grades as the default layout
		a.layout = [[BasisLayout new:d + 1 groups: a.grades] retain];
	}

	obj_free (indices);
	obj_free (counts);
	obj_free (blades);
	return a;
}

+(Algebra *) PGA:(int)n
{
	return [Algebra R:n, 0, 1];
}

-(void)dealloc
{
	obj_free (grades);
	[metric release];
	[layout release];
	[super dealloc];
}

-(BasisGroup *)grade:(int)k
{
	return grades[k];
}

-(BasisLayout *)layout
{
	return layout;
}

-(Metric *) metric
{
	return metric;
}

-(int)count
{
	return num_components;
}

-(int)dimension
{
	return dimension;
}

-(MultiVector *) group:(int)group
{
	return [MultiVector new:self group:[layout group:group]];
}

-(MultiVector *) group:(int)group values:(double *)values
{
	return [MultiVector new:self group:[layout group:group] values:values];
}

-(MultiVector *) ofGrade:(int)grade
{
	return [MultiVector new:self group:grades[grade]];
}

-(MultiVector *) ofGrade:(int)grade values:(double *)values
{
	return [MultiVector new:self group:grades[grade] values:values];
}

-(void) print
{
	int count = [layout count];
	for (int i = 0; i < count; i++) {
		BasisGroup *g = [layout group:i];
		int c = [g count];
		printf ("%d %d %@\n", i, c, [g set]);
		for (int j = 0; j < c; j++) {
			printf ("    %@\n", [g bladeAt:j]);
		}
	}
}

-(string) describe
{
	return sprintf ("R(%d,%d,%d)", plus, minus, zero);
}

@end
