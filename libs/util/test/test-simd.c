#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"

#define right   { 1, 0, 0 }
#define forward { 0, 1, 0 }
#define up      { 0, 0, 1 }
#define one     { 1, 1, 1, 1 }
#define half    { 0.5, 0.5, 0.5, 0.5 }
#define zero    { 0, 0, 0, 0 }
#define qident  { 0, 0, 0, 1 }
#define qtest   { 0.64, 0.48, 0, 0.6 }

#define nright   { -1,  0,  0 }
#define nforward {  0, -1,  0 }
#define nup      {  0,  0, -1 }
#define none     { -1, -1, -1, -1 }
#define nqident  {  0,  0,  0, -1 }

#define s05 0.70710678118654757

typedef  struct {
	vec4d_t   (*op) (vec4d_t a, vec4d_t b);
	vec4d_t     a;
	vec4d_t     b;
	vec4d_t     expect;
	vec4d_t     ulp_errors;
} vec4d_test_t;

typedef  struct {
	vec4f_t   (*op) (vec4f_t a, vec4f_t b);
	vec4f_t     a;
	vec4f_t     b;
	vec4f_t     expect;
	vec4f_t     ulp_errors;
} vec4f_test_t;

static vec4d_t tvtruncd (vec4d_t v, vec4d_t ignore)
{
	return vtruncd (v);
}

static vec4d_t tvceild (vec4d_t v, vec4d_t ignore)
{
	return vceild (v);
}

static vec4d_t tvfloord (vec4d_t v, vec4d_t ignore)
{
	return vfloord (v);
}

static vec4d_t tqconjd (vec4d_t v, vec4d_t ignore)
{
	return qconjd (v);
}

static vec4f_t tvtruncf (vec4f_t v, vec4f_t ignore)
{
	return vtruncf (v);
}

static vec4f_t tvceilf (vec4f_t v, vec4f_t ignore)
{
	return vceilf (v);
}

static vec4f_t tvfloorf (vec4f_t v, vec4f_t ignore)
{
	return vfloorf (v);
}

static vec4f_t tqconjf (vec4f_t v, vec4f_t ignore)
{
	return qconjf (v);
}

