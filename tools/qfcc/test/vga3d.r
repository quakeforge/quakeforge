#include "test-harness.h"
#pragma warn no-vararg-integer
typedef @algebra(float(3,0,0)) VGA;
typedef VGA.group_mask(0x08) scalar_t;
typedef VGA.group_mask(0x01) vector_t;
typedef VGA.group_mask(0x04) bivector_t;
typedef VGA.group_mask(0x02) trivector_t;
typedef union {
	VGA.group_mask(0x0c) mvec;
	struct {
		bivector_t bvec;
		scalar_t   scalar;
	};
} evengrades_t;
typedef union {
	VGA.group_mask(0x3) mvec;
	struct {
		vector_t vec;
		trivector_t tvec;
	};
} oddgrades_t;

static int
test_wedge (void)
{
	scalar_t scalar;
	vector_t vec, vecb, vecc, vecd;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	@algebra (VGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + 4*e3;
		bvec = 11*e23 - 4*e31 + 2*e12;
		tvec = 2*e123;

		vecb = 5*e1 + 12*e2;
		bvecb = 1*e12;
		tvecb = 1*e123;

		vecc = 5*e1 + 4*e2 - 3*e3;
		vecd = e1 + e2 + e3;
	}

	if (scalar != 42) {
		printf ("scalar != 42: %g\n", scalar);
		return 1;
	}
	if ((vec3)vec != '3 -2 4') {
		printf ("vec != '3 -2 4': %v\n", vec);
		return 1;
	}
	if ((vec3)bvec != '11 -4 2') {
		printf ("bvec != '11 -4 2': %v\n", bvec);
		return 1;
	}
	if ((scalar_t)tvec != 2) {
		printf ("tvec != 2: %g\n", (scalar_t)tvec);
		return 1;
	}

	bivector_t a = vec ∧ vecb;
	if ((vec3)a != '-48 20 46') {
		printf ("vec ∧ vecb != '-48 20 46': %v\n", a);
		return 1;
	}

	auto b = a ∧ vecc;
	if ((scalar_t)b != -298) {
		printf ("a ∧ vecc != -298: %g\n", (scalar_t)b);
		return 1;
	}

	auto c = b ∧ vecd;
	if ((scalar_t)c != 0) {
		printf ("b ∧ vecd != 0': %g\n", (scalar_t) c);
		return 1;
	}

	a = vecb ∧ vec;
	if ((vec3)a != '48 -20 -46') {
		printf ("vecb ∧ vec != '48 -20 -46': %v\n", a);
		return 1;
	}

	b = vecc ∧ a;
	if ((scalar_t)b != 298) {
		printf ("vecc ∧ a != 298: %g\n", (scalar_t)b);
		return 1;
	}

	c = vecd ∧ b;
	if ((scalar_t)c != 0) {
		printf ("vecd ^ b != 0': %g\n", (scalar_t) c);
		return 1;
	}

	c = a ∧ (vecc ∧ vecd);
	if ((scalar_t)c != 0) {
		printf ("a ∧ (vecc ∧ vecd) != 0': %g\n", (scalar_t) c);
		return 1;
	}

	c = (vecd ∧ vecc) ∧ a;
	if ((scalar_t)c != 0) {
		printf ("(vecd ∧ vecc) ∧ a != 0': %g\n", (scalar_t) c);
		return 1;
	}
	return 0;
}

static int
test_dot (void)
{
	scalar_t scalar;
	vector_t vec, vecb, vecc, vecd;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	@algebra (VGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + 4*e3;
		bvec = 11*e23 - 4*e31 + 2*e12;
		tvec = 8*e123;

		vecb = 5*e1 + 12*e2;
		bvecb = 1*e12;
		tvecb = 1*e123;

		vecc = 5*e1 + 4*e2 - 3*e3;
		vecd = e1 + e2 + e3;
	}

	auto a = bvec • tvec;
	if ((vec3)a != '-88 32 -16') {
		printf ("bvec • tvec != '-88 32 -16': %v\n", a);
		return 1;
	}

	auto b = vec • tvec;
	if ((vec3)b != '24 -16 32') {
		printf ("vec • tvec != '24 -16 32': %v\n", b);
		return 1;
	}

	if (bvec • bvec != -141) {
		printf ("(bvec • bvec) != -141': %g\n", bvec • bvec);
		return 1;
	}

	a = vec • bvec;
	if ((vec3)a != '-12 -38 -10') {
		printf ("vec • bvec != '-12 -38 -10': %v\n", a);
		return 1;
	}

	if (vec • vecb != -9) {
		printf ("(vec • vecb) != -9': %g\n", vec • vecb);
		return 1;
	}

	auto d = tvec • tvec;
	if (d != -64) {
		printf ("tvec • tvec != -64: %g\n", a);
		return 1;
	}

	a = tvec • bvec;
	if ((vec3)a != '-88 32 -16') {
		printf ("tvec • vec != '-88 32 -16': %v\n", a);
		return 1;
	}

	b = tvec • vec;
	if ((vec3)b != '24 -16 32') {
		printf ("tvec • vec != '24 -16 32': %v\n", b);
		return 1;
	}

	a = bvec • vec;
	if ((vec3)a != '12 38 10') {
		printf ("bvec • vec != '12 38 10': %v\n", a);
		return 1;
	}

	return 0;
}

