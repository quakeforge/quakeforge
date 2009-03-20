#include "QF/va.h"

#include "getopt.h"
#include "world.h"

#undef DIST_EPSILON
#define DIST_EPSILON 0
#define ENABLE_BOXCLIP
#include "trace.c"

dclipnode_t clipnodes1[] = {
	{  0, { 1, -2}},
	{  1, {-2,  2}},
	{  2, {-2,  3}},
	{  3, { 4, -2}},
	{  4, { 5,  6}},
	{  5, {-2, -1}},
	{  6, { 7,  8}},
	{  7, {-1, -2}},
	{  8, { 9, 13}},
	{  9, {10, -1}},
	{ 10, {-1, 11}},
	{ 11, {12, -2}},
	{ 12, {-2, -1}},
	{ 13, {-1, -2}},
};

mplane_t planes1[] = {
	{{0, 1, 0}, -96, 1, 0},	//  0	wall at right
	{{0, 1, 0},  96, 1, 0},	//  1   wall at left (with ditch)
	{{1, 0, 0}, 128, 0, 0},	//  2   wall at front (with shelves)
	{{1, 0, 0}, -32, 0, 0},	//  3   wall at back
	{{0, 0, 1},  32, 2, 0},	//  4   top of shelves
	{{0, 0, 1}, 104, 2, 0},	//  5   ceiling
	{{0, 1, 0},  32, 1, 0},	//  6   edge of ditch
	{{0, 0, 1}, -24, 2, 0},	//  7   floor of ditch
	{{0, 0, 1},  24, 2, 0},	//  8   bottom of selves
	{{1, 0, 0},  64, 0, 0},	//  9   end of shelves
	{{0, 1, 0}, -16, 1, 0},	// 10   ditch side of 2nd shelf
	{{0, 1, 0}, -64, 1, 0},	// 11   ditch side of 1st shelf
	{{0, 1, 0}, -48, 1, 0},	// 12   1st shelf side of 2nd shelf
	{{0, 0, 1},   8, 2, 0},	// 13   main floor
};

hull_t hull1 = {
	clipnodes1,
	planes1,
	0,
	13,
	{0, 0, 0},
	{0, 0, 0},
};

dclipnode_t clipnodes2[] = {
	{0, {1, 12}},	// 0
	{1, {-1, 2}},	// 1
	{2, {3, 6}},	// 2
	{3, {-1, 4}},	// 3
	{4, {5, -1}},	// 4
	{5, {-1, -2}},	// 5
	{6, {7, 9}},	// 6
	{7, {-1, 8}},	// 7
	{4, {-2, -1}},	// 8
	{8, {10, -1}},	// 9
	{5, {-1, 11}},	// 10
	{4, {-2, -1}},	// 11
	{9, {13, 25}},	// 12
	{2, {-1, 14}},	// 13
	{6, {15, 19}},	// 14
	{10, {-1, 16}},	// 15
	{11, {17, 18}},	// 16
	{7, {-1, -2}},	// 17
	{4, {-2, -1}},	// 18
	{11, {20, 23}},	// 19
	{12, {-1, 21}},	// 20
	{8, {22, -1}},	// 21
	{5, {-1, -2}},	// 22
	{8, {24, -1}},	// 23
	{4, {-2, -1}},	// 24
	{3, {-1, 26}},	// 25
	{8, {27, -1}},	// 26
	{11, {28, -1}},	// 27
	{12, {-1, 29}},	// 28
	{13, {-2, -1}},	// 29
};

mplane_t planes2[] = {
	{{1, 0, 0}, 912, 0, 0, {0, 0}},						// 0	0
	{{1, 0, 0}, 1168, 0, 0, {0, 0}},					// 1	1
	{{0, 1, 0}, 2112, 1, 0, {0, 0}},					// 2	2 13
	{{0, 1, 0}, 2128, 1, 0, {0, 0}},					// 3	3 25
	{{0.229038, 0, 0.973417}, 18.3218, 5, 0, {0, 0}},	// 4	4 8 11 18 24
	{{0.229038, -0, 0.973417}, 57.2585, 5, 0, {0, 0}},	// 5	5 10 22
	{{0, 1, 0}, 1984, 1, 0, {0, 0}},					// 6	6 14
	{{0.229039, -0, 0.973417}, 33.8978, 5, 0, {0, 0}},	// 7	7 17
	{{0, 1, 0}, 1968, 1, 0, {0, 0}},					// 8	9 21 23 26
	{{1, 0, 0}, 736, 0, 0, {0, 0}},						// 9	12
	{{0, 0, 1}, -176, 2, 0, {0, 0}},					// 10	15
	{{0, 0, 1}, -192, 2, 0, {0, 0}},					// 11	16 19 27
	{{0, 0, 1}, -152, 2, 0, {0, 0}},					// 12	20 28
	{{1, 0, 0}, 720, 0, 0, {0, 0}},						// 13	29
};