static vec4d_test_t vec4d_tests[] = {
	// 3D dot products
	{ dotd, right,   right,   one  },
	{ dotd, right,   forward, zero },
	{ dotd, right,   up,      zero },
	{ dotd, forward, right,   zero },
	{ dotd, forward, forward, one  },
	{ dotd, forward, up,      zero },
	{ dotd, up,      right,   zero },
	{ dotd, up,      forward, zero },
	{ dotd, up,      up,      one  },

	// one is 4D, so its self dot product is 4
	{ dotd, one,     one,     { 4,  4,  4,  4} },
	{ dotd, one,    none,     {-4, -4, -4, -4} },

	// 3D cross products
	{ crossd, right,   right,    zero    },
	{ crossd, right,   forward,  up      },
	{ crossd, right,   up,      nforward },
	{ crossd, forward, right,   nup      },
	{ crossd, forward, forward,  zero    },
	{ crossd, forward, up,       right   },
	{ crossd, up,      right,    forward },
	{ crossd, up,      forward, nright   },
	{ crossd, up,      up,       zero    },
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	{ crossd, right,   one,     { 0, -1,  1} },
	{ crossd, forward, one,     { 1,  0, -1} },
	{ crossd, up,      one,     {-1,  1,  0} },
	{ crossd, one,     right,   { 0,  1, -1} },
	{ crossd, one,     forward, {-1,  0,  1} },
	{ crossd, one,     up,      { 1, -1,  0} },
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp errors in z and w
	{ crossd, qtest,   qtest,   {0, 0, 0, 0} },

	{ qmuld, qident,  qident,   qident  },
	{ qmuld, qident,  right,    right   },
	{ qmuld, qident,  forward,  forward },
	{ qmuld, qident,  up,       up      },
	{ qmuld, right,   qident,   right   },
	{ qmuld, forward, qident,   forward },
	{ qmuld, up,      qident,   up      },
	{ qmuld, right,   right,   nqident  },
	{ qmuld, right,   forward,  up      },
	{ qmuld, right,   up,      nforward },
	{ qmuld, forward, right,   nup      },
	{ qmuld, forward, forward, nqident  },
	{ qmuld, forward, up,       right   },
	{ qmuld, up,      right,    forward },
	{ qmuld, up,      forward, nright   },
	{ qmuld, up,      up,      nqident  },
	{ qmuld, one,     one,     { 2, 2, 2, -2 } },
	{ qmuld, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } },
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp error in z
	{ qmuld, qtest,   qtest,   {0.768, 0.576, 0, -0.28} },

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	{ qvmuld, one,    right,     { 0, 4, 0, 0 } },
	{ qvmuld, one,    forward,   { 0, 0, 4, 0 } },
	{ qvmuld, one,    up,        { 4, 0, 0, 0 } },
	{ qvmuld, one,    {1,1,1,0}, { 4, 4, 4, 0 } },
	{ qvmuld, one,    one,       { 4, 4, 4, -2 } },
	// The half vector is unit.
	{ qvmuld, half,   right,     forward },
	{ qvmuld, half,   forward,   up      },
	{ qvmuld, half,   up,        right   },
	{ qvmuld, half,   {1,1,1,0}, { 1, 1, 1, 0 } },
	// one is a 4D vector and qvmuld is meant for 3D vectors. However, it
	// seems that the vector's w has no effect on the 3d portion of the
	// result, but the result's w is cosine of the full rotation angle
	// scaled by quaternion magnitude and vector w
	{ qvmuld, half,   one,       { 1, 1, 1, -0.5 } },
	{ qvmuld, half,   {2,2,2,2}, { 2, 2, 2, -1 } },
	{ qvmuld, qtest,  right,     {0.5392, 0.6144, -0.576, 0} },
	{ qvmuld, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
	                             {0, -2.7e-17, 0, 0} },
	{ qvmuld, qtest,  up,        {0.576, -0.768, -0.28, 0} },

	{ qrotd, right,   right,    qident },
	{ qrotd, right,   forward,  {    0,    0,  s05,  s05 },
	                            {0, 0, -1.1e-16, 0} },
	{ qrotd, right,   up,       {    0, -s05,    0,  s05 },
	                            {0, 1.1e-16, 0, 0} },
	{ qrotd, forward, right,    {    0,    0, -s05,  s05 },
	                            {0, 0, 1.1e-16, 0} },
	{ qrotd, forward, forward,  qident },
	{ qrotd, forward, up,       {  s05,    0,    0,  s05 },
	                            {-1.1e-16, 0, 0, 0} },
	{ qrotd, up,      right,    {    0,  s05,    0,  s05 },
	                            {0, -1.1e-16, 0, 0} },
	{ qrotd, up,      forward,  { -s05,    0,    0,  s05 },
	                            { 1.1e-16, 0, 0, 0} },
	{ qrotd, up,      up,       qident },

	{ tvtruncd, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } },
	{ tvceild,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } },
	{ tvfloord, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } },
	{ tqconjd,  one, {}, { -1, -1, -1, 1 } },
};
#define num_vec4d_tests (sizeof (vec4d_tests) / (sizeof (vec4d_tests[0])))

