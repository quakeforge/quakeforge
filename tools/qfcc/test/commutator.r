#include "test-harness.h"

typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0xa) bivector_t;

bivector_t
commutator (bivector_t a)
{
	bivector_t c = -@undual(a × ⋆a);
	return c;
}

vector
cross (vector a, vector b)
{
	return -(a × b);
}

int
main ()
{
	bivector_t m = {
		.bvect = (bivector_t.bvect) '1 0 0'f,
		.bvecp = (bivector_t.bvecp) '0 1 0'f,
	};
	float x = 42;
	auto dm = commutator (m);
	printf ("%v %v %g\n", dm.bvect, dm.bvecp, x);
	if ((vec3) dm.bvect != '0 0 0' || (vec3) dm.bvecp != '0 0 1') {
		return 1;
	}
	return 0;
}
