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
	bivector_t bvec;
	struct {
		PGA.group_mask(0x02) dir;
		scalar_t   scalar;
		PGA.group_mask(0x08) mom;
		quadvector_t qvec;
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

	b.bvec = qvec • bvec.bvec;
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

static int
test_geom (void)
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

	if (qvec * qvecb != 0) {
		printf ("(qvec * qvecb) != 0': %g\n", qvec * qvecb);
		return 1;
	}

	auto a = tvec * qvec;
	if ((vec4)a != '0 0 0 -16') {
		printf ("vec * vecb != '0 0 0 -16': %v\n", a);
		return 1;
	}

	bivector_t b = { .mom = bvec.bvec * qvec };
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		printf ("bvec * qvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
		return 1;
	}

	auto c = vec * qvec;
	if ((vec4)c != '24 -16 32 0') {
		printf ("vec * qvec != '24 -16 32 0': %v\n", c);
		return 1;
	}

	oddgrades_t d = { .mvec = bvec.bvec * tvec };
	if ((vec4)d.vec != '22 -8 4 9' || (vec4)d.tvec != '30 85 -34 0') {
		printf ("bvec * tvec != '22 -8 4 9' '30 85 -34 0': %q %q\n",
				d.vec, d.tvec);
		return 1;
	}

	evengrades_t e = { .mvec = vec * tvec };
	if ((vec3)e.dir != '-6 4 -8' || (vec3)e.mom != '30 17 -14'
		|| e.scalar || (scalar_t)e.qvec != 21) {
		printf ("vec * tvec != 0 '-6 4 -8' '30 17 -14' 21: %g %v %v %g\n",
				e.scalar, e.bvec.dir, e.bvec.mom, (scalar_t) e.qvec);
		return 1;
	}

	e.mvec = bvec.bvec * bvec.bvec;
	if (e.scalar != -141 || (vec3)e.bvec.dir || (vec3)e.bvec.mom
		|| (scalar_t)e.qvec != -78) {
		printf ("bvec * bvec != -141 '0 0 0' '0 0 0' -78: %g %v %v %g\n",
				e.scalar, e.bvec.dir, e.bvec.mom, (scalar_t)e.qvec);
		return 1;
	}

	d = { .mvec = vec * bvec.bvec };
	if ((vec4)d.vec != '-12 -38 -10 -9' || (vec4)d.tvec != '-45 -29 7 49') {
		printf ("vec * bvec != '-12 -38 -10 -9' '-45 -29 7 49': %v %v\n",
				d.vec, d.tvec);
		return 1;
	}

	e.mvec = vec * vecb;
	if (e.scalar != -9 || (scalar_t)e.qvec
		|| (vec3)e.bvec.dir != '-48 20 46' || (vec3)e.bvec.mom != '44 -14 52') {
		printf ("(vec * vecb) != -9 '-48 20 46' '44 -14 52' 0': %g %v %v %g\n",
				e.scalar, e.bvec.dir, e.bvec.mom, (scalar_t) e.qvec);
		return 1;
	}

	a = qvec * tvec;
	if ((vec4)a != '0 0 0 16') {
		printf ("vec * vecb != '0 0 0 16': %v\n", a);
		return 1;
	}

	e.mvec = qvec * bvec.bvec;
	if (e.scalar || (scalar_t)e.qvec
		|| (vec3)e.bvec.dir || (vec3)e.bvec.mom != '-88 32 -16') {
		printf ("(vec * vecb) != 0 '0 0 0' '-88 32 -16' 0': %g %v %v %g\n",
				e.scalar, e.bvec.dir, e.bvec.mom, (scalar_t) e.qvec);
		return 1;
	}

	c = qvec * vec;
	if ((vec4)c != '-24 16 -32 0') {
		printf ("qvec * vec != '-24 16 -32 0': %q\n", c);
		return 1;
	}

	d = { .mvec = tvec * bvec.bvec };
	if ((vec4)d.vec != '22 -8 4 9' || (vec4)d.tvec != '-30 -85 34 0') {
		printf ("vec * bvec != '22 -8 4 9' '-30 -85 34 0': %q %q\n",
				d.vec, d.tvec);
		return 1;
	}

	e.mvec = tvec * vec;
	if (e.scalar || (scalar_t)e.qvec != -21
		|| (vec3)e.bvec.dir != '-6 4 -8' || (vec3)e.bvec.mom != '30 17 -14') {
		printf ("tvec * vec != 0 '-6 4 -8' '30 17 -14' -21: %g %v %v %g\n",
				e.scalar, e.bvec.dir, e.bvec.mom, (scalar_t) e.qvec);
		return 1;
	}

	d.mvec = bvec.bvec * vec;
	if ((vec4)d.vec != '12 38 10 9' || (vec4)d.tvec != '-45 -29 7 49') {
		printf ("vec * bvec != '12 38 10 9' '-45 -29 7 49': %q %q\n",
				d.vec, d.tvec);
		return 1;
	}

	return 0;
}