static vec4f_test_t vec4f_tests[] = {
	// 3D dot products
	{ dotf, right,   right,   one  },
	{ dotf, right,   forward, zero },
	{ dotf, right,   up,      zero },
	{ dotf, forward, right,   zero },
	{ dotf, forward, forward, one  },
	{ dotf, forward, up,      zero },
	{ dotf, up,      right,   zero },
	{ dotf, up,      forward, zero },
	{ dotf, up,      up,      one  },

	// one is 4D, so its self dot product is 4
	{ dotf, one,     one,     { 4,  4,  4,  4} },
	{ dotf, one,    none,     {-4, -4, -4, -4} },

	// 3D cross products
	{ crossf, right,   right,    zero    },
	{ crossf, right,   forward,  up      },
	{ crossf, right,   up,      nforward },
	{ crossf, forward, right,   nup      },
	{ crossf, forward, forward,  zero    },
	{ crossf, forward, up,       right   },
	{ crossf, up,      right,    forward },
	{ crossf, up,      forward, nright   },
	{ crossf, up,      up,       zero    },
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	{ crossf, right,   one,     { 0, -1,  1} },
	{ crossf, forward, one,     { 1,  0, -1} },
	{ crossf, up,      one,     {-1,  1,  0} },
	{ crossf, one,     right,   { 0,  1, -1} },
	{ crossf, one,     forward, {-1,  0,  1} },
	{ crossf, one,     up,      { 1, -1,  0} },
	{ crossf, qtest,   qtest,   {0, 0, 0, 0} },

	{ qmulf, qident,  qident,   qident  },
	{ qmulf, qident,  right,    right   },
	{ qmulf, qident,  forward,  forward },
	{ qmulf, qident,  up,       up      },
	{ qmulf, right,   qident,   right   },
	{ qmulf, forward, qident,   forward },
	{ qmulf, up,      qident,   up      },
	{ qmulf, right,   right,   nqident  },
	{ qmulf, right,   forward,  up      },
	{ qmulf, right,   up,      nforward },
	{ qmulf, forward, right,   nup      },
	{ qmulf, forward, forward, nqident  },
	{ qmulf, forward, up,       right   },
	{ qmulf, up,      right,    forward },
	{ qmulf, up,      forward, nright   },
	{ qmulf, up,      up,      nqident  },
	{ qmulf, one,     one,     { 2, 2, 2, -2 } },
	{ qmulf, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } },
	{ qmulf, qtest,   qtest,   {0.768, 0.576, 0, -0.28},
	                           {0, 6e-8, 0, 3e-8} },

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	{ qvmulf, one,    right,     { 0, 4, 0, 0 } },
	{ qvmulf, one,    forward,   { 0, 0, 4, 0 } },
	{ qvmulf, one,    up,        { 4, 0, 0, 0 } },
	{ qvmulf, one,    {1,1,1,0}, { 4, 4, 4, 0 } },
	{ qvmulf, one,    one,       { 4, 4, 4, -2 } },
	{ qvmulf, qtest,  right,     {0.5392, 0.6144, -0.576, 0},
	                             {0, -5.9e-08, -6e-8, 0} },
	{ qvmulf, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
	                             {-5.9e-08, 1.5e-08, 0, 0} },
	{ qvmulf, qtest,  up,        {0.576, -0.768, -0.28, 0},
	                             {6e-8, 0, 3e-8, 0} },

	{ qrotf, right,   right,    qident },
	{ qrotf, right,   forward,  {    0,    0,  s05,  s05 } },
	{ qrotf, right,   up,       {    0, -s05,    0,  s05 } },
	{ qrotf, forward, right,    {    0,    0, -s05,  s05 } },
	{ qrotf, forward, forward,  qident },
	{ qrotf, forward, up,       {  s05,    0,    0,  s05 } },
	{ qrotf, up,      right,    {    0,  s05,    0,  s05 } },
	{ qrotf, up,      forward,  { -s05,    0,    0,  s05 } },
	{ qrotf, up,      up,       qident },

	{ tvtruncf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } },
	{ tvceilf,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } },
	{ tvfloorf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } },
	{ tqconjf,  one, {}, { -1, -1, -1, 1 } },
};
#define num_vec4f_tests (sizeof (vec4f_tests) / (sizeof (vec4f_tests[0])))

static int
run_vec4d_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4d_tests; i++) {
		__auto_type test = &vec4d_tests[i];
		vec4d_t     result = test->op (test->a, test->b);
		vec4d_t     expect = test->expect + test->ulp_errors;
		vec4l_t     res = result != expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4d_tests\n");
			printf ("a: " VEC4D_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4D_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4D_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4L_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4D_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4D_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4D_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_vec4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4f_tests; i++) {
		__auto_type test = &vec4f_tests[i];
		vec4f_t     result = test->op (test->a, test->b);
		vec4f_t     expect = test->expect + test->ulp_errors;
		vec4i_t     res = result != expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4f_tests\n");
			printf ("a: " VEC4F_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4F_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4F_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4I_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4F_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4F_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4F_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

int
main (void)
{
	int         ret = 0;
	ret |= run_vec4d_tests ();
	ret |= run_vec4f_tests ();
	return ret;
}
