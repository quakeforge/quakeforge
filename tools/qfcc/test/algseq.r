#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;

int
main()
{
	bivec_t Y = {
		.bvect = (PGA.bvect)'0 0 1',
		.bvecp = (PGA.bvecp)'0 -1 0',
	};
	motor_t R = {
		.scalar = 1,
		.bvect = (PGA.bvect)'0 0 0',
		.bvecp = (PGA.bvecp)'-0.5 0 0',
		.qvec = (PGA.qvec)0,
	};
	printf ("Y:%.9g %.9v %.9v %.9g\n", Y.scalar, Y.bvect, Y.bvecp, Y.qvec);
	printf ("R:%.9g %.9v %.9v %.9g\n", R.scalar, R.bvect, R.bvecp, R.qvec);
	R = Y * R;
	printf ("R:%.9g %.9v %.9v %.9g\n", R.scalar, R.bvect, R.bvecp, R.qvec);

	if (R.scalar != 0
		|| (vec3)R.bvect != '0 0 1'
		|| (vec3)R.bvecp != '0 -0.5 0'
		|| (float)R.qvec != 0) {
		printf ("incorrect R\n");
		return 1;
	}
	return 0;
}
