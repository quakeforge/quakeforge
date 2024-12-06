#include "test-harness.h"
#pragma warn no-vararg-integer

#define error(fmt, ...) \
	do { \
		printf ("%s:%d: in %s: " fmt, __FILE__, __LINE__, __FUNCTION__ \
				__VA_OPT__(,) __VA_ARGS__); \
		ret = 1; \
	} while (false)

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
	int ret = 0;
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
		error ("scalar != 42: %g\n", scalar);
	}
	if ((vec4)vec != '3 -2 4 1') {
		error ("vec != '3 -2 4 1': %q\n", vec);
	}
	if ((vec3)bvec.dir != '11 -4 2' || (vec3)bvec.mom != '-3 5 7') {
		error ("bvec != '11 -4 2' '-3 5 7': %v %v\n", bvec.dir, bvec.mom);
	}
	if ((vec4)tvec != '1 4 7 -2') {
		error ("tvec != '1 4 7 -2': %g\n", tvec);
	}
	if ((scalar_t)qvec != 8) {
		error ("tvec != 8: %g\n", qvec);
	}

	bivector_t a = { .bvec = vec ∧ vecb };
	if ((vec3)a.dir != '-48 20 46' || (vec3)a.mom != '44 -14 52') {
		error ("vec ∧ vecb != '-48 20 46' '44 -14 52': %v %v\n", a.dir, a.mom);
	}

	auto b = a.bvec ∧ vecc;
	if ((vec4)b != '358 -472 -430 -298') {
		error ("a ∧ vecc != '358 -472 -430 -298': %v\n", b);
	}

	auto c = b ∧ vecd;
	if ((scalar_t)c != 842) {
		error ("b ∧ vecd != 842': %g\n", c);
	}

	a.bvec = vecb ∧ vec;
	if ((vec3)a.dir != '48 -20 -46' || (vec3)a.mom != '-44 14 -52') {
		error ("vecb ∧ vec != '48 -20 -46' '-44 14 -52': %v %v\n",
			   a.dir, a.mom);
	}

	b = vecc ∧ a.bvec;
	if ((vec4)b != '-358 472 430 298') {
		error ("vecc ∧ a != '-358 472 430 298': %v\n", b);
	}

	c = vecd ∧ b;
	if ((scalar_t)c != 842) {
		error ("vecd ^ b != 842': %g\n", c);
	}

	c = a.bvec ∧ (vecc ∧ vecd);
	if ((scalar_t)c != -842) {
		error ("a ∧ (vecc ∧ vecd) != -842': %g\n", c);
	}

	c = (vecd ∧ vecc) ∧ a.bvec;
	if ((scalar_t)c != 842) {
		error ("(vecd ∧ vecc) ∧ a != 842': %g\n", c);
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
		error ("(qvec • qvecb) != 0': %g\n", qvec • qvecb);
	}

	auto a = tvec • qvec;
	if ((vec4)a != '0 0 0 -16') {
		error ("vec • vecb != '0 0 0 -16': %v\n", a);
	}

	bivector_t b = { .mom = bvec.bvec • qvec };
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		error ("bvec • qvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
	}

	auto c = vec • qvec;
	if ((vec4)c != '24 -16 32 0') {
		error ("vec • qvec != '24 -16 32 0': %v\n", c);
	}

	a = bvec.bvec • tvec;
	if ((vec4)a != '22 -8 4 9') {
		error ("bvec • tvec != '22 -8 4 9': %v\n", a);
	}

	b.bvec = vec • tvec;
	if ((vec3)b.dir != '-6 4 -8' || (vec3)b.mom != '30 17 -14') {
		error ("vec • tvec != '-6 4 -8' '30 17 -14': %v %v\n", b.dir, b.mom);
	}

	if (bvec.bvec • bvec.bvec != -141) {
		error ("(bvec • bvec) != -141': %g\n", bvec.bvec • bvec.bvec);
	}

	a = vec • bvec.bvec;
	if ((vec4)a != '-12 -38 -10 -9') {
		error ("vec • bvec != '-12 -38 -10 -9': %v\n", a);
	}

	if (vec • vecb != -9) {
		error ("(vec • vecb) != -9': %g\n", vec • vecb);
	}

	a = qvec • tvec;
	if ((vec4)a != '0 0 0 16') {
		error ("vec • vecb != '0 0 0 16': %v\n", a);
	}

	b.bvec = qvec • bvec.bvec;
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		error ("qvec • bvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
	}

	c = qvec • vec;
	if ((vec4)c != '-24 16 -32 0') {
		error ("vec • qvec != '-24 16 -32 0': %v\n", c);
	}

	a = tvec • bvec.bvec;
	if ((vec4)a != '22 -8 4 9') {
		error ("tvec • vec != '22 -8 4 9': %v\n", a);
	}

	b.bvec = tvec • vec;
	if ((vec3)b.dir != '-6 4 -8' || (vec3)b.mom != '30 17 -14') {
		error ("tvec • vec != '-6 4 -8' '30 17 -14': %v %v\n", b.dir, b.mom);
	}

	a = bvec.bvec • vec;
	if ((vec4)a != '12 38 10 9') {
		error ("bvec • vec != '12 38 10 9': %q\n", a);
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
		error ("(qvec * qvecb) != 0': %g\n", qvec * qvecb);
	}

	auto a = tvec * qvec;
	if ((vec4)a != '0 0 0 -16') {
		error ("vec * vecb != '0 0 0 -16': %v\n", a);
	}

	bivector_t b = { .mom = bvec.bvec * qvec };
	if ((vec3)b.dir != '0 0 0' || (vec3)b.mom != '-88 32 -16') {
		error ("bvec * qvec != '0 0 0' '-88 32 -16': %v %v\n", b.dir, b.mom);
	}

	auto c = vec * qvec;
	if ((vec4)c != '24 -16 32 0') {
		error ("vec * qvec != '24 -16 32 0': %v\n", c);
	}

	oddgrades_t d = { .mvec = bvec.bvec * tvec };
	if ((vec4)d.vec != '22 -8 4 9' || (vec4)d.tvec != '30 85 -34 0') {
		error ("bvec * tvec != '22 -8 4 9' '30 85 -34 0': %q %q\n",
			   d.vec, d.tvec);
	}

	evengrades_t e = { .mvec = vec * tvec };
	if ((vec3)e.dir != '-6 4 -8' || (vec3)e.mom != '30 17 -14'
		|| e.scalar || (scalar_t)e.qvec != 21) {
		error ("vec * tvec != 0 '-6 4 -8' '30 17 -14' 21: %g %v %v %g\n",
			   e.scalar, e.bvec.dir, e.bvec.mom, e.qvec);
	}

	e.mvec = bvec.bvec * bvec.bvec;
	if (e.scalar != -141 || (vec3)e.bvec.dir || (vec3)e.bvec.mom
		|| (scalar_t)e.qvec != -78) {
		error ("bvec * bvec != -141 '0 0 0' '0 0 0' -78: %g %v %v %g\n",
			   e.scalar, e.bvec.dir, e.bvec.mom, e.qvec);
	}

	d = { .mvec = vec * bvec.bvec };
	if ((vec4)d.vec != '-12 -38 -10 -9' || (vec4)d.tvec != '-45 -29 7 49') {
		error ("vec * bvec != '-12 -38 -10 -9' '-45 -29 7 49': %v %v\n",
			   d.vec, d.tvec);
	}

	e.mvec = vec * vecb;
	if (e.scalar != -9 || (scalar_t)e.qvec
		|| (vec3)e.bvec.dir != '-48 20 46' || (vec3)e.bvec.mom != '44 -14 52') {
		error ("(vec * vecb) != -9 '-48 20 46' '44 -14 52' 0': %g %v %v %g\n",
			   e.scalar, e.bvec.dir, e.bvec.mom, e.qvec);
	}

	a = qvec * tvec;
	if ((vec4)a != '0 0 0 16') {
		error ("vec * vecb != '0 0 0 16': %v\n", a);
	}

	e.mvec = qvec * bvec.bvec;
	if (e.scalar || (scalar_t)e.qvec
		|| (vec3)e.bvec.dir || (vec3)e.bvec.mom != '-88 32 -16') {
		error ("(vec * vecb) != 0 '0 0 0' '-88 32 -16' 0': %g %v %v %g\n",
			   e.scalar, e.bvec.dir, e.bvec.mom, e.qvec);
	}

	c = qvec * vec;
	if ((vec4)c != '-24 16 -32 0') {
		error ("qvec * vec != '-24 16 -32 0': %q\n", c);
	}

	d = { .mvec = tvec * bvec.bvec };
	if ((vec4)d.vec != '22 -8 4 9' || (vec4)d.tvec != '-30 -85 34 0') {
		error ("tvec * bvec != '22 -8 4 9' '-30 -85 34 0': %q %q\n",
			   d.vec, d.tvec);
	}

	e.mvec = tvec * vec;
	if (e.scalar || (scalar_t)e.qvec != -21
		|| (vec3)e.bvec.dir != '-6 4 -8' || (vec3)e.bvec.mom != '30 17 -14') {
		error ("tvec * vec != 0 '-6 4 -8' '30 17 -14' -21: %g %v %v %g\n",
			   e.scalar, e.bvec.dir, e.bvec.mom, e.qvec);
	}

	d.mvec = bvec.bvec * vec;
	if ((vec4)d.vec != '12 38 10 9' || (vec4)d.tvec != '-45 -29 7 49') {
		error ("vec * bvec != '12 38 10 9' '-45 -29 7 49': %q %q\n",
			   d.vec, d.tvec);
	}

	return ret;
}

