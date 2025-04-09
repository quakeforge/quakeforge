#pragma bug die
#include "test-harness.h"
#pragma warn no-vararg-integer

#define error(fmt, ...) \
	do { \
		printf ("%s:%d: in %s: " fmt, __FILE__, __LINE__, __FUNCTION__ \
				__VA_OPT__(,) __VA_ARGS__); \
		ret = 1; \
	} while (false)

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
	int ret = 0;
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
		error ("scalar != 42: %g\n", scalar);
	}
	if ((vec3)vec != '3 -2 4') {
		error ("vec != '3 -2 4': %v\n", vec);
	}
	if ((vec3)bvec != '11 -4 2') {
		error ("bvec != '11 -4 2': %v\n", bvec);
	}
	if ((scalar_t)tvec != 2) {
		error ("tvec != 2: %g\n", (scalar_t)tvec);
	}

	bivector_t a = vec ∧ vecb;
	if ((vec3)a != '-48 20 46') {
		error ("vec ∧ vecb != '-48 20 46': %v\n", a);
	}

	auto b = a ∧ vecc;
	if ((scalar_t)b != -298) {
		error ("a ∧ vecc != -298: %g\n", (scalar_t)b);
	}

	auto c = b ∧ vecd;
	if ((scalar_t)c != 0) {
		error ("b ∧ vecd != 0': %g\n", (scalar_t) c);
	}

	a = vecb ∧ vec;
	if ((vec3)a != '48 -20 -46') {
		error ("vecb ∧ vec != '48 -20 -46': %v\n", a);
	}

	b = vecc ∧ a;
	if ((scalar_t)b != 298) {
		error ("vecc ∧ a != 298: %g\n", (scalar_t)b);
	}

	c = vecd ∧ b;
	if ((scalar_t)c != 0) {
		error ("vecd ^ b != 0': %g\n", (scalar_t) c);
	}

	c = a ∧ (vecc ∧ vecd);
	if ((scalar_t)c != 0) {
		error ("a ∧ (vecc ∧ vecd) != 0': %g\n", (scalar_t) c);
	}

	c = (vecd ∧ vecc) ∧ a;
	if ((scalar_t)c != 0) {
		error ("(vecd ∧ vecc) ∧ a != 0': %g\n", (scalar_t) c);
	}
	return ret;
}

static int
test_dot (void)
{
	int ret = 0;
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
		error ("bvec • tvec != '-88 32 -16': %v\n", a);
	}

	auto b = vec • tvec;
	if ((vec3)b != '24 -16 32') {
		error ("vec • tvec != '24 -16 32': %v\n", b);
	}

	if (bvec • bvec != -141) {
		error ("(bvec • bvec) != -141': %g\n", bvec • bvec);
	}

	a = vec • bvec;
	if ((vec3)a != '-12 -38 -10') {
		error ("vec • bvec != '-12 -38 -10': %v\n", a);
	}

	if (vec • vecb != -9) {
		error ("(vec • vecb) != -9': %g\n", vec • vecb);
	}

	auto d = tvec • tvec;
	if (d != -64) {
		error ("tvec • tvec != -64: %g\n", a);
	}

	a = tvec • bvec;
	if ((vec3)a != '-88 32 -16') {
		error ("tvec • vec != '-88 32 -16': %v\n", a);
	}

	b = tvec • vec;
	if ((vec3)b != '24 -16 32') {
		error ("tvec • vec != '24 -16 32': %v\n", b);
	}

	a = bvec • vec;
	if ((vec3)a != '12 38 10') {
		error ("bvec • vec != '12 38 10': %v\n", a);
	}

	return ret;
}

static int
test_geom (void)
{
	int ret = 0;
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
		error ("(tvec * tvecb) != -8': %g\n", tvec * tvecb);
	}

	auto d = bvec * tvec;
	if ((vec3)d != '-88 32 -16') {
		error ("bvec * tvec != '-88 32 -16': %v\n", d);
	}

	auto e = vec * tvec;
	if ((vec3)e != '24 -16 32') {
		error ("vec * tvec != 0 '24 -16 32': %v\n", e);
	}

	auto f = bvec * bvec;
	if (f != -141) {
		error ("bvec * bvec != -141: %g\n", f);
	}

	oddgrades_t odd = { .mvec = vec * bvec };
	if ((vec3)odd.vec != '-12 -38 -10' || (scalar_t)odd.tvec != 49) {
		error ("vec * bvec != '-12 -38 -10' 49: %v %g\n",
			   odd.vec, (scalar_t)odd.tvec);
	}

	evengrades_t even = { .mvec = vec * vecb };
	if (even.scalar != -9 || (vec3)even.bvec != '-48 20 46') {
		error ("(vec * vecb) != -9 '-48 20 46': %g %v\n",
			   even.scalar, even.bvec);
	}

	auto b = tvec * bvec;
	if ((vec3)b != '-88 32 -16') {
		error ("tvec * bvec != '-88 32 -16': %v\n", b);
	}

	e = tvec * vec;
	if ((vec3)e != '24 -16 32') {
		error ("tvec * vec != '24 -16 32': %v\n", e);
	}

	odd.mvec = bvec * vec;
	if ((vec3)odd.vec != '12 38 10' || (scalar_t)odd.tvec != 49) {
		error ("vec * bvec != '12 38 10' 49: %v %g\n",
			   odd.vec, (scalar_t)odd.tvec);
	}

	return ret;
}

static int
test_basics (void)
{
	return 0;
}

int
main (void)
{
	int ret = 0;
	if (sizeof (scalar_t) != sizeof (float)) {
		error ("scalar has wrong size: %d\n", sizeof (scalar_t));
	}
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		error ("vector has wrong size: %d\n", sizeof (vector_t));
	}
	if (sizeof (bivector_t) != 4 * sizeof (scalar_t)) {
		error ("bivector has wrong size: %d\n", sizeof (bivector_t));
	}
	if (sizeof (trivector_t) != sizeof (scalar_t)) {
		error ("trivector has wrong size: %d\n", sizeof (trivector_t));
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
		ret = 1;
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
		ret = 1;
	}

	if (test_wedge ()) {
		error ("wedge products failed\n");
	}

	if (test_dot ()) {
		error ("dot products failed\n");
	}

	if (test_geom ()) {
		error ("geometric products failed\n");
	}

	if (test_basics ()) {
		error ("basics failed\n");
	}

	return ret;
}
