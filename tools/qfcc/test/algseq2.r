#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.vec plane_t;

plane_t foo (plane_t a, bivec_t b)
{
	a = (b * a * ~b).vec;//FIXME qvec doesn't cancel
	return a;
}

int
main()
{
	bivec_t Y = {
		.bvect = (PGA.bvect)'0.6 0 0.8',
		.bvecp = (PGA.bvecp)'0.8 1 -0.6',
	};
	plane_t P = {
		//.vec = (PGA.vec)'0.6 -0.8 0 1',
		.vec = (PGA.vec)'0 -1 0 1',
	};
	auto X = foo (P, Y);
	printf ("Y:%.9g %.9v %.9v %.9g\n", Y.scalar, Y.bvect, Y.bvecp, Y.qvec);
	printf ("P:%.9q\n", P.vec);
	printf ("X:%.9q\n", X.vec);

	if (Y.scalar != 0
		|| (vec3)Y.bvect != '0.6 0 0.8'
		|| (vec3)Y.bvecp != '0.8 1 -0.6'
		|| (float)Y.qvec != 0) {
		printf ("incorrect Y\n");
		return 1;
	}
	//if ((vec4)P != '0.6 -0.8 0 1') {
	if ((vec4)P != '0 -1 0 1') {
		printf ("incorrect P\n");
		return 1;
	}
	if ((vec4)X != '0 1 0 -1') {
		printf ("incorrect X\n");
		return 1;
	}
	return 0;
}
