#include "test-harness.h"

typedef struct bisector_s {
	uint        propagationID;
	uint        problem_neighbor;
	int         state:3;
	uint        visible:1;
	uint        modified:1;
	uint        subdiv;

	uint        indices[3];
} bisector_t;

typedef struct neighbors_s {
	uint        prev;
	uint        next;
	uint        twin;
} neighbors_t;

bisector_t bisector_data[32];

void doit (bisector_t bisector, uint selfID, uint sib0ID, uint sib1ID,
		   neighbors_t n)
{
	auto b = bisector;
	b.propagationID = selfID;
	b.modified = true;

	b.problem_neighbor = ~0u;
	bisector_data[selfID] = b;

	b.problem_neighbor = n.next;
	bisector_data[sib0ID] = b;

	b.problem_neighbor = ~0u;
	bisector_data[sib1ID] = b;
}

bool check_bisector (bisector_t *b, uint parent, uint target)
{
	return b.propagationID == parent && b.problem_neighbor == target;
}

int
main ()
{
	bisector_t bisector = {
		.state = 2,
		.visible = 1,
		.subdiv = 3,
	};
	neighbors_t n = {
		.prev = 13,
		.next = 9,
		.twin = 5,
	};
	doit (bisector, 8, 21, 22, n);
	printf ("expect 8, -1\n");
	printf ("   got %d %d\n",
			bisector_data[8].propagationID,
			bisector_data[8].problem_neighbor);
	printf ("expect 8, 9\n");
	printf ("   got %d %d\n",
			bisector_data[21].propagationID,
			bisector_data[21].problem_neighbor);
	printf ("expect 8, -1\n");
	printf ("   got %d %d\n",
			bisector_data[22].propagationID,
			bisector_data[22].problem_neighbor);
	bool ok = check_bisector (&bisector_data[8], 8, -1)
			&&check_bisector (&bisector_data[21], 8, 9)
			&&check_bisector (&bisector_data[22], 8, -1);
	return ok ? 0 : 1;
}