hull_t hull2 = {
	clipnodes2,
	planes2,
	0,
	6,
	{0, 0, 0},
	{0, 0, 0},
};

dclipnode_t clipnodes3[] = {
	{0, {1, 12}},	// 0
	{1, {2, 7}},	// 1
	{2, {-2, 3}},	// 2
	{3, {4, -2}},	// 3
	{4, {-2, 5}},	// 4
	{5, {-2, 6}},	// 5
	{6, {-1, -2}},	// 6
	{7, {-2, 8}},	// 7
	{8, {9, -2}},	// 8
	{9, {10, 11}},	// 9
	{10, {-2, -1}},	// 10
	{6, {-1, -2}},	// 11
	{3, {13, -2}},	// 12
	{11, {14, -2}},	// 13
	{4, {-2, 15}},	// 14
	{2, {-2, 16}},	// 15
	{9, {-1, -2}},	// 16
};

mplane_t planes3[] = {
	{{0, 1, 0}, -224, 1, 0, {0, 0}},					// 0
	{{0, 1, 0}, -192, 1, 0, {0, 0}},					// 1
	{{0, 0, 1}, 192, 2, 0, {0, 0}},						// 2
	{{1, 0, 0}, 384, 0, 0, {0, 0}},						// 3
	{{1, 0, 0}, 576, 0, 0, {0, 0}},						// 4
	{{0, 1, 0}, -16, 1, 0, {0, 0}},						// 5
	{{-0, 0.242536, 0.970142}, -3.88057, 5, 0, {0, 0}},	// 6
	{{1, 0, 0}, 552, 0, 0, {0, 0}},						// 7
	{{1, 0, 0}, 408, 0, 0, {0, 0}},						// 8
	{{0, 0, 1}, 48, 2, 0, {0, 0}},						// 9
	{{0, 0, 1}, 160, 2, 0, {0, 0}},						// 10
	{{0, 1, 0}, -416, 1, 0, {0, 0}},					// 11
};

hull_t hull3 = {
	clipnodes3,
	planes3,
	0,
	6,
	{0, 0, 0},
	{0, 0, 0},
};

typedef struct {
	const char *desc;
	hull_t     *hull;
	vec3_t      start;
	vec3_t      end;
	struct {
		float       frac;
		qboolean    allsolid;
		qboolean    startsolid;
		qboolean    inopen;
		qboolean    inwater;
	}           expect;
} test_t;

test_t tests[] = {
	{"drop on trench edge",
		&hull1, {0, 47, 40}, {0, 47, 32}, {0.5, 0, 0, 1, 0}},
	{"drop past trench edge",
		&hull1, {0, 48, 40}, {0, 48, 32}, {1, 0, 0, 1, 0}},

	{"run into trench edge",
		&hull1, {0, 52, 35}, {0, 44, 35}, {0.5, 0, 0, 1, 0}},
	{"run over trench edge",
		&hull1, {0, 52, 36}, {0, 44, 36}, {1, 0, 0, 1, 0}},

	{"run into end of 2nd shelf",
		&hull1, {47, -32, 36}, {49, -32, 36}, {0.5, 0, 0, 1, 0}},
	{"run inside end of 2nd shelf",
		&hull1, {48, -32, 36}, {50, -32, 36}, {1, 1, 1, 0, 0}},

	{"run into end of 2nd shelf p2",
		&hull1, {44, -32, 59}, {52, -32, 59}, {0.5, 0, 0, 1, 0}},
	{"run over end of 2nd shelf",
		&hull1, {44, -32, 60}, {52, -32, 60}, {1, 0, 0, 1, 0}},

	{"drop past end of 2nd shelf",
		&hull1, {47, -32, 76}, {47, -32, 44}, {1, 0, 1, 1, 0}},
	{"drop onto end of 2nd shelf",
		&hull1, {48, -32, 76}, {48, -32, 44}, {0.5, 0, 1, 1, 0}},

	{"drop onto top of ramp: hull2",
		&hull2, {896, 2048, -144}, {896, 2048, -152}, {0.5, 0, 0, 1, 0}},

	{"drop onto edge of 2nd shelf p1",
		&hull1, {96, -47, 61}, {96, -47, 59}, {0.5, 0, 0, 1, 0}},

	{"drop onto edge of 2nd shelf p2",
		&hull1, {96, -47.9612, 61}, {96, -47.1025, 59}, {0.5, 0, 0, 1, 0}},

	{"drop onto edge of 2nd shelf p3",
		&hull1, {94.8916, -34.8506, 61}, {94.8146, -28.5696, 59},
		{0.5, 0, 0, 1, 0}},

	{"run over top of ramp: hull3",
		&hull3, {480, -216, 76}, {480, -200, 76}, {1, 0, 0, 1, 0}},

	{"drop onto top of ramp: hull2",
		&hull2, {468, -216, 80}, {468, -216, 72}, {0.5, 0, 0, 1, 0}},
};
const int num_tests = sizeof (tests) / sizeof (tests[0]);

