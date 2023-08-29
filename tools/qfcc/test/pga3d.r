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

static int
test_wedge (void)
{
	scalar_t scalar;
	vector_t vec, vecb, vecc, vecd;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	quadvector_t qvec, qvecb;
	@algebra (PGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + 4*e3 + e0;
		bvec.bvec = -3*e01 + 5*e02 + 7*e03 + 11*e23 - 4*e31 + 2*e12;
		tvec = 1*e032 + 4*e013 + 7*e021 - 2*e123;
		qvec = 8*e0123;

		vecb = 5*e1 + 12*e2 - 13*e0;
		bvecb.bvec = 6*e20 + 4*e01 + 1*e12;
		tvecb = 3*e032;
		qvecb = 1*e0123;

		vecc = 5*e1 + 4*e2 - 3*e3 + 4*e0;
		vecd = e1 + e2 + e3 + e0;
	}

	if (scalar != 42) {
		printf ("scalar != 42: %g\n", scalar);
		return 1;
	}
	if ((vec4)vec != '3 -2 4 1') {
		printf ("vec != '3 -2 4 1': %q\n", vec);
		return 1;
	}
	if ((vec3)bvec.dir != '11 -4 2' || (vec3)bvec.mom != '-3 5 7') {
		printf ("bvec != '11 -4 2' '-3 5 7': %v %v\n", bvec.dir, bvec.mom);
		return 1;
	}
	if ((vec4)tvec != '1 4 7 -2') {
		printf ("tvec != '1 4 7 -2': %g\n", tvec);
		return 1;
	}
	if ((scalar_t)qvec != 8) {
		printf ("tvec != 8: %g\n", (scalar_t) qvec);
		return 1;
	}

	bivector_t a = { .bvec = vec ∧ vecb };
	if ((vec3)a.dir != '-48 20 46' || (vec3)a.mom != '44 -14 52') {
		printf ("vec ∧ vecb != '-48 20 46' '44 -14 52': %v %v\n", a.dir, a.mom);
		return 1;
	}

	auto b = a.bvec ∧ vecc;
	if ((vec4)b != '358 -472 -430 -298') {
		printf ("a ∧ vecc != '358 -472 -430 -298': %v\n", b);
		return 1;
	}

	auto c = b ∧ vecd;
	if ((scalar_t)c != 842) {
		printf ("b ∧ vecd != 742': %g\n", (scalar_t) c);
		return 1;
	}

	a.bvec = vecb ∧ vec;
	if ((vec3)a.dir != '48 -20 -46' || (vec3)a.mom != '-44 14 -52') {
		printf ("vecb ∧ vec != '48 -20 -46' '-44 14 -52': %v %v\n",
				a.dir, a.mom);
		return 1;
	}

	b = vecc ∧ a.bvec;
	if ((vec4)b != '-358 472 430 298') {
		printf ("vecc ∧ a != '-358 472 430 298': %v\n", b);
		return 1;
	}

	c = vecd ∧ b;
	if ((scalar_t)c != 842) {
		printf ("vecd ^ b != 842': %g\n", (scalar_t) c);
		return 1;
	}

	c = a.bvec ∧ (vecc ∧ vecd);
	if ((scalar_t)c != -842) {
		printf ("a ∧ (vecc ∧ vecd) != -742': %g\n", (scalar_t) c);
		return 1;
	}

	c = (vecd ∧ vecc) ∧ a.bvec;
	if ((scalar_t)c != 842) {
		printf ("(vecd ∧ vecc) ∧ a != 742': %g\n", (scalar_t) c);
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
	quadvector_t qvec, qvecb;
	@algebra (PGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + 4*e3 + e0;
		bvec.bvec = -3*e01 + 5*e02 + 7*e03 + 11*e23 - 4*e31 + 2*e12;
		tvec = 1*e032 + 4*e013 + 7*e021 - 2*e123;
		qvec = 8*e0123;

		vecb = 5*e1 + 12*e2 - 13*e0;
		bvecb.bvec = 6*e20 + 4*e01 + 1*e12;
		tvecb = 3*e032;
		qvecb = 1*e0123;

		vecc = 5*e1 + 4*e2 - 3*e3 + 4*e0;
		vecd = e1 + e2 + e3 + e0;
	}

	if (qvec • qvecb != 0) {
		printf ("(qvec • qvecb) != 0': %g\n", qvec • qvecb);
		return 1;
	}

	auto a = tvec • qvec;
	if ((vec4)a != '0 0 0 -16') {
		printf ("vec • vecb != '0 0 0 -16': %v\n", a);
		return 1;
	}

	bivector_t b = { .mom = bvec.bvec • qvec };
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		printf ("bvec • qvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
		return 1;
	}

	auto c = vec • qvec;
	if ((vec4)c != '24 -16 32 0') {
		printf ("vec • qvec != '24 -16 32 0': %v\n", c);
		return 1;
	}

	a = bvec.bvec • tvec;
	if ((vec4)a != '22 -8 4 9') {
		printf ("bvec • tvec != '22 -8 4 9': %v\n", a);
		return 1;
	}

	b.bvec = vec • tvec;
	if ((vec3)b.dir != '-6 4 -8' || (vec3)b.mom != '30 17 -14') {
		printf ("vec • tvec != '-6 4 -8' '30 17 -14': %v %v\n", b.dir, b.mom);
		return 1;
	}

	if (bvec.bvec • bvec.bvec != -141) {
		printf ("(bvec • bvec) != -141': %g\n", bvec.bvec • bvec.bvec);
		return 1;
	}

	a = vec • bvec.bvec;
	if ((vec4)a != '-12 -38 -10 -9') {
		printf ("vec • bvec != '-12 -38 -10 -9': %v\n", a);
		return 1;
	}

	if (vec • vecb != -9) {
		printf ("(vec • vecb) != -9': %g\n", vec • vecb);
		return 1;
	}

	a = qvec • tvec;
	if ((vec4)a != '0 0 0 16') {
		printf ("vec • vecb != '0 0 0 16': %v\n", a);
		return 1;
	}

	b.dir = nil;//FIXME allow bvec = mom
	b.mom = qvec • bvec.bvec;
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		printf ("qvec • bvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
		return 1;
	}

	c = qvec • vec;
	if ((vec4)c != '-24 16 -32 0') {
		printf ("vec • qvec != '-24 16 -32 0': %v\n", c);
		return 1;
	}

	a = tvec • bvec.bvec;
	if ((vec4)a != '22 -8 4 9') {
		printf ("tvec • vec != '22 -8 4 9': %v\n", a);
		return 1;
	}

	b.bvec = tvec • vec;
	if ((vec3)b.dir != '-6 4 -8' || (vec3)b.mom != '30 17 -14') {
		printf ("tvec • vec != '-6 4 -8' '30 17 -14': %v %v\n", b.dir, b.mom);
		return 1;
	}

	a = bvec.bvec • vec;
	if ((vec4)a != '12 38 10 9') {
		printf ("bvec • vec != '12 38 10 9': %q\n", a);
		return 1;
	}

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

	if (test_wedge ()) {
		printf ("wedge products failed\n");
		return 1;
	}

	if (test_dot ()) {
		printf ("dot products failed\n");
		return 1;
	}

	return 0;		// to survive and prevail :)
}
