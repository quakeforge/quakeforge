#include "test-harness.h"
#pragma warn no-vararg-integer
typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x04) scalar_t;
typedef PGA.group_mask(0x01) vector_t;
typedef PGA.group_mask(0x20) trivector_t;
typedef PGA.group_mask(0x10) quadvector_t;
typedef union {
	PGA.group_mask(0x0a) bvec;
	struct {
		PGA.group_mask(0x02) dir;
		PGA.group_mask(0x08) mom;
	};
} bivector_t;
typedef union {
	PGA.group_mask(0x1e) mvec;
	struct {
		bivector_t bvec;
		scalar_t   scalar;
	};
} evengrades_t;
typedef union {
	PGA.group_mask(0x21) mvec;
	struct {
		vector_t vec;
		trivector_t tvec;
	};
} oddgrades_t;

int
main (void)
{
	if (sizeof (scalar_t) != sizeof (float)) {
		printf ("scalar has wrong size: %d\n", sizeof (scalar_t));
		return 1;
	}
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (vector_t));
		return 1;
	}
	// the pair of vec3s in a bivector have an alignment of 4
	if (sizeof (bivector_t) != 8 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (bivector_t));
		return 1;
	}
	if (sizeof (bivector_t) != sizeof (PGA.group_mask(0x0a))) {
		printf ("bivector group has wrong size: %d\n",
				sizeof (PGA.group_mask(0x0a)));
		return 1;
	}
	if (sizeof (trivector_t) != 4 * sizeof (scalar_t)) {
		printf ("trivector has wrong size: %d\n", sizeof (trivector_t));
		return 1;
	}
	if (sizeof (quadvector_t) != sizeof (scalar_t)) {
		printf ("quadvector has wrong size: %d\n", sizeof (quadvector_t));
		return 1;
	}

	scalar_t scalar;
	vector_t vec, vecb;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	quadvector_t qvec, qvecb;
	@algebra (PGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + e0;
		bvec.bvec = 4*e20 - 3*e01 + 2*e12;
		tvec = 7*e012;
		qvec = 8*e0123;

		vecb = 5*e1 + 12*e2 - 13*e0;
		bvecb.bvec = 6*e20 + 4*e01 + 1*e12;
		tvecb = 3*e032;
		qvecb = 1*e0123;
	}

	if (scalar != 42) {
		printf ("scalar != 42: %g\n", scalar);
		return 1;
	}
	if ((vec4)vec != '3 -2 0 1') {
		printf ("vec != '3 -2 0 1': %q\n", vec);
		return 1;
	}
	if ((vec3)bvec.dir != '0 0 2' || (vec3)bvec.mom != '-3 -4 0') {
		printf ("bvec != '0 0 2' '-3 -4 0': %v %v\n", bvec.dir, bvec.mom);
		return 1;
	}
	if ((vec4)tvec != '0 0 -7 0') {
		printf ("tvec != '0 0 -7': %g\n", tvec);
		return 1;
	}
	if ((scalar_t)qvec != 8) {
		printf ("tvec != 8: %g\n", (scalar_t) qvec);
		return 1;
	}
	return 0;		// to survive and prevail :)
}
