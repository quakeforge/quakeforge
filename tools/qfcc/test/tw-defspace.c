#include "../source/defspace.c"
#include "test-defspace.h"

__attribute__((pure)) int
def_list_is_empty (const defspace_t *space)
{
	return (!space->defs && space->def_tail == &space->defs);
}

__attribute__((pure)) int
def_list_is_valid (const defspace_t *space)
{
	def_t *const *d = &space->defs;

	while (*d) {
		d = &(*d)->next;
	}
	return d == space->def_tail;
}

int
free_locs_is_valid (const defspace_t *space)
{
	int         free_space = 0;
	const locref_t *loc;

	for (loc = space->free_locs; loc; loc = loc->next) {
		if (loc->ofs < 0) {
			printf ("negative offset in free_locs\n");
			return 0;
		}
		if (loc->size <= 0) {
			printf ("zero or negative size in free_locs\n");
			return 0;
		}
		if (loc->next && loc->ofs > loc->next->ofs) {
			printf ("free_locs not in ascending order\n");
			return 0;
		}
		if (loc->next && loc->ofs + loc->size > loc->next->ofs) {
			printf ("overlap in free_locs\n");
			return 0;
		}
		if (loc->next && loc->ofs + loc->size == loc->next->ofs) {
			printf ("adjoining nodes in free_locs\n");
			return 0;
		}
		free_space += loc->size;
	}
	if (free_space > space->size) {
		printf ("free_locs describes too much free space\n");
		return 0;
	}
	return 1;
}
