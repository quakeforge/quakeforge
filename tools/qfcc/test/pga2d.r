#include "test-harness.h"
#pragma warn no-vararg-integer

#define error(fmt, ...) \
	do { \
		printf ("%s:%d: in %s: " fmt, __FILE__, __LINE__, __FUNCTION__ \
				__VA_OPT__(,) __VA_ARGS__); \
		ret = 1; \
	} while (false)

typedef @algebra(double(2,0,1)) PGA;
typedef PGA.group_mask(0x08) scalar_t;
typedef PGA.group_mask(0x01) vector_t;
typedef PGA.group_mask(0x04) bivector_t;
typedef PGA.group_mask(0x02) trivector_t;
typedef union {
	PGA.group_mask(0x0c) mvec;
	struct {
		bivector_t bvec;
		scalar_t   scalar;
	};
} evengrades_t;
typedef union {
	PGA.group_mask(0x03) mvec;
	struct {
		vector_t vec;
		trivector_t tvec;
	};
} oddgrades_t;

static int
test_dual (void)
{
	int ret = 0;
	@algebra (PGA) {
		auto a = 1f;
		auto a0 = e0;
		auto a1 = e1;
		auto a2 = e2;
		auto a12 = e12;
		auto a01 = e01;
		auto a20 = e20;
		auto a012 = e012;
#define TEST_DUAL(x, y) \
do { \
	if (⋆x != y) { \
		error ("⋆" #x " != " #y "\n"); \
	} \
} while (false)
		TEST_DUAL (a, e012);
		TEST_DUAL (a0, e12);
		TEST_DUAL (a1, e20);
		TEST_DUAL (a2, e01);
		TEST_DUAL (a20, e1);
		TEST_DUAL (a01, e2);
		TEST_DUAL (a12, e0);
		TEST_DUAL (a012, 1);
#undef TEST_DUAL

#define TEST_DUAL(x) \
do { \
	if (x * ⋆x != e012) { \
		error (#x " * ⋆" #x " != e012\n"); \
	} \
} while (false)
		TEST_DUAL (a);
		TEST_DUAL (a0);
		TEST_DUAL (a1);
		TEST_DUAL (a2);
		TEST_DUAL (a20);
		TEST_DUAL (a01);
		TEST_DUAL (a12);
		TEST_DUAL (a012);
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
		auto a12 = e12;
		auto a01 = e01;
		auto a20 = e20;
		auto a012 = e012;
#define TEST_UNDUAL(x, y) \
do { \
	if (@undual x != y) { \
		error ("@undual " #x " != " #y "\n"); \
	} \
} while (false)
		TEST_UNDUAL (a, e012);
		TEST_UNDUAL (a0, e12);
		TEST_UNDUAL (a1, e20);
		TEST_UNDUAL (a2, e01);
		TEST_UNDUAL (a20, e1);
		TEST_UNDUAL (a01, e2);
		TEST_UNDUAL (a12, e0);
		TEST_UNDUAL (a012, 1);
		#undef TEST_UNDUAL

#define TEST_UNDUAL(x) \
do { \
	if (@undual x * x != e012) { \
		error ("@undual " #x " * " #x " != e012\n"); \
	} \
} while (false)
		TEST_UNDUAL (a);
		TEST_UNDUAL (a0);
		TEST_UNDUAL (a1);
		TEST_UNDUAL (a2);
		TEST_UNDUAL (a20);
		TEST_UNDUAL (a01);
		TEST_UNDUAL (a12);
		TEST_UNDUAL (a012);
		#undef TEST_DUAL
	}
	return ret;
}

int
main (void)
{
	int ret = 0;
	if (sizeof (scalar_t) != sizeof (double)) {
		error ("scalar has wrong size: %d\n", sizeof (scalar_t));
	}
	// vector_t is a vec3 but has alignment of 4 thus sizeof of 4 * scalar
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		error ("vector has wrong size: %d\n", sizeof (vector_t));
	}
	// bivector_t is a vec3 but has alignment of 4 thus sizeof of 4 * scalar
	if (sizeof (bivector_t) != 4 * sizeof (scalar_t)) {
		error ("bivector has wrong size: %d\n", sizeof (bivector_t));
	}
	if (sizeof (trivector_t) != sizeof (scalar_t)) {
		error ("trivector has wrong size: %d\n", sizeof (trivector_t));
	}
	scalar_t scalar;
	vector_t vec, vecb;
	bivector_t bvec, bvecb;
	trivector_t tvec, tvecb;
	@algebra (PGA) {
		scalar = 42;
		vec = 3*e1 - 2*e2 + e0;
		bvec = 4*e20 - 3*e01 + 2*e12;
		tvec = 7*e012;

		vecb = 5*e1 + 12*e2 - 13*e0;
		bvecb = 6*e20 + 4*e01 + 1*e12;
		tvecb = 3*e012;
	}
	if (scalar != 42) {
		error ("scalar != 42: %g\n", scalar);
	}
	if ((dvec3)vec != '3 -2 1'd) {
		error ("vec != '3 -2 1': %lv\n", vec);
	}
	if ((dvec3)bvec != '4 -3 2'd) {
		error ("vec != '4 -3 2': %lv\n", bvec);
	}
	if ((double)tvec != 7) {
		error ("tvec != 7: %g\n", tvec);
	}
	auto a = vec ∧ bvec;
	if ((double)a != 20) {
		error ("vec ∧ bvec != 20: %lv\n", a);
	}
	auto b = bvec ∧ vec;
	if ((double)b != 20) {
		error ("bvec ∧ vec != 20: %lv\n", b);
	}
	auto c = vec • bvec;
	if ((dvec3)c != '4 6 1'd) {
		error ("vec • bvec != '4 6 1': %lv\n", c);
	}
	auto d = bvec • vec;
	if ((dvec3)d != '-4 -6 -1'd) {
		error ("bvec • vec != '-4 -6 -1': %lv\n", d);
	}
	oddgrades_t e;
	e.mvec = vec * bvec;
	if ((dvec3)e.vec != '4 6 1'd || (scalar_t)e.tvec != 20) {
		error ("vec • bvec != '4 6 1' + 20: %lv + %g\n", e.vec, e.tvec);
	}
	oddgrades_t f;
	f.mvec = bvec * vec;
	if ((dvec3)f.vec != '-4 -6 -1'd || (scalar_t)f.tvec != 20) {
		error ("bvec • vec != '-4 -6 -1' + 20: %lv %g\n", f.vec, f.tvec);
	}
	if (vec ∧ tvec || tvec ∧ vec) {
		error ("didn't get 0: %g %g", vec ∧ tvec, tvec ∧ vec);
		return 0;
	}
	auto g = vec • tvec;
	if ((dvec3)g != '21 -14 0'd) {
		error ("vec • tvec != '21 -14 0': %lv\n", g);
	}
	auto h = tvec • vec;
	if ((dvec3)h != '21 -14 0'd) {
		error ("vec • tvec != '21 -14 0': %lv\n", h);
	}
	auto i = vec * tvec;
	if ((dvec3)i != '21 -14 0'd) {
		error ("vec * tvec != '21 -14 0': %lv\n", i);
	}
	auto j = tvec * vec;
	if ((dvec3)j != '21 -14 0'd) {
		error ("vec * tvec != '21 -14 0': %lv\n", j);
	}
	if (bvec ∧ tvec || tvec ∧ bvec) {
		error ("didn't get 0: %g %g", bvec ∧ tvec, tvec ∧ bvec);
		return 0;
	}
	auto k = bvec • tvec;
	if ((dvec3)k != '0 0 -14'd) {
		error ("bvec • tvec != '0 0 -14': %lv\n", k);
	}
	auto l = tvec • bvec;
	if ((dvec3)l != '0 0 -14'd) {
		error ("tvec • bvec != '0 0 -14': %lv\n", l);
	}
	auto m = bvec * tvec;
	if ((dvec3)m != '0 0 -14'd) {
		error ("bvec * tvec != '0 0 -14': %lv\n", m);
	}
	auto n = tvec * bvec;
	if ((dvec3)n != '0 0 -14'd) {
		error ("tvec * bvec != '0 0 -14': %lv\n", n);
	}
//	if (vec ∧ vec || bvec ∧ bvec || tvec ∧ tvec) {
//		error ("didn't get 0: %g %g %g", vec ∧ vec, bvec ∧ bvec, tvec ∧ tvec);
//		return 0;
//	}
	auto o = vec ∧ vecb;
	if ((dvec3)o != '14 44 46'd) {
		error ("vec ∧ vecb != '14 44 46': %lv\n", o);
	}
	auto p = vecb ∧ vec;
	if ((dvec3)p != '-14 -44 -46'd) {
		error ("vecb ∧ vec != '-14 -44 -46': %lv\n", p);
	}
	auto q = vec • vecb;
	if (q != -9) {
		error ("vec • vecb != -9: %g\n", q);
	}
	auto r = vecb • vec;
	if (r != -9) {
		error ("vecb • vec != -9: %g\n", r);
	}
	evengrades_t s;
	s.mvec= vec * vecb;
	if (s.scalar != -9 || (dvec3)s.bvec != '14 44 46'd) {
		error ("vec * vecb != -9, '14 44 46': %g %lv\n",
				s.scalar, s.bvec);
	}
	evengrades_t t;
	t.mvec = vecb * vec;
	if (t.scalar != -9 || (dvec3)t.bvec != '-14 -44 -46'd) {
		error ("vecb * vec != -9, '-14 -44 -46': %g %lv\n",
				t.scalar, t.bvec);
	}
	if (bvec ∧ bvecb || tvec ∧ tvecb) {
		error ("didn't get 0: %g %g", bvec ∧ bvecb, tvec ∧ tvecb);
		return 0;
	}
	auto u = bvec • bvecb;
	if (u != -2) {
		error ("bvec • bvecb != -2: %g\n", u);
	}
	auto v = bvecb • bvec;
	if (v != -2) {
		error ("bvecb • bvec != -2: %g\n", v);
	}
	evengrades_t w;
	w.mvec = bvec * bvecb;
	if (w.scalar != -2 || (dvec3)w.bvec != '11 -8 0'd) {
		error ("bvec * bvecb != -2, '11 -8 0': %g %lv\n",
				w.scalar, w.bvec);
	}
	evengrades_t x;
	x.mvec = bvecb * bvec;
	if (x.scalar != -2 || (dvec3)x.bvec != '-11 8 0'd) {
		error ("vecb * vec != -2, '-11 8 0': %g %lv\n",
				x.scalar, x.bvec);
	}
	if (tvec • tvecb || tvec * tvecb) {
		error ("didn't get 0: %g %g", tvec • tvecb, tvec * tvecb);
		return 0;
	}
	e.mvec = e.mvec†;	// odd
	if ((dvec3)e.vec != '4 6 1'd || (scalar_t)e.tvec != -20) {
		error ("odd† != '4 6 1' + -20: %lv %g\n", e.vec, e.tvec);
	}
	s.mvec = s.mvec†;	// even
	if (s.scalar != -9 || (dvec3)s.bvec != '-14 -44 -46'd) {
		error ("even† != -9, '-14 -44 -46': %g %lv\n",
				s.scalar, s.bvec);
	}
	e.mvec = e.mvec / 2;	// odd
	if ((dvec3)e.vec != '2 3 0.5'd || (scalar_t)e.tvec != -10) {
		error ("odd† != '2 3 0.5' + -10: %lv %g\n", e.vec, e.tvec);
	}
	s.mvec = s.mvec / 2;	// even
	if (s.scalar != -4.5 || (dvec3)s.bvec != '-7 -22 -23'd) {
		error ("even† != -4.5, '-7 -22 -23': %g %lv\n",
				s.scalar, s.bvec);
	}
	e.mvec = -e.mvec;	// odd
	if ((dvec3)e.vec != '-2 -3 -0.5'd || (scalar_t)e.tvec != 10) {
		error ("odd† != '-2 -3 -0.5' + 10: %lv %g\n", e.vec, e.tvec);
	}
	s.mvec = -s.mvec;	// even
	if (s.scalar != 4.5 || (dvec3)s.bvec != '7 22 23'd) {
		error ("even† != 4.5, '7 22 23': %g %lv\n",
				s.scalar, s.bvec);
	}
	evengrades_t dual_e;
	dual_e.mvec = ⋆e.mvec;	// test both dual operators
	if ((dvec3)dual_e.bvec != '-2 -3 -0.5'd || dual_e.scalar != 10) {
		error ("⋆odd != '-2 -3 -0.5' + 10: %lv %g\n", e.vec, e.tvec);
	}
	oddgrades_t dual_s;
	dual_s.mvec = !s.mvec;	// test both dual operators
	if ((scalar_t)dual_s.tvec != 4.5 || (dvec3)dual_s.vec != '7 22 23'd) {
		error ("!even != 4.5, '7 22 23': %g %lv\n",
				s.scalar, s.bvec);
	}
	auto line = bvec ∨ bvecb;
	if ((dvec3)line != '-11 8 34'd) {
		error ("bvec(%lv) ∨ bvecb(%lv) != '-11 8 34': %lv\n", bvec, bvecb, line);
	}

	if (test_dual ()) {
		printf ("dual failed\n");
		ret = 1;
	}

	if (test_undual ()) {
		printf ("dual failed\n");
		ret = 1;
	}
	return ret;
}
