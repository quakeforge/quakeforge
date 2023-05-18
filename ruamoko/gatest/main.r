#include <AutoreleasePool.h>
#include "algebra.h"
#include "basisblade.h"
#include "basisgroup.h"
#include "metric.h"
#include "multivector.h"
#include "util.h"

@static AutoreleasePool *autorelease_pool;
@static void
arp_start (void)
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}

@static void
arp_end (void)
{
	[autorelease_pool release];
	autorelease_pool = nil;
}

int
main ()
{
	arp_start ();

	BasisBlade *a = [[BasisBlade basis:1] retain];
	BasisBlade *b = [[BasisBlade basis:2] retain];
	BasisBlade *c = [[BasisBlade basis:4] retain];
	BasisBlade *d = [[BasisBlade basis:8] retain];
	BasisBlade *blades[] = {a, b, c, d};
	static string names[] = {"a", "b", "c", "d"};

//	printf ("a: %@\n", a);
//	printf ("b: %@\n", b);
//	printf ("c: %@\n", c);
//	printf ("d: %@\n", d);

	arp_end ();
#if 0
	arp_start ();
	for (int i = 0; i < 4; i++) {
		arp_end ();
		arp_start ();
		BasisBlade *vec = blades[i];
		printf ("%s: %@\n", names[i], vec);
		for (int j = 0; j < 4; j++) {
			BasisBlade *bvec = [vec outerProduct:blades[j]];
			if (![bvec scale]) {
				continue;
			}
			printf ("%s^%s: %@\n", names[i], names[j], bvec);
			for (int k = 0; k < 4; k++) {
				BasisBlade *tvec = [bvec outerProduct:blades[k]];
				if (![tvec scale]) {
					continue;
				}
				printf ("%s^%s^%s: %@\n", names[i], names[j], names[k],
						tvec);
				for (int l = 0; l < 4; l++) {
					BasisBlade *qvec = [tvec outerProduct:blades[l]];
					if (![qvec scale]) {
						continue;
					}
					printf ("%s^%s^%s^%s: %@\n",
							names[i], names[j], names[k], names[l],
							qvec);
				}
			}
		}
	}
	arp_end ();
#endif
	arp_start ();

	Metric *m = [Metric R:3,0,1];
	BasisBlade *ad = [a geometricProduct:d metric:m];
	BasisBlade *prod = [ad geometricProduct:ad metric:m];
	printf ("%s%s %s%s: %@\n",
			names[0], names[3], names[0], names[3], prod);

	Algebra *alg = [Algebra R:3, 0, 1];
	double vals1[32] = {1, 0, 0, 8};
	static double vals2[32] = {0, 1, 0, 8};//FIXME qfcc bug (static)
	static double origin_vals[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
	MultiVector *plane1 = [MultiVector new:alg values:vals1];
	MultiVector *plane2 = [MultiVector new:alg values:vals2];
	MultiVector *origin = [MultiVector new:alg values:origin_vals];

	MultiVector *line = [plane1 wedge:plane2];
	MultiVector *point = [[line dot:origin] product:line];
	printf ("plane1:%@\nplane2:%@\nline:%@\norigin:%@\n", plane1, plane2, line, origin);
	printf ("point:%@\n", point);

	arp_end ();
	return 0;
}
