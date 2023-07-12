#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"
#include "QF/mersenne.h"
#include "QF/simd/vec4f.h"
#include "QF/sys.h"

const vec4f_t points[] = {
	{-1, -1,  1},
	{ 1,  1,  1},
	{-1,  1, -1},
	{ 1, -1, -1},
	{-1, -1, -1},
	{ 1,  1, -1},
	{-1,  1,  1},
	{ 1, -1,  1},
	{ 0,  0,  0},
};

// This particular triangle from ad_tears caused SEB to hit its iteration
// limit because the center in affine hull test failed due to excessivly tight
// epsilon.
// However, the problem isn't here (but good to have a set of tests for it
// anyway).
const vec4f_t tears_triangleA[] = {
	{2201.82007, -1262, -713.450012, 1},
	{2201.8501, -1262, -713.593994, 1},
};
const vec4f_t tears_triangleB[] = {
	{2201.82007, -1262, -713.450012, 1},
	{2201.84009, -1262, -713.445007, 1},
};
const vec4f_t tears_triangleC[] = {
	{2201.8501, -1262, -713.593994, 1},
	{2201.84009, -1262, -713.445007, 1},
};

struct {
	const vec4f_t *points;
	int         num_points;
	sphere_t    expect;
} tests[] = {
	{0,      0, {{ 0,  0, 0}, 0}},
	{points, 1, {{-1, -1, 1}, 0}},
	{points, 2, {{ 0,  0, 1}, 1.41421356}},
	{points, 3, {{-0.333333343, 0.333333343, 0.333333343}, 1.63299322}},
	{points, 4, {{0, 0, 0}, 1.73205081}},
	{tears_triangleA, 2, {{2201.83496, -1262, -713.521973}, 0.0734853372}},
	{tears_triangleB, 2, {{2201.83008, -1262, -713.44751}, 0.0103178304}},
	{tears_triangleC, 2, {{2201.84521, -1262, -713.519531}, 0.0746228099}},
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
	return uf.f - 3.0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;
	int         j;
	vspheref_t  sphere;
	mtstate_t   mt;
	double      start, end;

	for (i = 0; i < num_tests; i ++) {
		sphere = CircumSphere_vf (tests[i].points, tests[i].num_points);
		if (VectorDistance_fast (sphere.center, tests[i].expect.center) > 1e-4
			|| fabs (sphere.radius - tests[i].expect.radius) > 1e-4) {
			res = 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: {%.9g, %.9g, %.9g},%.9g\n",
					VectorExpand (tests[i].expect.center),
					tests[i].expect.radius);
			printf ("got   : {%.9g, %.9g, %.9g},%.9g\n",
					VectorExpand (sphere.center),
					sphere.radius);
		}
	}

	mtwist_seed (&mt, 0);
	start = Sys_DoubleTime ();
	for (i = 0; !res && i < 1000000; i++) {
		vec4f_t     cloud[4];
		vspheref_t  cc;
		vec_t       r2;
		vec_t       fr;

		for (j = 0; j < 4; j++) {
			VectorSet (rnd (&mt), rnd (&mt), rnd (&mt), cloud[j]);
			cloud[j][3] = 1;
		}
		cc = CircumSphere_vf (cloud, 4);
		if (cc.radius < 0) {
			// degenerate
			continue;
		}
		r2 = cc.radius * cc.radius;
		for (j = 0; j < 4; j++) {
			vec4f_t     d = cloud[j] - cc.center;
			fr = dotf (d, d)[0];
			if (fabs (fr - r2) < 1e-3 * r2)
				continue;
			printf ("%zd %d %.9g - %.9g = %.9g %.9g\n",
					i, j, fr, r2, fr - r2, fabs(fr - r2));
			printf ("[%.9g %.9g %.9g] - [%.9g %.9g %.9g] = %.9g != %.9g\n",
					VectorExpand (cloud[j]), VectorExpand (cc.center), fr, r2);
			res = 1;
		}
	}
	end = Sys_DoubleTime ();
	printf ("%d itterations in %gs: %g itters/second\n", (int) i, end - start,
			i / (end - start));
	return res;
}
