#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include "QF/va.h"

#include "getopt.h"
#include "world.h"
#include "QF/winding.h"

#include "hulls.h"

#define FloatCompare(a, b) (fabs (a - b) < 1e-5)
#undef VectorCompare
#define VectorCompare(x, y)									\
	(FloatCompare (x[0], y[0]) && FloatCompare (x[1], y[1])	\
	 && FloatCompare (x[2], y[2]))


typedef struct {
	const char *desc;
	hull_t     *hull;
	struct {
		int         num_portals;
	}           expect;
} test_t;

test_t tests[] = {
	{"Three parallel planes 1",
		&hull_tpp1,         { 3}},
	{"Three parallel planes 2",
		&hull_tpp2,         { 3}},
	{"Three parallel planes with water",
		&hull_tppw,         { 3}},
	{"Step 1",
		&hull_step1,        { 5}},
	{"Covered Step",
		&hull_covered_step, { 9}},
	{"Step 2",
		&hull_step2,        { 5}},
	{"Step 3",
		&hull_step3,        { 5}},
	{"Ramp",
		&hull_ramp,         { 4}},
	{"Simple Wedge",
		&hull_simple_wedge, { 3}},
	{"Hole",
		&hull_hole,         {13}},
	{"ridge",
		&hull_ridge,        { 8}},
	{"cave",
		&hull_cave,         {14}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int verbose = 0;

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
	int         err = 0;
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
					err = 1;
				}
			}
			if (hull->nodeleafs[i].leafs[0]
				&& (hull->nodeleafs[i].leafs[0]
					== hull->nodeleafs[i].leafs[1])) {
				printf ("bad nodeleaf %d %d\n", i, j);
				err = 1;
			}
		}
		if (err)
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
				err = 1;
				printf ("portal with missing vertex/edge information\n");
			}
		}
		if (err)
			goto nodeleaf_bail;
		if (i != test->expect.num_portals) {
			err = 1;
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
						err = 1;
					}
					if (portal == p->portal)
						found = 1;
				}
				if (!found) {
					printf ("portal unreachable from leaf\n");
					err = 1;
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
	MOD_FreeBrushes (test->hull);

	if (test->desc)
		desc = va (0, "(%d) %s", (int)(long)(test - tests), test->desc);
	else
		desc = va (0, "test #%d", (int)(long)(test - tests));
	if (verbose >= 0 || err) {
		if (output)
			puts("");
		output = 1;
		printf ("%s: %s\n", !err ? "PASS" : "FAIL", desc);
	}
	return !err;
}

#include "main.c"