static int
test_geom (void)
{
	scalar_t scalar;
	vector_t vec, vecb, vecc, vecd;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	@algebra (VGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + 4*e3;
		bvec = 11*e23 - 4*e31 + 2*e12;
		tvec = 8*e123;

		vecb = 5*e1 + 12*e2;
		bvecb = 1*e12;
		tvecb = 1*e123;

		vecc = 5*e1 + 4*e2 - 3*e3;
		vecd = e1 + e2 + e3;
	}

	if (tvec * tvecb != -8) {
		printf ("(tvec * tvecb) != -8': %g\n", tvec * tvecb);
		return 1;
	}

	auto d = bvec * tvec;
	if ((vec3)d != '-88 32 -16') {
		printf ("bvec * tvec != '-88 32 -16': %v\n", d);
		return 1;
	}

	auto e = vec * tvec;
	if ((vec3)e != '24 -16 32') {
		printf ("vec * tvec != 0 '24 -16 32': %v\n", e);
		return 1;
	}

	auto f = bvec * bvec;
	if (f != -141) {
		printf ("bvec * bvec != -141: %g\n", f);
		return 1;
	}

	oddgrades_t odd = { .mvec = vec * bvec };
	if ((vec3)odd.vec != '-12 -38 -10' || (scalar_t)odd.tvec != 49) {
		printf ("vec * bvec != '-12 -38 -10' 49: %v %g\n",
				odd.vec, (scalar_t)odd.tvec);
		return 1;
	}

	evengrades_t even = { .mvec = vec * vecb };
	if (even.scalar != -9 || (vec3)even.bvec != '-48 20 46') {
		printf ("(vec * vecb) != -9 '-48 20 46': %g %v\n",
				even.scalar, even.bvec);
		return 1;
	}

	auto b = tvec * bvec;
	if ((vec3)b != '-88 32 -16') {
		printf ("tvec * bvec != '-88 32 -16': %v\n", b);
		return 1;
	}

	e = tvec * vec;
	if ((vec3)e != '24 -16 32') {
		printf ("tvec * vec != '24 -16 32': %v\n", e);
		return 1;
	}

	odd.mvec = bvec * vec;
	if ((vec3)odd.vec != '12 38 10' || (scalar_t)odd.tvec != 49) {
		printf ("vec * bvec != '12 38 10' 49: %v %g\n",
				odd.vec, (scalar_t)odd.tvec);
		return 1;
	}

	return 0;
}

static int
test_basics (void)
{
	return 0;
}

int
main (void)
{
	if (sizeof (scalar_t) != sizeof (float)) {
		printf ("scalar has wrong size: %d\n", sizeof (scalar_t));
		return 1;
	}
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		printf ("vector has wrong size: %d\n", sizeof (vector_t));
		return 1;
	}
	if (sizeof (bivector_t) != 4 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (bivector_t));
		return 1;
	}
	if (sizeof (trivector_t) != sizeof (scalar_t)) {
		printf ("trivector has wrong size: %d\n", sizeof (trivector_t));
		return 1;
	}
	if (sizeof (evengrades_t) != 4) {
		evengrades_t e;
		#define offsetof(s,f) ((int)(&((s*)0).f))
		printf ("evengrades has wrong size: %d\n", sizeof (evengrades_t));
		printf ("mvec: %d, bvec: %d, scalar: %d\n",
				sizeof (e.mvec), sizeof (e.bvec), sizeof (e.scalar));
		printf ("mvec: %d, bvec: %d, scalar: %d\n",
				offsetof (evengrades_t, mvec), offsetof (evengrades_t, bvec),
				offsetof (evengrades_t, scalar));
		return 1;
	}
	if (sizeof (oddgrades_t) != 4) {
		oddgrades_t o;
		#define offsetof(s,f) ((int)(&((s*)0).f))
		printf ("oddgrades has wrong size: %d\n", sizeof (oddgrades_t));
		printf ("mvec: %d, vec: %d, tvec: %d\n",
				sizeof (o.mvec), sizeof (o.vec), sizeof (o.tvec));
		printf ("mvec: %d, vec: %d, tvec: %d\n",
				offsetof (oddgrades_t, mvec), offsetof (oddgrades_t, vec),
				offsetof (oddgrades_t, tvec));
		return 1;
	}

	if (test_wedge ()) {
		printf ("wedge products failed\n");
		return 1;
	}

	if (test_dot ()) {
		printf ("dot products failed\n");
		return 1;
	}

	if (test_geom ()) {
		printf ("geometric products failed\n");
		return 1;
	}

	if (test_basics ()) {
		printf ("basics failed\n");
		return 1;
	}

	return 0;		// to survive and prevail :)
}
