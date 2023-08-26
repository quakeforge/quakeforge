#include "test-harness.h"
#pragma warn no-vararg-integer
typedef @algebra(double(2,0,1)) PGA;
typedef PGA.group_mask(0x02) scalar_t;
typedef PGA.group_mask(0x04) vector_t;
typedef PGA.group_mask(0x01) bivector_t;
typedef PGA.group_mask(0x08) trivector_t;
typedef union {
	PGA.group_mask(0x03) mvec;
	struct {
		bivector_t bvec;
		scalar_t   scalar;
	};
} evengrades_t;
typedef union {
	PGA.group_mask(0x0c) mvec;
	struct {
		vector_t vec;
		trivector_t tvec;
	};
} oddgrades_t;

int
main (void)
{
	if (sizeof (scalar_t) != sizeof (double)) {
		printf ("scalar has wrong size: %d\n", sizeof (scalar_t));
		return 1;
	}
	if (sizeof (vector_t) != 3 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (vector_t));
		return 1;
	}
	if (sizeof (bivector_t) != 3 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (bivector_t));
		return 1;
	}
	if (sizeof (trivector_t) != sizeof (scalar_t)) {
		printf ("trivector has wrong size: %d\n", sizeof (trivector_t));
		return 1;
	}
	scalar_t scalar;
	vector_t vec;
	bivector_t bvec;
	trivector_t tvec;
	@algebra (PGA) {
		scalar = 42;
		vec = 3 * e1 - 2*e2 + e0;
		bvec = 4*e20 - 3*e01 + 2*e12;
		tvec = 7 * e012;
	}
	if (scalar != 42) {
		printf ("scalar != 42: %g\n", scalar);
		return 1;
	}
	if ((dvec3)vec != '3 -2 1'd) {
		printf ("vec != '3 -2 1': %lv\n", vec);
		return 1;
	}
	if ((dvec3)bvec != '4 -3 2'd) {
		printf ("vec != '4 -3 2': %lv\n", bvec);
		return 1;
	}
	if ((double)tvec != 7) {
		printf ("tvec != 7: %g\n", tvec);
		return 1;
	}
	auto t = vec ∧ bvec;
	if ((double)t != 20) {
		printf ("vec ∧ bvec != 20: %lv\n", t);
		return 1;
	}
	auto c = vec • bvec;
	if ((dvec3)c != '4 6 1'd) {
		printf ("vec • bvec != '4 6 1': %lv\n", c);
		return 1;
	}
	auto d = bvec • vec;
	if ((dvec3)d != '-4 -6 -1'd) {
		printf ("bvec • vec != '-4 -6 -1': %lv\n", d);
		return 1;
	}
	oddgrades_t e;
	e.mvec = vec * bvec;
	if ((dvec3)e.vec != '4 6 1'd || (scalar_t)e.tvec != 20) {
		printf ("vec • bvec != '4 6 1' + 20: %lv %g\n", e.vec, e.tvec);
		return 1;
	}
	oddgrades_t f;
	f.mvec = bvec * vec;
	if ((dvec3)f.vec != '-4 -6 -1'd || (scalar_t)f.tvec != 20) {
		printf ("bvec • vec != '-4 -6 -1' + 20: %lv %g\n", f.vec, f.tvec);
		return 1;
	}
	if (vec ∧ tvec || tvec ∧ vec) {
		printf ("didn't get 0: %g %g", vec ∧ tvec, tvec ∧ vec);
		return 0;
	}
//	auto g = vec • tvec;
//	if ((dvec3)g != '-4 -6 -1'd) {
//		printf ("vec • tvec != '-4 -6 -1': %lv\n", g);
//		return 1;
//	}
	return 0;		// to survive and prevail :)
}
