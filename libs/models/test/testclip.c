#include <stdlib.h>
#include "QF/va.h"

#include "getopt.h"
#include "world.h"

#undef DIST_EPSILON
#define DIST_EPSILON 0

#ifdef TEST_ID
# include "trace-id.c"
#elif defined(TEST_QF_BAD)
# include "trace-qf-bad.c"
#else
# include "../trace.c"
#endif

//  0,0
//   |\   .
//   |s\  .
//   |ss\ .
//   0   1

mclipnode_t clipnodes_simple_wedge[] = {
	{  0, {             1, CONTENTS_EMPTY}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

plane_t planes_simple_wedge[] = {
	{{1, 0, 0}, 0, 0, 0},		//  0
	{{0.8, 0, 0.6}, 0, 4, 0},	//  1 
};

hull_t hull_simple_wedge = {
	clipnodes_simple_wedge,
	planes_simple_wedge,
	0,
	1,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     0   1   2

mclipnode_t clipnodes_tpp1[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             2, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

plane_t planes_tpp1[] = {
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp1 = {
	clipnodes_tpp1,
	planes_tpp1,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     1   0   2

mclipnode_t clipnodes_tpp2[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_SOLID, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

plane_t planes_tpp2[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp2 = {
	clipnodes_tpp2,
	planes_tpp2,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|   |www|sss
//  sss|   |www|sss
//     1   0   2

mclipnode_t clipnodes_tppw[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_WATER}},
};

plane_t planes_tppw[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tppw = {
	clipnodes_tppw,
	planes_tppw,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//     2
//  eee|eee
// 0,32+--- 1
//  eee|sss
//  ---+--- 0
//  ss0,0ss
mclipnode_t clipnodes_step1[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {CONTENTS_EMPTY,              2}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

plane_t planes_step1[] = {
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
	{{1, 0, 0},   0, 0, 0},
};

hull_t hull_step1 = {
	clipnodes_step1,
	planes_step1,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//     0
//  eee|eee
// 0,32+--- 1
//  eee|sss
//  ---+sss 2
//  ss0,0ss
mclipnode_t clipnodes_step2[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

plane_t planes_step2[] = {
	{{1, 0, 0},   0, 0, 0},
	{{0, 0, 1},  32, 2, 0},
	{{0, 0, 1},   0, 2, 0},
};

hull_t hull_step2 = {
	clipnodes_step2,
	planes_step2,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//     0
//  eee|eee
// 2---+0,32
//  sss|eee
//  sss+--- 1
//  ss0,0ss
mclipnode_t clipnodes_step3[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

plane_t planes_step3[] = {
	{{1, 0, 0},   0, 0, 0},
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
};

hull_t hull_step3 = {
	clipnodes_step3,
	planes_step3,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//   3 2
//  s|e|eee
// 4-+e|-20,40
//  e|e|eee
// 0,32+--- 1
//  eee|sss
//  ---+--- 0
//  ss0,0ss
mclipnode_t clipnodes_covered_step[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             3,              2}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
	{  3, {CONTENTS_EMPTY,              4}},
	{  4, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

plane_t planes_covered_step[] = {
	{{0, 0, 1},   0, 2, 0},
	{{0, 0, 1},  32, 2, 0},
	{{1, 0, 0},   0, 0, 0},
	{{1, 0, 0}, -20, 0, 0},
	{{0, 0, 1},  40, 2, 0},
};

hull_t hull_covered_step = {
	clipnodes_covered_step,
	planes_covered_step,
	0,
	4,
	{0, 0, 0},
	{0, 0, 0},
};

//     0
//  eee|eee
//  eee+--- 1
//  ee/0,0s
//   2 ssss
mclipnode_t clipnodes_ramp[] = {
	{  0, {             1,              2}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

plane_t planes_ramp[] = {
	{{   1, 0,   0},   0, 0, 0},
	{{   0, 0,   1},   0, 2, 0},
	{{-0.6, 0, 0.8},   0, 4, 0},
};

hull_t hull_ramp = {
	clipnodes_ramp,
	planes_ramp,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};


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
		int         num_portals;
	}           expect;
} test_t;

box_t point = { {0, 0, 0} };
box_t box =   { {8, 8, 8} };
box_t player = { {16, 16, 28} };

test_t tests[] = {
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0, 3}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0, 3}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0, 3}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0, 3}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 1, 1, 0, 0, 3}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 1, 0, 3}},

	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0, 3}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0, 3}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0, 3}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0, 3}},

	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{-64, 0, 0}, { 64, 0, 0}, { 0.875, 0, 1, 1, 1, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, {-88, 0, 0}, {0.5625, 0, 0, 1, 1, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 0, 1, 1, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 0, 0, 1, 0, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 0, 1, 3}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1, 3}},

	{"Point, Step 1", &point, &hull_step1,
		{ -16, 0, 8}, {16, 0, 24}, { 0.5, 0, 0, 1, 0, 5}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 24}, { 0.25, 0, 0, 1, 0, 5}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 40}, { 0.25, 0, 0, 1, 0, 5}},
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 135}, { 0.25, 0, 0, 1, 0, 5}},
	// 136 is a corner case caused by back/front side issues and 0
	{"Box, Step 1", &box, &hull_step1,
		{ -16, 0, 8}, {16, 0, 137}, { 1, 0, 0, 1, 0, 5}},

	{"Point, Covered Step", &point, &hull_covered_step,
		{ -24, 0, 8}, {-24, 0, 72}, { 0.5, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -32, 0, 8}, {-32, 0, 72}, { 0.375, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -24, 0, 8}, {-24, 0, 72}, { 0.375, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -25, 0, 8}, {7, 0, 72}, { 0.375, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -8, 0, 40}, {-16, 0, 40}, { 0.5, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -17, 0, 8}, {-1, 0, 72}, { 1, 0, 0, 1, 0, 9}},
	{"Box, Covered Step", &box, &hull_covered_step,
		{ -8, 0, 40}, {8, 0, 72}, { 1, 0, 0, 1, 0, 9}},

	{"Box,  Step 2", &box, &hull_step2,
		{ 0, 0, 64}, {0, 0, 0}, { 0.375, 0, 0, 1, 0, 5}},
	{"Box,  Step 3", &box, &hull_step3,
		{ 0, 0, 64}, {0, 0, 0}, { 0.375, 0, 0, 1, 0, 5}},

	{"Box,  Ramp", &box, &hull_ramp,
		{ 0, 0, 16}, {0, 0, 0}, { 0.5, 0, 0, 1, 0, 4}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

static int test_enabled[num_tests] = { 0 };

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
	trace.type = tr_box;
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
	int         res = 0;
	char       *expect;
	char       *got;
	static int  output = 0;
	portlist_t *portal_list = 0;

	if (!test->hull->nodeleafs) {
		hull_t     *hull = test->hull;
		int         i, j;
		portlist_t *p;
		clipport_t *portal;
		clipleaf_t *leaf;
		int         side;

		hull->nodeleafs = MOD_BuildBrushes (hull);
		for (i = hull->firstclipnode; i <= hull->lastclipnode; i++) {
			for (j = 0; j < 2; j++) {
				if (((hull->clipnodes[i].children[j] >= 0)
					 != (!hull->nodeleafs[i].leafs[j]))
					|| (hull->nodeleafs[i].leafs[j]
						&& (hull->nodeleafs[i].leafs[j]->contents
							!= hull->clipnodes[i].children[j]))) {
					printf ("bad nodeleaf %d %d\n", i, j);
					res = 1;
				}
			}
			if (hull->nodeleafs[i].leafs[0]
				&& (hull->nodeleafs[i].leafs[0]
					== hull->nodeleafs[i].leafs[1])) {
				printf ("bad nodeleaf %d %d\n", i, j);
				res = 1;
			}
		}
		if (res)
			goto nodeleaf_bail;
		for (i = hull->firstclipnode; i <= hull->lastclipnode; i++) {
			for (j = 0; j < 2; j++) {
				leaf = hull->nodeleafs[i].leafs[j];
				if (!leaf)
					continue;
				portal_list = collect_portals (leaf->portals, portal_list);
			}
		}
		for (i = 0, p = portal_list; p; i++, p = p->next) {
			if (!p->portal->winding || !p->portal->edges) {
				res = 1;
				printf ("portal with missing vertex/edge information\n");
			}
		}
		if (res)
			goto nodeleaf_bail;
		if (i != test->expect.num_portals) {
			res = 1;
			printf ("bad portal count: %d %d\n", test->expect.num_portals, i);
			goto nodeleaf_bail;
		}
		for (p = portal_list; p; p = p->next) {
			for (j = 0; j < 2; j++) {
				int         found = 0;

				leaf = p->portal->leafs[j];
				for (portal = leaf->portals; portal;
					 portal = portal->next[side]) {
				//printf("%p %d %p %p %p\n", p, j, leaf, portal, p->portal);
					side = portal->leafs[1] == leaf;
					if (!side && portal->leafs[0] != leaf) {
						printf ("mislinked portal\n");
						res = 1;
					}
					if (portal == p->portal)
						found = 1;
				}
				if (!found) {
					printf ("portal unreachable from leaf\n");
					res = 1;
				}
			}
		}
	}
nodeleaf_bail:
	while (portal_list) {
		portlist_t *t = portal_list;
		portal_list = portal_list->next;
		free (t);
	}

	VectorSubtract (test->end, test->start, end);
	VectorMultAdd (test->start, test->expect.frac, end, end);
	expect = nva ("expect: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
				  " %3g %d %d %d %d",
				  test->start[0], test->start[1], test->start[2],
				  test->end[0], test->end[1], test->end[2],
				  end[0], end[1], end[2],
				  test->expect.frac, 
				  test->expect.allsolid, test->expect.startsolid,
				  test->expect.inopen, test->expect.inwater);
	trace_t trace = do_trace (test->box, test->hull, test->start, test->end);
	got = nva ("   got: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
			   " %3g %d %d %d %d",
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

int
main (int argc, char **argv)
{
//	vec3_t      start, end;
	int         c;
	size_t      i, test;
	int         pass = 1;

	while ((c = getopt (argc, argv, "qvt:")) != EOF) {
		switch (c) {
			case 'q':
				verbose--;
				break;
			case 'v':
				verbose++;
				break;
			case 't':
				test = atoi (optarg);
				if (test < num_tests) {
					test_enabled[test] = 1;
				} else {
					fprintf (stderr, "Bad test number (0 - %zd)\n", num_tests);
					return 1;
				}
				break;
			default:
				fprintf (stderr, "-q (quiet) -v (verbose) and/or -t TEST "
								 "(test number)\n");
				return 1;
		}
	}

	for (i = 0; i < num_tests; i++)
		if (test_enabled[i])
			break;
	if (i == num_tests) {
		for (i = 0; i < num_tests; i++)
			test_enabled[i] = 1;
	}

	if (verbose > 0)
		printf ("start -> end => stop frac allsolid startsolid inopen "
				"inwater\n");
	for (i = 0; i < num_tests; i++) {
		if (!test_enabled[i])
			continue;
		pass &= run_test (&tests[i]);
	}

	return !pass;
}
