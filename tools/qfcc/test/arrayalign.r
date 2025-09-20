#include "test-harness.h"

typedef struct gizmo_node_s {
	vec4        plane;
	int         children[2];
} gizmo_node_t;

static gizmo_node_t tetra_brush[] = {
	// Node 0 is the root node, but since the root node cannot be a child node,
	// a child node index of 0 indicates the outside. -1 is inside
	{ .plane = { 1,-1,-1,-0.5}, .children= {0, 1} },
	{ .plane = {-1, 1,-1,-0.5}, .children= {0, 2} },
	{ .plane = { 1, 1, 1,-0.5}, .children= {0, 3} },
	{ .plane = { 1,-1,-1,-0.5}, .children= {0,-1} },
};

int
main()
{
	if (sizeof (tetra_brush) % sizeof(vec4)) {
		printf ("array size is not vec4 aligned: %d\n", sizeof (tetra_brush));
		return 1;
	}
	auto base = (uint)&tetra_brush[0];
	auto one = (uint)&tetra_brush[1];
	if (base % sizeof (vec4)) {
		printf ("tetra_brush[0] is not vec4 aligned: %d\n", base);
		return 1;
	}
	if (one % sizeof (vec4)) {
		printf ("tetra_brush[1] is not vec4 aligned: %d\n", one);
		return 1;
	}
	for (int i = 0; i < countof (tetra_brush); i++) {
		if (tetra_brush[i].children[0] != 0) {
			printf ("tetra_brush[%d].children[0](%d) != 0\n", i,
					tetra_brush[i].children[1]);
			return 1;
		}
		static int child_expect[] = {1, 2, 3, -1};
		if (tetra_brush[i].children[1] != child_expect[i]) {
			printf ("tetra_brush[%d].children[1](%d) != %d\n", i,
					tetra_brush[i].children[1], child_expect[i]);
			return 1;
		}
	}
	return 0;
}
