#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/qfcc.h"

#include "tools/qfcc/test/test-defspace.h"

options_t   options;
pr_info_t   pr;
function_t *current_func;
class_type_t *current_class;

void
free_def (def_t *def)
{
	if (0) free (def);
}

__attribute__((const))const char *
get_class_name (class_type_t *class_type, int prett)
{
	return 0;
}

expr_t *
new_expr (void)
{
	return calloc(1, sizeof (expr_t));
}

static int
check_init_state (const defspace_t *space, const char *name)
{
	int pass = 1;

	if (space->free_locs) {
		printf ("%s free_locs not null\n", name);
		pass = 0;
	}
	if (space->defs) {
		printf ("%s defs not null\n", name);
		pass = 0;
	}
	if (space->def_tail != &space->defs) {
		printf ("%s def_tail not pointing to defs\n", name);
		pass = 0;
	}
	if (space->data) {
		printf ("%s data not null\n", name);
		pass = 0;
	}
	if (space->size) {
		printf ("%s size not 0\n", name);
		pass = 0;
	}
	if (space->max_size) {
		printf ("%s max_size not 0\n", name);
		pass = 0;
	}
	if (!space->grow) {
		printf ("%s grow is null\n", name);
		pass = 0;
	}
	if (space->qfo_space) {
		printf ("%s qfo_space not 0\n", name);
		pass = 0;
	}
	return pass;
}

static int
test_init (void)
{
	int pass = 1;
	defspace_t *backed = defspace_new (ds_backed);
	defspace_t *virtual = defspace_new (ds_virtual);

	if (backed->grow == virtual->grow) {
		printf ("expected different grow functions for backed and virtual\n");
		pass = 0;
	}

	if (backed->type != ds_backed) {
		printf ("backed ds has wrong type\n");
		pass = 0;
	}

	if (virtual->type != ds_virtual) {
		printf ("virtual ds has wrong type\n");
		pass = 0;
	}

	pass &= check_init_state (backed, "backed");
	pass &= check_init_state (virtual, "virtual");

	return pass;
}

static int
test_aligned_alloc (void)
{
	defspace_t *space = defspace_new (ds_virtual);
	struct {
		int size, align;
	}           allocations[6] = {
		{ 2, 2 },
		{ 2, 2 },
		{ 1, 1 },
		{ 4, 4 },
		{ 2, 2 },
		{ 2, 2 },
	};
	int         offsets[6];
	for (int i = 0; i < 6; i++) {
		offsets[i] = defspace_alloc_aligned_loc (space, allocations[i].size,
												 allocations[i].align);
	}
	for (int i = 0; i < 5; i++) {
		for (int j = i + 1; j < 6; j++) {
			if (offsets[i] == offsets[j]) {
				printf ("duplicate offset in allocations");
				printf ("%d %d %d %d %d %d\n",
						offsets[0], offsets[1], offsets[2],
						offsets[3], offsets[4], offsets[5]);
				return 0;
			}
		}
	}
	return 1;
}

int
main (int argc, const char **argv)
{
	pr.strings = strpool_new ();

	int pass = 1;

	pass &= test_init ();
	pass &= test_aligned_alloc ();

	return !pass;
}
