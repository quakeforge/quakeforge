#pragma bug die
#include "test-harness.h"

typedef struct link_s {
	int something;
	struct link_s *next;
} link_t;

link_t *
link_objs(link_t **array, int count)
{
	link_t     *obj = nil, *o;
	while (count-- > 0) {
		o = array[count];
		o.next = obj;
		obj = o;
	}
	return obj;
}

link_t link_a;
link_t link_b;
link_t *links[2];

int
main ()
{
	links[0] = &link_a;
	links[1] = &link_b;
	link_t *chain = link_objs (links, 2);
	if (chain != &link_a) {
		printf ("chain doesn't point to link_a\n");
		return 1;
	}
	if (chain.next != &link_b) {
		printf ("chain.next doesn't point to link_b\n");
		return 1;
	}
	if (chain.next.next != nil) {
		printf ("chain.next.next isn't nil\n");
		return 1;
	}
	return 0;
}
