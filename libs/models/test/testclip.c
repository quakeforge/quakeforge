#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include "QF/va.h"

#include "getopt.h"
#include "mod_internal.h"
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

#define FloatCompare(a, b) (fabs (a - b) < 1e-5)
#undef VectorCompare
#define VectorCompare(x, y)									\
	(FloatCompare (x[0], y[0]) && FloatCompare (x[1], y[1])	\
	 && FloatCompare (x[2], y[2]))


typedef struct {
	vec3_t      extents;
} box_t;

typedef struct {
	const char *desc;
	box_t      *box;
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

box_t point =  { { 0,  0,  0} };
box_t box =    { { 8,  8,  8} };
box_t bigbox = { { 8,  8, 24} };
box_t player = { {16, 16, 28} };

test_t tests[] = {
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 1, 0}},

	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0}},

	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{-64, 0, 0}, { 64, 0, 0}, { 0.875, 0, 1, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, {-88, 0, 0}, {0.5625, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 0, 0, 1, 0}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 0, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1}},
	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{-32, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 1}},
	{"Box, Three parallel planes with water", &box, &hull_tppw,
		{-32, 0, 0}, { 20, 0, 0}, {     1, 0, 1, 1, 0}},

	{"Point, Step 1", &point, &hull_step1,
		{ -16, 0, 8}, {16, 0,  24}, {  0.5, 0, 0, 1, 0}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0,  24}, { 0.25, 0, 0, 1, 0}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0,  40}, { 0.25, 0, 0, 1, 0}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 135}, { 0.25, 0, 0, 1, 0}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 136}, {    1, 0, 0, 1, 0}},

	{"Point, Covered Step", &point, &hull_covered_step,
		{ -24, 0,   8}, {-24, 0, 72}, {   0.5, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -32, 0,   8}, {-32, 0, 72}, { 0.375, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -24, 0,   8}, {-24, 0, 72}, { 0.375, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -25, 0,   8}, {  7, 0, 72}, { 0.375, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{  -8, 0,  40}, {-16, 0, 40}, {   0.5, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -17, 0,   8}, { -1, 0, 72}, {     1, 0, 0, 1, 0}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{  -8, 0,  40}, {  8, 0, 72}, {     1, 0, 0, 1, 0}},
	{"Box, Covered Step touch backside", &box, &hull_covered_step,
		{  -8, 0,   8}, {-12, 0, 12}, {     1, 0, 0, 1, 0}},
	{"Point, Covered Step touch backside", &point, &hull_covered_step,
		{   0, 0,   8}, {-12, 0, 12}, {     1, 0, 1, 1, 0}},
	{"Point, Covered Step start solid", &point, &hull_covered_step,
		{   0, 0,  -4}, {-16, 0, 12}, {     1, 0, 1, 1, 0}},
	{"Point, Covered Step start solid", &point, &hull_covered_step,
		{   4, 0,  -4}, {-12, 0, 12}, {     1, 1, 1, 0, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{  -4, 0,   4}, {-12, 0, 12}, {     1, 0, 1, 1, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{ -17, 0,   4}, {-17, 0, 12}, {     1, 0, 1, 1, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{  -4, 0,   4}, {-16, 0, 36}, { 0.875, 0, 1, 1, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{  -4, 0,   4}, {-16, 0,  4}, {     1, 1, 1, 0, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{ -12, 0,   4}, {-16, 0,  4}, {     1, 1, 1, 0, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{ -16, 0,   4}, { -4, 0,  4}, {     1, 1, 1, 0, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{  16, 0, -16}, { 16, 0, 64}, {     1, 1, 1, 0, 0}},
	{"Box, Covered Step start solid", &box, &hull_covered_step,
		{  16, 0,  16}, { 16, 0, 64}, {     1, 0, 1, 1, 0}},

	{"Box,  Step 2", &box, &hull_step2,
		{ 0, 0, 64}, {0, 0, 0}, { 0.375, 0, 0, 1, 0}},
	{"Box,  Step 3", &box, &hull_step3,
		{ 0, 0, 64}, {0, 0, 0}, { 0.375, 0, 0, 1, 0}},

	{"Box,  Ramp", &box, &hull_ramp,
		{ 0, 0, 16}, { 0, 0, 0}, {   0.5, 0, 0, 1, 0}},
	{"Box,  Ramp", &box, &hull_ramp,
		{-16, 0, 8}, {16, 0, 8}, {     1, 0, 0, 1, 0}},
	{"Box,  Ramp", &box, &hull_ramp,
		{-16, 0, 5}, {16, 0, 5}, { 0.125, 0, 0, 1, 0}},

	{"Box,  Simple Wedge", &box, &hull_simple_wedge,
		{  0, 0, 16}, { 0, 0, 0}, {  0.5, 0, 0, 1, 0}},
	{"Box,  Simple Wedge", &box, &hull_simple_wedge,
		{-16, 0,  8}, {16, 0, 8}, {    1, 0, 0, 1, 0}},
	{"Box,  Simple Wedge", &box, &hull_simple_wedge,
		{-16, 0,  5}, {16, 0, 5}, { 0.25, 0, 0, 1, 0}},
	{"Box,  Simple Wedge", &box, &hull_simple_wedge,
		{-16, 0, 12}, {16, 0, 4}, {  0.5, 0, 0, 1, 0}},

	{"Box,  Hole. slide in", &box, &hull_hole,
		{ 0, -16,  0}, {  0,  16,   0}, {    1, 0, 0, 1, 0}},
	{"Box,  Hole. slide out", &box, &hull_hole,
		{ 0,  16,  0}, {  0, -16,   0}, {    1, 0, 0, 1, 0}},
	{"Box,  Hole. tight", &box, &hull_hole,
		{ 0,  16,  0}, {  0,  16,  16}, {    0, 0, 0, 1, 0}},
	{"Box,  Hole. tight", &box, &hull_hole,
		{ 0,  16,  0}, {  0,  16, -16}, {    0, 0, 0, 1, 0}},
	{"Box,  Hole. tight", &box, &hull_hole,
		{ 0,  16,  0}, { 16,  16,   0}, {    0, 0, 0, 1, 0}},
	{"Box,  Hole. tight", &box, &hull_hole,
		{ 0,  16,  0}, {-16,  16,   0}, {    0, 0, 0, 1, 0}},
	{"Box,  Hole. edge", &box, &hull_hole,
		{ 0, -16,  1}, {  0,  16,   1}, { 0.25, 0, 0, 1, 0}},
	{"Box,  Hole. edge", &box, &hull_hole,
		{ 0, -16, -1}, {  0,  16,  -1}, { 0.25, 0, 0, 1, 0}},
	{"Box,  Hole. edge", &box, &hull_hole,
		{ 1, -16,  0}, {  1,  16,   0}, { 0.25, 0, 0, 1, 0}},
	{"Box,  Hole. edge", &box, &hull_hole,
		{-1, -16,  0}, { -1,  16,   0}, { 0.25, 0, 0, 1, 0}},

	{"Box,  ridge", &box, &hull_ridge,
		{4, 0, 41}, { 4, 0, 39}, { 0.5, 0, 0, 1, 0}},
	{"Box,  ridge, all solid", &box, &hull_ridge,
		{4, 41, 0}, { 4, 39, 0}, {   1, 1, 1, 0, 0}},

	{"Box,  cave. start solid", &box, &hull_cave,
		{  4, 0, -16}, {  4, 0,  0}, { 0.5, 0, 1, 0, 1}},
	{"Box,  cave. start solid", &box, &hull_cave,
		{ -8, 0, -16}, { -8, 0,  0}, { 0.5, 0, 1, 0, 1}},
	{"Box,  cave. start solid", &box, &hull_cave,
		{ 54, 0, -16}, { 54, 0, 56}, { 0.5, 0, 1, 1, 0}},
	{"Box,  cave", &box, &hull_cave,
		{ 54, 0,  12}, { 54, 0, 28}, { 0.5, 0, 0, 1, 0}},
	{"Big Box,  cave. start solid", &bigbox, &hull_cave,
		{ 54, 0,   0}, { 54, 0,  8}, { 0.5, 0, 1, 1, 0}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int verbose = 0;

static trace_t
do_trace (box_t *box, hull_t *hull, vec3_t start, vec3_t end)
{
	trace_t trace;

	trace.allsolid = true;
	trace.startsolid = false;
	trace.inopen = false;
	trace.inwater = false;
	trace.fraction = 1;
	VectorCopy (box->extents, trace.extents);
	// FIXME specify tract type in test spec
	trace.type = box == &point ? tr_point : tr_box;
	VectorCopy (end, trace.endpos);
	MOD_TraceLine (hull, 0, start, end, &trace);
	return trace;
}

typedef struct portlist_s {
	struct portlist_s *next;
	clipport_t *portal;
} portlist_t;

static portlist_t *
collect_portals (clipport_t *portal, portlist_t *portal_list)
{
	portlist_t *p;

	if (!portal)
		return portal_list;
	for (p = portal_list; p; p = p->next)
		if (p->portal == portal)
			return portal_list;
	p = malloc (sizeof (portlist_t));
	p->portal = portal;
	p->next = portal_list;
	portal_list = p;
	portal_list = collect_portals (portal->next[0], portal_list);
	portal_list = collect_portals (portal->next[1], portal_list);
	return portal_list;
}

static int
run_test (test_t *test)
{
	const char *desc;
	vec3_t      end;
	int         err = 0;
	char       *expect;
	char       *got;
	static int  output = 0;
	portlist_t *portal_list = 0;

	if (!test->hull->depth)
		Mod_FindClipDepth (test->hull);
	if (!test->hull->nodeleafs) {
		hull_t     *hull = test->hull;
		int         i, j;
		portlist_t *p;
		clipleaf_t *leaf;

		hull->nodeleafs = MOD_BuildBrushes (hull);
		for (i = hull->firstclipnode; i <= hull->lastclipnode; i++) {
			for (j = 0; j < 2; j++) {
				leaf = hull->nodeleafs[i].leafs[j];
				if (!leaf)
					continue;
				portal_list = collect_portals (leaf->portals, portal_list);
			}
		}
		for (p = portal_list; p; p = p->next) {
			for (j = 0; j < p->portal->winding->numpoints; j++) {
				p->portal->winding->points[j][0]
					= bound (-8192, p->portal->winding->points[j][0], 8192);
				p->portal->winding->points[j][1]
					= bound (-8192, p->portal->winding->points[j][1], 8192);
				p->portal->winding->points[j][2]
					= bound (-8192, p->portal->winding->points[j][2], 8192);
			}
			FreeWinding (p->portal->edges);
			p->portal->edges = WindingVectors (p->portal->winding, 0);
		}
	}
	while (portal_list) {
		portlist_t *t = portal_list;
		portal_list = portal_list->next;
		free (t);
	}

	VectorSubtract (test->end, test->start, end);
	VectorMultAdd (test->start, test->expect.frac, end, end);
	expect = nva ("expect: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
				  " %3g as:%d ss:%d io:%d iw:%d",
				  test->start[0], test->start[1], test->start[2],
				  test->end[0], test->end[1], test->end[2],
				  end[0], end[1], end[2],
				  test->expect.frac,
				  test->expect.allsolid, test->expect.startsolid,
				  test->expect.inopen, test->expect.inwater);
	trace_t trace = do_trace (test->box, test->hull, test->start, test->end);
	got = nva ("   got: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
			   " %3g as:%d ss:%d io:%d iw:%d",
			   test->start[0], test->start[1], test->start[2],
			   test->end[0], test->end[1], test->end[2],
			   trace.endpos[0], trace.endpos[1], trace.endpos[2],
			   trace.fraction,
			   trace.allsolid, trace.startsolid,
			   trace.inopen, trace.inwater);
	if (!(VectorCompare (end, trace.endpos)
		  && FloatCompare (test->expect.frac, trace.fraction)
		  && test->expect.allsolid == trace.allsolid
		  && test->expect.startsolid == trace.startsolid
		  && test->expect.inopen == trace.inopen
		  && test->expect.inwater == trace.inwater))
		err = 1;

	if (test->desc)
		desc = va (0, "(%d) %s", (int)(long)(test - tests), test->desc);
	else
		desc = va (0, "test #%d", (int)(long)(test - tests));
	if (verbose >= 0 || err) {
		if (output)
			puts("");
		output = 1;
		puts (expect);
		puts (got);
		printf ("%s: %s\n", !err ? "PASS" : "FAIL", desc);
	}
	free (expect);
	free (got);
	return !err;
}

#include "main.c"
