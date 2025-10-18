#include <stdio.h>
#include <inttypes.h>
#include "QF/thread/deque.h"

static int
test_array (void)
{
	circarr_t  *array = circarr_new (6);
	if (!array) {
		printf ("failed to allocate array\n");
		return 0;
	}
	if (!array->data) {
		printf ("array has no data\n");
		return 0;
	}
	if (array->size_log != 6) {
		printf ("array has incorrect log size: %"PRId64"\n", array->size_log);
		return 0;
	}
	if (circarr_size (array) != 64) {
		printf ("array has incorrect size: %"PRId64"\n", circarr_size (array));
		return 0;
	}
	if (array->size_mask & (array->size_mask + 1)) {
		printf ("array has incorrect mask: %016"PRIx64"\n",
				array->size_mask);
		return 0;
	}
	for (int64_t i = 0; i < circarr_size (array); i++) {
		circarr_put (array, i, (void *) i);
	}
	for (int64_t i = circarr_size (array); i < 2 * circarr_size (array); i++) {
		__auto_type v = (int64_t) circarr_get (array, i);
		if (v != i - circarr_size (array)) {
			printf ("got wrong value from array: e: %"PRId64" g: %"PRId64"\n",
					i - circarr_size (array), v);
			return 0;
		}
	}
	circarr_delete (array);
	return 1;
}

static int
test_deque (void)
{
	deque_t    *deque = deque_new (DEQUE_DEF_SIZE);
	void       *o;
	int         objs[4];
	if ((o = deque_popBottom (deque))) {
		printf ("deque not empty for pop: %p\n", o);
		return 0;
	}
	if ((o = deque_steal (deque))) {
		printf ("deque not empty for steal: %p\n", o);
		return 0;
	}
	for (int j = 0; j < 100; j++) {
		for (int i = 0; i < 4; i++) {
			deque_pushBottom (deque, &objs[i]);
		}
		if ((o = deque_popBottom (deque)) != &objs[3]) {
			printf ("popped wrong obj 1 %d: g:%p e:%p\n", j, o, &objs[3]);
			return 0;
		}
		if ((o = deque_steal (deque)) != &objs[0]) {
			printf ("stole wrong obj 1 %d: g:%p e:%p\n", j, o, &objs[0]);
			return 0;
		}
		if ((o = deque_popBottom (deque)) != &objs[2]) {
			printf ("popped wrong obj 2 %d: g:%p e:%p\n", j, o, &objs[2]);
			return 0;
		}
		if ((o = deque_steal (deque)) != &objs[1]) {
			printf ("stole wrong obj 2 %d: g:%p e:%p\n", j, o, &objs[1]);
			return 0;
		}
		if ((o = deque_popBottom (deque))) {
			printf ("deque not empty for pop %d: %p\n", j, o);
			return 0;
		}
		if ((o = deque_steal (deque))) {
			printf ("deque not empty for steal %d: %p\n", j, o);
			return 0;
		}
	}
	deque_delete (deque);
	return 1;
}

int
main (void)
{
	int         ret = 0;
	ret |= !test_array ();
	ret |= !test_deque ();
	return ret;
}
