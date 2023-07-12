#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/mathlib.h"
#include "QF/mersenne.h"
#include "QF/sys.h"

#define SIZEOF(x) (sizeof(x) / sizeof(x[0]))

const vec3_t points[] = {
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
// epsilon. Yes, a rather insanely small triangle for a quake map. Probably
// due to qfbsp not culling the portal for being too small.
const vec3_t tears_triangle[] = {
	{2201.82007, -1262, -713.450012},
	{2201.8501,  -1262, -713.593994},
	{2201.84009, -1262, -713.445007},
};
const vec3_t tears_triangle2[] = {
	{-519.989014, 1111.94995, -2592.05005},
	{-520.002991, 1112,       -2592.02002},
	{-520.005005, 1112,       -2592.04004},
};
const vec3_t tears_triangle3[] = {
	{284,        624,        2959.76001},
	{284,        624,        2960},
	{284.234985, 624.471008, 2959.53003},
};

const vec3_t tears_quad[] = {
	{1792,       1155.53003, 240},
	{1792,       1030.32996, 240},
	{1792.01001, 1030.32996, 240},
	{1792.01001, 1155.53003, 240},
};

const vec3_t tears_quad2[] = {
	{304,         3248, 3208},
	{-61.2307014, 3248, 3208},
	{-61.2307014, 3248, 3192},
	{304,         3248, 3192},
};

const vec3_t tears_cluster[] = {
	{551.638,    -439.277008, 2804.63989},
	{551,        -438,        2806.02002},
	{555.999023, -447.998993, 2805.1499},
	{555.999023, -447.998993, 2804.32007},
	{551.638,    -439.277008, 2804.63989},
	{555.999023, -447.998993, 2804.32007},
	{555.999023, -447.998993, 2795.19995},
	{550.97699,  -437.954987, 2806.02002},
	{551,        -438,        2806.02002},
	{555.999023, -447.998993, 2795.19995},
	{555.999023, -447.998993, 2794.6499},
	{550.978027, -437.954987, 2806.02002},
	{545.945984, -442.945007, 2804.78003},
	{546,        -442.998993, 2804.77002},
	{546,        -443,        2804.69995},
	{546,        -443,        2804.78003},
	{545.945007, -442.945007, 2804.78003},
	{556,        -448,        2794.6499},
	{556,        -448,        2805.1499},
	{546,        -443,        2804.78003},
	{546,        -443,        2804.69995},
	{550.97699,  -437.954987, 2806.02002},
	{556,        -448,        2794.6499},
	{546.000977, -443,        2804.69995},
	{545.945007, -442.945007, 2804.78003},
};

const vec3_t tears_cluster2[] = {
	{-2160, -192,        -704.242004},
	{-2160, -192,        -662},
	{-2160, -196,        -658},
	{-2160, -220,        -646},
	{-2160, -222.332001, -645.416992},
	{-2160, -320,        -752},
	{-2160, -192,        -752},
	{-2160, -192,        -704.242004},
	{-2160, -222.332001, -645.416992},
	{-2160, -244,        -640},
	{-2160, -268,        -640},
	{-2160, -292,        -646},
	{-2160, -316,        -658},
	{-2160, -320,        -662},
	{-2160, -196,        -814},
	{-2160, -192,        -810},
	{-2160, -192,        -752},
	{-2160, -320,        -752},
	{-2160, -320,        -810},
	{-2160, -316,        -814},
	{-2160, -312,        -816},
	{-2160, -200,        -816},
	{-2174, -334,        -676},
	{-2186, -346,        -700},
	{-2192, -352,        -724},
	{-2192, -352,        -748},
	{-2186, -346,        -772},
	{-2174, -334,        -796},
	{-2160, -319.998993, -810},
	{-2160, -319.998993, -661.999023},
	{-2174, -178,        -796},
	{-2186, -166,        -772},
	{-2192, -160,        -748},
	{-2192, -160,        -724},
	{-2186, -166,        -700},
	{-2174, -178,        -676},
	{-2160, -192,        -662},
	{-2160, -192,        -810},
	{-2224, -320,        -810},
	{-2224, -330,        -800},
	{-2224, -320,        -800},
	{-2224, -200,        -816},
	{-2224, -312,        -816},
	{-2224, -316,        -814},
	{-2224, -320,        -810},
	{-2224, -320,        -800},
	{-2224, -192,        -800},
	{-2224, -192,        -810},
	{-2224, -196,        -814},
	{-2224, -192,        -800},
	{-2224, -182,        -800},
	{-2224, -192,        -810},
	{-2224, -330,        -800},
	{-2224, -334,        -796},
	{-2224, -178,        -796},
	{-2224, -182,        -800},
	{-2224, -334,        -796},
	{-2224, -346,        -772},
	{-2224, -351,        -752},
	{-2224, -161,        -752},
	{-2224, -166,        -772},
	{-2224, -178,        -796},
	{-2224, -351,        -752},
	{-2224, -352,        -748},
	{-2224, -352,        -724},
	{-2224, -351,        -720},
	{-2224, -161,        -720},
	{-2224, -160,        -724},
	{-2224, -160,        -748},
	{-2224, -161,        -752},
	{-2224, -351,        -720},
	{-2224, -346,        -700},
	{-2224, -334,        -676},
	{-2224, -316,        -658},
	{-2224, -292,        -646},
	{-2224, -268,        -640},
	{-2224, -244,        -640},
	{-2224, -220,        -646},
	{-2224, -196,        -658},
	{-2224, -178,        -676},
	{-2224, -166,        -700},
	{-2224, -161,        -720},
};

// random numbers dug this up. needed more than 6 iterations
const vec3_t cloud_points[] = {
	{1.70695281, 2.60889673, 2.86233163},
	{2.11825109, 2.59883952, 1.48193693},
	{2.76728868, 1.57646465, 2.39769602},
	{1.88795376, 2.73980999, 1.58031297},
	{2.79736757, 1.89582968, 1.80514407},
};

struct {
	const vec3_t *points;
	int         num_points;
	sphere_t    expect;
} tests[] = {
	{0,      0, {{ 0,  0, 0}, 0}},
	{points, 1, {{-1, -1, 1}, 0}},
	{points, 2, {{ 0,  0, 1}, 1.41421356}},
	{points, 3, {{-0.333333343, 0.333333343, 0.333333343}, 1.63299322}},
	{points, 4, {{0, 0, 0}, 1.73205081}},
	{tears_triangle, SIZEOF (tears_triangle),
		{{2201.84521, -1262, -713.519531}, 0.0747000724}},//FIXME numbers?
	{tears_cluster, SIZEOF (tears_cluster),
		{{552.678101, -443.460876, 2800.40405}, 8.04672813}},//FIXME numbers?
	{tears_quad, SIZEOF (tears_quad),
		{{1792.005, 1092.92999, 240}, 62.6000302}},
	{tears_triangle2, SIZEOF (tears_triangle2),
		{{-519.995972, 1111.97498, -2592.03516}, 0.0300767105}},//FIXME numbers?
	{tears_quad2, SIZEOF (tears_quad2),
		{{121.384659, 3248, 3200}, 182.790497}},
	{tears_triangle3, SIZEOF (tears_triangle3),
		{{284.117493, 624.235535, 2959.76489}, 0.352925777}},
	{tears_cluster2, SIZEOF (tears_cluster2),
		{{-2192, -256.000031, -736}, 103.479492}},//FIXME numbers?
	{cloud_points, SIZEOF (cloud_points),
		{{2.20056152, 2.23369908, 2.2332375}, 0.88327992}},
};
#define num_tests SIZEOF (tests)

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
	sphere_t    sphere;
	mtstate_t   mt;
	double      start, end;

	for (i = 0; i < num_tests; i ++) {
		sphere = SmallestEnclosingBall (tests[i].points, tests[i].num_points);
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
		vec3_t      cloud[10];
		sphere_t    seb;
		vec_t       r2;

		for (j = 0; j < 5; j++) {
			VectorSet (rnd (&mt), rnd (&mt), rnd (&mt), cloud[j]);
		}
		seb = SmallestEnclosingBall ((const vec_t (*)[3]) cloud, 5);
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