static int
test_basics (void)
{
	@algebra (PGA) {
		auto plane1 = e1 + 8*e0;
		auto plane2 = e2 + 8*e0;
		auto line = plane1 @wedge plane2;
		auto p = -3*e021 + e123;
		auto point = line @dot p * ~line;
		auto rot = line * p * ~line;
		auto p1 = 12*e032 + 4*e013 + e123;
		auto p2 = 4*e032 + 12*e013 + e123;
		auto x = point @regressive rot;

		if ((vec4)plane1 != '1 0 0 8' || (vec4)plane2 != '0 1 0 8') {
			printf ("plane1 or plane2 wrong: %q %q\n", plane1, plane2);
			return 1;
		}
		if ((vec4)p != '0 0 -3 1' || (vec4)p1 != '12 4 0 1'
			|| (vec4)p2 != '4 12 0 1') {
			printf ("p or p1 or p2 wrong: %q %q %q\n", p, p1, p2);
			return 1;
		}
		bivector_t b = { .bvec = line };
		if ((vec3)b.dir != '0 0 1' || (vec3)b.mom != '-8 8 0') {
			printf ("line is wrong: %v %v\n", b.dir, b.mom);
			return 1;
		}
		oddgrades_t o = { .mvec = point };
		if ((vec4)o.vec || (vec4)o.tvec != '-8 -8 -3 1') {
			printf ("point is wrong: %q\n", point);
			return 1;
		}
		o.mvec = rot;
		if ((vec4)o.vec || (vec4)o.tvec != '-16 -16 -3 1') {
			printf ("rot is wrong: %q %q\n", o.vec, o.tvec);
			return 1;
		}
		evengrades_t e = { .mvec = x };
		if (e.scalar || (scalar_t)e.qvec
			|| (vec3)e.dir != '-8 -8 0' || (vec3)e.mom != '-24 24 0') {
			printf ("x is wrong: %g %v %v %g\n",
					e.scalar, e.dir, e.mom, (scalar_t)e.qvec);
			return 1;
		}
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
	if (sizeof (evengrades_t) != sizeof (bivector_t)) {
		evengrades_t e;
		#define offsetof(s,f) ((int)(&((s*)0).f))
		printf ("evengrades has wrong size: %d\n", sizeof (evengrades_t));
		printf ("mvec: %d, bvec: %d, scalar: %d, qvec: %d\n",
				sizeof (e.mvec), sizeof (e.bvec),
				sizeof (e.scalar), sizeof (e.qvec));
		printf ("mvec: %d, bvec: %d, scalar: %d, qvec: %d\n",
				offsetof (evengrades_t, mvec), offsetof (evengrades_t, bvec),
				offsetof (evengrades_t, scalar), offsetof (evengrades_t, qvec));
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
