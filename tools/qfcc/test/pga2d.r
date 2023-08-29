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
	// vector_t is a vec3 but has alignment of 4 thus sizeof of 4 * scalar
	if (sizeof (vector_t) != 4 * sizeof (scalar_t)) {
		printf ("vector has wrong size: %d\n", sizeof (vector_t));
		return 1;
	}
	// bivector_t is a vec3 but has alignment of 4 thus sizeof of 4 * scalar
	if (sizeof (bivector_t) != 4 * sizeof (scalar_t)) {
		printf ("bivector has wrong size: %d\n", sizeof (bivector_t));
		return 1;
	}
	if (sizeof (trivector_t) != sizeof (scalar_t)) {
		printf ("trivector has wrong size: %d\n", sizeof (trivector_t));
		return 1;
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
	auto a = vec ∧ bvec;
	if ((double)a != 20) {
		printf ("vec ∧ bvec != 20: %lv\n", a);
		return 1;
	}
	auto b = bvec ∧ vec;
	if ((double)b != 20) {
		printf ("bvec ∧ vec != 20: %lv\n", b);
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
	auto g = vec • tvec;
	if ((dvec3)g != '21 -14 0'd) {
		printf ("vec • tvec != '21 -14 0': %lv\n", g);
		return 1;
	}
	auto h = tvec • vec;
	if ((dvec3)h != '21 -14 0'd) {
		printf ("vec • tvec != '21 -14 0': %lv\n", h);
		return 1;
	}
	auto i = vec * tvec;
	if ((dvec3)i != '21 -14 0'd) {
		printf ("vec * tvec != '21 -14 0': %lv\n", i);
		return 1;
	}
	auto j = tvec * vec;
	if ((dvec3)j != '21 -14 0'd) {
		printf ("vec * tvec != '21 -14 0': %lv\n", j);
		return 1;
	}
	if (bvec ∧ tvec || tvec ∧ bvec) {
		printf ("didn't get 0: %g %g", bvec ∧ tvec, tvec ∧ bvec);
		return 0;
	}
	auto k = bvec • tvec;
	if ((dvec3)k != '0 0 -14'd) {
		printf ("bvec • tvec != '0 0 -14': %lv\n", k);
		return 1;
	}
	auto l = tvec • bvec;
	if ((dvec3)l != '0 0 -14'd) {
		printf ("tvec • bvec != '0 0 -14': %lv\n", l);
		return 1;
	}
	auto m = bvec * tvec;
	if ((dvec3)m != '0 0 -14'd) {
		printf ("bvec * tvec != '0 0 -14': %lv\n", m);
		return 1;
	}
	auto n = tvec * bvec;
	if ((dvec3)n != '0 0 -14'd) {
		printf ("tvec * bvec != '0 0 -14': %lv\n", n);
		return 1;
	}
//	if (vec ∧ vec || bvec ∧ bvec || tvec ∧ tvec) {
//		printf ("didn't get 0: %g %g %g", vec ∧ vec, bvec ∧ bvec, tvec ∧ tvec);
//		return 0;
//	}
	auto o = vec ∧ vecb;
	if ((dvec3)o != '14 44 46'd) {
		printf ("vec ∧ vecb != '14 44 46': %lv\n", o);
		return 1;
	}
	auto p = vecb ∧ vec;
	if ((dvec3)p != '-14 -44 -46'd) {
		printf ("vecb ∧ vec != '-14 -44 -46': %lv\n", p);
		return 1;
	}
	auto q = vec • vecb;
	if (q != -9) {
		printf ("vec • vecb != -9: %g\n", q);
		return 1;
	}
	auto r = vecb • vec;
	if (r != -9) {
		printf ("vecb • vec != -9: %g\n", r);
		return 1;
	}
	evengrades_t s;
	s.mvec= vec * vecb;
	if (s.scalar != -9 || (dvec3)s.bvec != '14 44 46'd) {
		printf ("vec * vecb != -9, '14 44 46': %g %lv\n",
				s.scalar, s.bvec);
		return 1;
	}
	evengrades_t t;
	t.mvec = vecb * vec;
	if (t.scalar != -9 || (dvec3)t.bvec != '-14 -44 -46'd) {
		printf ("vecb * vec != -9, '-14 -44 -46': %g %lv\n",
				t.scalar, t.bvec);
		return 1;
	}
	if (bvec ∧ bvecb || tvec ∧ tvecb) {
		printf ("didn't get 0: %g %g", bvec ∧ bvecb, tvec ∧ tvecb);
		return 0;
	}
	auto u = bvec • bvecb;
	if (u != -2) {
		printf ("bvec • bvecb != -2: %g\n", u);
		return 1;
	}
	auto v = bvecb • bvec;
	if (v != -2) {
		printf ("bvecb • bvec != -2: %g\n", v);
		return 1;
	}
	evengrades_t w;
	w.mvec = bvec * bvecb;
	if (w.scalar != -2 || (dvec3)w.bvec != '11 -8 0'd) {
		printf ("bvec * bvecb != -2, '11 -8 0': %g %lv\n",
				w.scalar, w.bvec);
		return 1;
	}
	evengrades_t x;
	x.mvec = bvecb * bvec;
	if (x.scalar != -2 || (dvec3)x.bvec != '-11 8 0'd) {
		printf ("vecb * vec != -2, '-11 8 0': %g %lv\n",
				x.scalar, x.bvec);
		return 1;
	}
	if (tvec • tvecb || tvec * tvecb) {
		printf ("didn't get 0: %g %g", tvec • tvecb, tvec * tvecb);
		return 0;
	}
	e.mvec = e.mvec†;	// odd
	if ((dvec3)e.vec != '4 6 1'd || (scalar_t)e.tvec != -20) {
		printf ("odd† != '4 6 1' + -20: %lv %g\n", e.vec, e.tvec);
		return 1;
	}
	s.mvec = s.mvec†;	// even
	if (s.scalar != -9 || (dvec3)s.bvec != '-14 -44 -46'd) {
		printf ("even† != -9, '-14 -44 -46': %g %lv\n",
				s.scalar, s.bvec);
		return 1;
	}
	e.mvec = e.mvec / 2;	// odd
	if ((dvec3)e.vec != '2 3 0.5'd || (scalar_t)e.tvec != -10) {
		printf ("odd† != '2 3 0.5' + -10: %lv %g\n", e.vec, e.tvec);
		return 1;
	}
	s.mvec = s.mvec / 2;	// even
	if (s.scalar != -4.5 || (dvec3)s.bvec != '-7 -22 -23'd) {
		printf ("even† != -4.5, '-7 -22 -23': %g %lv\n",
				s.scalar, s.bvec);
		return 1;
	}
	e.mvec = -e.mvec;	// odd
	if ((dvec3)e.vec != '-2 -3 -0.5'd || (scalar_t)e.tvec != 10) {
		printf ("odd† != '-2 -3 -0.5' + 10: %lv %g\n", e.vec, e.tvec);
		return 1;
	}
	s.mvec = -s.mvec;	// even
	if (s.scalar != 4.5 || (dvec3)s.bvec != '7 22 23'd) {
		printf ("even† != 4.5, '7 22 23': %g %lv\n",
				s.scalar, s.bvec);
		return 1;
	}
	evengrades_t dual_e;
	dual_e.mvec = ⋆e.mvec;	// test both dual operators
	if ((dvec3)dual_e.bvec != '-2 -3 -0.5'd || dual_e.scalar != 10) {
		printf ("⋆odd != '-2 -3 -0.5' + 10: %lv %g\n", e.vec, e.tvec);
		return 1;
	}
	oddgrades_t dual_s;
	dual_s.mvec = !s.mvec;	// test both dual operators
	if ((scalar_t)dual_s.tvec != 4.5 || (dvec3)dual_s.vec != '7 22 23'd) {
		printf ("!even != 4.5, '7 22 23': %g %lv\n",
				s.scalar, s.bvec);
		return 1;
	}
	auto line = bvec ∨ bvecb;
	if ((dvec3)line != '-11 8 34'd) {
		printf ("bvec ∨ bvecb != '-11 8 34': %lv\n", line);
		return 1;
	}
	return 0;		// to survive and prevail :)
}
