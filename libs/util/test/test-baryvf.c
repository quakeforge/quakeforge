#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"
#include "QF/mersenne.h"
#include "QF/sys.h"
#include "QF/simd/vec4f.h"

vec4f_t points[] = {
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
const vec4f_t *line[] =  {&points[0], &points[1]};
const vec4f_t *tri[] =   {&points[0], &points[1], &points[2]};
const vec4f_t *tetra[] = {&points[0], &points[1], &points[2], &points[3]};

struct {
	const vec4f_t **points;
	int         num_points;
	vec4f_t    *x;
	vec_t       expect[4];
} tests[] = {
	{line, 2, &points[0], {1, 0}},
	{line, 2, &points[1], {0, 1}},
	{line, 2, &points[2], {0.5, 0.5}},
	{line, 2, &points[3], {0.5, 0.5}},
	{line, 2, &points[8], {0.5, 0.5}},
	{tri, 3, &points[0], {1, 0, 0}},
	{tri, 3, &points[1], {0, 1, 0}},
	{tri, 3, &points[2], {0, 0, 1}},
	{tri, 3, &points[3], {0.333333284, 0.333333333, 0.333333333}},//rounding :P
	{tri, 3, &points[8], {0.333333284, 0.333333333, 0.333333333}},//rounding :P
	{tetra, 4, &points[0], {1, 0, 0, 0}},
	{tetra, 4, &points[1], {0, 1, 0, 0}},
	{tetra, 4, &points[2], {0, 0, 1, 0}},
	{tetra, 4, &points[3], {0, 0, 0, 1}},
	{tetra, 4, &points[4], { 0.5, -0.5,  0.5,  0.5}},
	{tetra, 4, &points[5], {-0.5,  0.5,  0.5,  0.5}},
	{tetra, 4, &points[6], { 0.5,  0.5,  0.5, -0.5}},
	{tetra, 4, &points[7], { 0.5,  0.5, -0.5,  0.5}},
	{tetra, 4, &points[8], {0.25, 0.25, 0.25, 0.25}},
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
	vec4f_t     lambda;
	mtstate_t   mt;
	double      start, end;

	for (i = 0; i < num_tests; i ++) {
		lambda = BarycentricCoords_vf (tests[i].points, tests[i].num_points, *tests[i].x);
		for (j = 0; j < tests[i].num_points; j++) {
			if (tests[i].expect[j] != lambda[j])
				break;
		}
		if (j != tests[i].num_points) {
			res = 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect:");
			for (j = 0; j < tests[i].num_points; j++)
				printf (" %.9g", tests[i].expect[j]);
			printf ("\ngot   :");
			for (j = 0; j < tests[i].num_points; j++)
				printf (" %.9g", lambda[j]);
			printf ("\n");
		}
	}

	mtwist_seed (&mt, 0);
	start = Sys_DoubleTime ();
	for (i = 0; i < 1000000; i++) {
		vec4f_t     p = { rnd (&mt), rnd (&mt), rnd (&mt) };
		vec4f_t     x = {};
		lambda = BarycentricCoords_vf (tetra, 4, p);
		for (j = 0; j < 4; j++) {
			x = x + lambda[j] * *tetra[j];
		}
		if (VectorDistance_fast (x, p) > 1e-4) {
			res = 1;
			printf ("[%.9g %.9g %.9g] != [%.9g %.9g %.9g]: [%g %g %g %g]\n",
					VectorExpand (x), VectorExpand (p), QuatExpand (lambda));
			break;
		}
	}
	end = Sys_DoubleTime ();
	printf ("%d itterations in %gs: %g itters/second\n", (int) i, end - start,
			i / (end - start));
	return res;
}