static int
test_dual (void)
{
	int ret = 0;
	@algebra (PGA) {
		auto a = 1f;
		auto a0 = e0;
		auto a1 = e1;
		auto a2 = e2;
		auto a3 = e3;
		auto a23 = e23;
		auto a31 = e31;
		auto a12 = e12;
		auto a01 = e01;
		auto a02 = e02;
		auto a03 = e03;
		auto a032 = e032;
		auto a013 = e013;
		auto a021 = e021;
		auto a123 = e123;
		auto a0123 = e0123;
		#define TEST_DUAL(x, y) \
			if (⋆x != y) { \
				error ("⋆" #x " != " #y "\n"); \
			}
		TEST_DUAL (a, e0123);
		TEST_DUAL (a0, e123);
		TEST_DUAL (a1, e032);
		TEST_DUAL (a2, e013);
		TEST_DUAL (a3, e021);
		TEST_DUAL (a23, e01);
		TEST_DUAL (a31, e02);
		TEST_DUAL (a12, e03);
		TEST_DUAL (a01, e23);
		TEST_DUAL (a02, e31);
		TEST_DUAL (a03, e12);
		TEST_DUAL (a032, -e1);
		TEST_DUAL (a013, -e2);
		TEST_DUAL (a021, -e3);
		TEST_DUAL (a123, -e0);
		TEST_DUAL (a0123, 1);
		#undef TEST_DUAL

		#define TEST_DUAL(x) \
			if (x * ⋆x != e0123) { \
				error (#x " * ⋆" #x " != e0123\n"); \
			}
		TEST_DUAL (a);
		TEST_DUAL (a0);
		TEST_DUAL (a1);
		TEST_DUAL (a2);
		TEST_DUAL (a3);
		TEST_DUAL (a23);
		TEST_DUAL (a31);
		TEST_DUAL (a12);
		TEST_DUAL (a01);
		TEST_DUAL (a02);
		TEST_DUAL (a03);
		TEST_DUAL (a032);
		TEST_DUAL (a013);
		TEST_DUAL (a021);
		TEST_DUAL (a123);
		TEST_DUAL (a0123);
		#undef TEST_DUAL
	}
	return ret;
}

static int
test_undual (void)
{
	int ret = 0;
	@algebra (PGA) {
		auto a = 1f;
		auto a0 = e0;
		auto a1 = e1;
		auto a2 = e2;
		auto a3 = e3;
		auto a23 = e23;
		auto a31 = e31;
		auto a12 = e12;
		auto a01 = e01;
		auto a02 = e02;
		auto a03 = e03;
		auto a032 = e032;
		auto a013 = e013;
		auto a021 = e021;
		auto a123 = e123;
		auto a0123 = e0123;
		#define TEST_UNDUAL(x, y) \
			if (@undual x != y) { \
				error ("@undual" #x " != " #y "\n"); \
			}
		TEST_UNDUAL (a, e0123);
		TEST_UNDUAL (a0, -e123);
		TEST_UNDUAL (a1, -e032);
		TEST_UNDUAL (a2, -e013);
		TEST_UNDUAL (a3, -e021);
		TEST_UNDUAL (a23, e01);
		TEST_UNDUAL (a31, e02);
		TEST_UNDUAL (a12, e03);
		TEST_UNDUAL (a01, e23);
		TEST_UNDUAL (a02, e31);
		TEST_UNDUAL (a03, e12);
		TEST_UNDUAL (a032, e1);
		TEST_UNDUAL (a013, e2);
		TEST_UNDUAL (a021, e3);
		TEST_UNDUAL (a123, e0);
		TEST_UNDUAL (a0123, 1);
		#undef TEST_UNDUAL

		#define TEST_UNDUAL(x) \
			if (@undual x * x != e0123) { \
				error ("@undual " #x " * " #x " != e0123\n"); \
			}
		TEST_UNDUAL (a);
		TEST_UNDUAL (a0);
		TEST_UNDUAL (a1);
		TEST_UNDUAL (a2);
		TEST_UNDUAL (a3);
		TEST_UNDUAL (a23);
		TEST_UNDUAL (a31);
		TEST_UNDUAL (a12);
		TEST_UNDUAL (a01);
		TEST_UNDUAL (a02);
		TEST_UNDUAL (a03);
		TEST_UNDUAL (a032);
		TEST_UNDUAL (a013);
		TEST_UNDUAL (a021);
		TEST_UNDUAL (a123);
		TEST_UNDUAL (a0123);
		#undef TEST_UNDUAL
	}
	return ret;
}

static int
test_basics (void)
{
	int ret = 0;
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
			error ("plane1 or plane2 wrong: %q %q\n", plane1, plane2);
		}
		if ((vec4)p != '0 0 -3 1' || (vec4)p1 != '12 4 0 1'
			|| (vec4)p2 != '4 12 0 1') {
			error ("p or p1 or p2 wrong: %q %q %q\n", p, p1, p2);
		}
		bivector_t b = { .bvec = line };
		if ((vec3)b.dir != '0 0 1' || (vec3)b.mom != '-8 8 0') {
			error ("line is wrong: %v %v\n", b.dir, b.mom);
		}
		oddgrades_t o = { .mvec = point };
		if ((vec4)o.vec || (vec4)o.tvec != '-8 -8 -3 1') {
			error ("point is wrong: %q\n", point);
		}
		o.mvec = rot;
		if ((vec4)o.vec || (vec4)o.tvec != '-16 -16 -3 1') {
			error ("rot is wrong: %q %q\n", o.vec, o.tvec);
		}
		evengrades_t e = { .mvec = x };
		if (e.scalar || (scalar_t)e.qvec
			|| (vec3)e.dir != '-8 -8 0' || (vec3)e.mom != '-24 24 0') {
			error ("x is wrong: %g %v %v %g\n", e.scalar, e.dir, e.mom, e.qvec);
		}
	}
	return ret;
}

int
main (void)
{
	int ret = 0;
	if (sizeof (scalar_t) != sizeof (float)) {
		error ("scalar has wrong size: %d\n", sizeof (scalar_t));
	}
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		error ("bivector has wrong size: %d\n", sizeof (vector_t));
	}
	// the pair of vec3s in a bivector have an alignment of 4
	if (sizeof (bivector_t) != 8 * sizeof (scalar_t)) {
		error ("bivector has wrong size: %d\n", sizeof (bivector_t));
	}
	if (sizeof (bivector_t) != sizeof (PGA.group_mask(0x0a))) {
		error ("bivector group has wrong size: %d\n",
				sizeof (PGA.group_mask(0x0a)));
	}
	if (sizeof (trivector_t) != 4 * sizeof (scalar_t)) {
		error ("trivector has wrong size: %d\n", sizeof (trivector_t));
	}
	if (sizeof (quadvector_t) != sizeof (scalar_t)) {
		error ("quadvector has wrong size: %d\n", sizeof (quadvector_t));
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
		ret = 1;
	}

	if (test_wedge ()) {
		printf ("wedge products failed\n");
		ret = 1;
	}

	if (test_dot ()) {
		printf ("dot products failed\n");
		ret = 1;
	}

	if (test_geom ()) {
		printf ("geometric products failed\n");
		ret = 1;
	}

	if (test_dual ()) {
		printf ("dual failed\n");
		ret = 1;
	}

	if (test_undual ()) {
		printf ("dual failed\n");
		ret = 1;
	}

	if (test_basics ()) {
		printf ("basics failed\n");
		ret = 1;
	}

	return ret;
}
