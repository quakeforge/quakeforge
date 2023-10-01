#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;

point_t
apply_motor (motor_t m, point_t p)
{
	return (m * p * ~m).tvec;
}

int
main (void)
{
	motor_t m = {
		.scalar = 0.707106769,
		.bvect = (PGA.bvect) '0 0 0.707106769'f,
		.bvecp = (PGA.bvecp) '0 -4.2426405 0'f,
		.qvec = (PGA.qvec) 0,
	};
	point_t p = (point_t)'10 4 -1.5 1'f;
	point_t n = apply_motor (m, p);
	printf ("n: %.9q\n", n);
	if ((vec4)n != '9.99999905 -3.99999952 -1.49999988 0.99999994'f) {
		return 1;
	}
	return 0;
}
