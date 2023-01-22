#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include "QF/va.h"

#include "getopt.h"
#include "world.h"

#include "hulls.h"

#undef DIST_EPSILON
#define DIST_EPSILON 0

#ifdef TEST_ID
# include "trace-id.c"
#elif defined(TEST_QF_BAD)
# include "trace-qf-bad.c"
#else
# include "../trace.c"
#endif

typedef struct {
	vec3_t      extents;
} box_t;

typedef struct {
	const char *desc;
	box_t      *box;
	hull_t     *hull;
	vec3_t      origin;
	struct {
		int         contents;
		unsigned    contents_flags;
	}           expect;
} test_t;

box_t point = { {0, 0, 0} };
box_t box =   { {8, 8, 8} };
box_t player = { {16, 16, 28} };

test_t tests[] = {
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-33, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-32, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-31, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 31, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 32, 0, 0}, { CONTENTS_EMPTY, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 48, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 49, 0, 0}, { CONTENTS_SOLID, 0}},

	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{-40, 0, 0}, { CONTENTS_SOLID, 2}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{-32, 0, 0}, { CONTENTS_SOLID, 2}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 24, 0, 0}, { CONTENTS_SOLID, 2}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 31, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 32, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 33, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 40, 0, 0}, { CONTENTS_EMPTY, 1}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 48, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 49, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes 1", &box, &hull_tpp1,
		{ 56, 0, 0}, { CONTENTS_SOLID, 2}},

	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{-33, 0, 0}, { CONTENTS_SOLID, 0}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{-32, 0, 0}, { CONTENTS_EMPTY, 0}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 32, 0, 0}, { CONTENTS_WATER, 0}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 48, 0, 0}, { CONTENTS_SOLID, 0}},

	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{-32, 0, 0}, { CONTENTS_SOLID, 3}},
	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{  0, 0, 0}, { CONTENTS_EMPTY, 1}},
	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{ 32, 0, 0}, { CONTENTS_WATER, 5}},
	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{ 48, 0, 0}, { CONTENTS_SOLID, 6}},

	{"Box, ramp", &box, &hull_ramp,
		{ 0, 0, 8}, { CONTENTS_EMPTY, 1}},
	{"Box, ramp", &box, &hull_ramp,
		{ 0, 0, 7}, { CONTENTS_SOLID, 3}},

	{"Box, ridge", &box, &hull_ridge,
		{ 4, 0, 40}, { CONTENTS_EMPTY, 1}},
	{"Box, ridge", &box, &hull_ridge,
		{ 4, 0, 39}, { CONTENTS_SOLID, 3}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int verbose = 0;

static int
do_contents (box_t *box, hull_t *hull, vec3_t origin, trace_t *trace)
{
	memset (trace, 0xff, sizeof (*trace));
	VectorCopy (box->extents, trace->extents);
	// FIXME specify trace type in test spec
	trace->type = box == &point ? tr_point : tr_box;
	return MOD_HullContents (hull, 0, origin, trace);
}

static int
run_test (test_t *test)
{
	const char *desc;
	int         res = 0;
	char       *expect;
	char       *got;
	static int  output = 0;
	trace_t     trace;
	int         contents;

	if (!test->hull->nodeleafs)
		test->hull->nodeleafs = MOD_BuildBrushes (test->hull);

	expect = nva ("expect: (%g %g %g) => %3d %08x",
				  VectorExpand (test->origin),
				  test->expect.contents, test->expect.contents_flags);
	contents = do_contents (test->box, test->hull, test->origin, &trace);
	got = nva ("   got: (%g %g %g) => %3d %08x",
			   VectorExpand (test->origin),
			   contents, trace.contents);
	if (test->expect.contents == contents
		&& test->expect.contents_flags == trace.contents)
		res = 1;

	if (test->desc)
		desc = va (0, "(%d) %s", (int)(long)(test - tests), test->desc);
	else
		desc = va (0, "test #%d", (int)(long)(test - tests));
	if (verbose >= 0 || !res) {
		if (output)
			puts("");
		output = 1;
		puts (expect);
		puts (got);
		printf ("%s: %s\n", res ? "PASS" : "FAIL", desc);
	}
	free (expect);
	free (got);
	return res;
}

#include "main.c"