int verbose = 0;

static trace_t
do_trace (hull_t *hull, vec3_t start, vec3_t end)
{
	trace_t trace;

	trace.allsolid = true;
	trace.startsolid = false;
	trace.inopen = false;
	trace.inwater = false;
	trace.fraction = 1;
	trace.extents[0] = 16;
	trace.extents[1] = 16;
	trace.extents[2] = 28;
	trace.isbox = true;
	VectorCopy (end, trace.endpos);
	MOD_TraceLine (hull, 0, start, end, &trace);
	return trace;
}

static int
run_test (test_t *test)
{
	const char *desc;
	vec3_t      end;
	int         res = 0;

	VectorSubtract (test->end, test->start, end);
	VectorMultAdd (test->start, test->expect.frac, end, end);
	if (verbose)
		printf ("expect: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
				" %3g %d %d %d %d\n",
				test->start[0], test->start[1], test->start[2],
				test->end[0], test->end[1], test->end[2],
				end[0], end[1], end[2],
				test->expect.frac, 
				test->expect.allsolid, test->expect.startsolid,
				test->expect.inopen, test->expect.inwater);
	trace_t trace = do_trace (test->hull, test->start, test->end);
	if (verbose)
		printf ("   got: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
				" %3g %d %d %d %d\n",
				test->start[0], test->start[1], test->start[2],
				test->end[0], test->end[1], test->end[2],
				trace.endpos[0], trace.endpos[1], trace.endpos[2],
				trace.fraction, 
				trace.allsolid, trace.startsolid,
				trace.inopen, trace.inwater);
	if (VectorCompare (end, trace.endpos)
		&& test->expect.frac == trace.fraction
		&& test->expect.allsolid == trace.allsolid
		&& test->expect.startsolid == trace.startsolid
		&& test->expect.inopen == trace.inopen
		&& test->expect.inwater == trace.inwater)
		res = 1;

	if (test->desc)
		desc = va ("(%d) %s", (int)(long)(test - tests), test->desc);
	else
		desc = va ("test #%d", (int)(long)(test - tests));
	printf ("%s: %s\n", res ? "PASS" : "FAIL", desc);
	return res;
}

int
main (int argc, char **argv)
{
	vec3_t      start, end;
	int         c, i;
	int         pass = 1;
	int         test = -1;

	while ((c = getopt (argc, argv, "vt:")) != EOF) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 't':
				test = atoi (optarg);
				break;
			default:
				fprintf (stderr,
						 "-v (verbose) and/or -t TEST (test number)\n");
				return 1;
		}
	}

	if (test == -1) {
		for (i = 0; i < num_tests; i++) {
			if (verbose && i)
				puts ("");
			pass &= run_test (&tests[i]);
		}
	} else if (test >= 0 && test < num_tests) {
		pass &= run_test (&tests[test]);
	} else {
		fprintf (stderr, "Bad test number (0 - %d)\n", num_tests);
		return 1;
	}

	for (i = 0; i < 0; i++) {
		start[0] = 480;
		start[1] = -240 + i;
		start[2] = 77;
		VectorCopy (start, end);
		end[2] -= 16;
		do_trace (&hull3, start, end);
	}
//	start[0] = 480;
//	start[1] = -207.253967;
//	start[2] = 76.03125;
//	VectorCopy (start, end);
//	end[1] += 16;
//	do_trace (&hull3, start, end);
	return !pass;
}
