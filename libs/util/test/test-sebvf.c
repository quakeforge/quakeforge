#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"
#include "QF/mersenne.h"
#include "QF/sys.h"
#include "QF/simd/vec4f.h"

const vec4f_t points[] = {
	{-1, -1,  1, 1},
	{ 1,  1,  1, 1},
	{-1,  1, -1, 1},
	{ 1, -1, -1, 1},
	{-1, -1, -1, 1},
	{ 1,  1, -1, 1},
	{-1,  1,  1, 1},
	{ 1, -1,  1, 1},
	{ 0,  0,  0, 1},
};

// This particular triangle from ad_tears caused SEB to hit its iteration
// limit because the center in affine hull test failed due to excessivly tight
// epsilon. Yes, a rather insanely small triangle for a quake map. Probably
// due to qfbsp not culling the portal for being too small.
const vec4f_t tears_triangle[] = {
	{2201.82007, -1262, -713.450012, 1},
	{2201.8501, -1262, -713.593994, 1},
	{2201.84009, -1262, -713.445007, 1},
};

struct {
	const vec4f_t *points;
	int         num_points;
	vspheref_t  expect;
} tests[] = {
	{0,      0, {{ 0,  0, 0, 1}, 0}},
	{points, 1, {{-1, -1, 1, 1}, 0}},
	{points, 2, {{ 0,  0, 1, 1}, 1.41421356}},
	{points, 3, {{-0.333333343, 0.333333343, 0.333333343, 1}, 1.63299322}},
	{points, 4, {{0, 0, 0, 1}, 1.73205081}},
	{tears_triangle, 3, {{2201.84521, -1262, -713.519531}, 0.0747000724}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

static inline float
rnd (mtstate_t *mt)
{
	union {
		uint32_t    u;
		float       f;
	}           uf;

	do {
		uf.u = mtwist_rand (mt) & 0x007fffff;
	} while (!uf.u);
	uf.u |= 0x40000000;
	return uf.f - 1.0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i, j;
	vspheref_t  sphere;
	mtstate_t   mt;
	double      start, end;

	for (i = 0; i < num_tests; i ++) {
		sphere = SmallestEnclosingBall_vf (tests[i].points,
										   tests[i].num_points);
		if (VectorDistance_fast (sphere.center, tests[i].expect.center) > 1e-4
			|| fabs (sphere.radius - tests[i].expect.radius) > 1e-4) {
			res = 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: {%.9g, %.9g, %.9g}, %.9g\n",
					VectorExpand (tests[i].expect.center),
					tests[i].expect.radius);
			printf ("got   : {%.9g, %.9g, %.9g}, %.9g\n",
					VectorExpand (sphere.center),
					sphere.radius);
		}
	}

	mtwist_seed (&mt, 0);
	start = Sys_DoubleTime ();
	for (i = 0; !res && i < 1000000; i++) {
		vec4f_t     cloud[10];
		vspheref_t  seb;
		vec_t       r2;

		for (j = 0; j < 5; j++) {
			VectorSet (rnd (&mt), rnd (&mt), rnd (&mt), cloud[j]);
		}
		seb = SmallestEnclosingBall_vf (cloud, 5);
		r2 = seb.radius * seb.radius;
		for (j = 0; j < 5; j++) {
			if (VectorDistance_fast (cloud[j], seb.center) - r2
				> 1e-5 * r2) {
				res = 1;
				printf ("%d %.9g - %.9g = %.9g\n", (int)j,
						VectorDistance_fast (cloud[j], seb.center), r2,
						VectorDistance_fast (cloud[j], seb.center) - r2);
				printf ("[%.9g %.9g %.9g] - [%.9g %.9g %.9g] = %.9g > %.9g\n",
						VectorExpand (cloud[j]), VectorExpand (seb.center),
						VectorDistance_fast (cloud[j], seb.center), r2);
			}
		}
	}
	end = Sys_DoubleTime ();
	printf ("%d iterations in %gs: %g iters/second\n", (int) i, end - start,
			i / (end - start));
	return res;
}
