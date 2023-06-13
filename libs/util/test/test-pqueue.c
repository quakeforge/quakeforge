/*
	test-pqueue.c

	Priority queue related tests

	Copyright (C) 2021  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "QF/pqueue.h"

#define SIZEOF(x) (sizeof(x) / sizeof(x[0]))

static int
compare (const int *a, const int *b, void *arg)
{
	//printf ("%d %d\n", *a, *b);
	return *a - *b;
}

static int
rev (const int *a, const int *b, void *arg)
{
	return *b - *a;
}

static int queue_array[7];
static struct PQUEUE_TYPE (int) pqueue = {
	.maxSize = SIZEOF (queue_array),
	.compare = compare,
	.a = queue_array,
	.data = 0,
};

static int
pq_is_empty (void)
{
	return PQUEUE_IS_EMPTY (&pqueue);
}

static int
pq_is_full (void)
{
	return PQUEUE_IS_FULL (&pqueue);
}

static int
pq_peek (void)
{
	return PQUEUE_PEEK (&pqueue);
}

static void
pq_insert (int value)
{
	PQUEUE_INSERT (&pqueue, value);
}

static int
pq_remove (void)
{
	return PQUEUE_REMOVE (&pqueue);
}

static void
pq_adjust (int index)
{
	PQUEUE_ADJUST (&pqueue, index);
}

static void
dump_array (const int *arr, size_t n)
{
	while (n-- > 0) {
		printf (" %d", *arr++);
	}
	printf ("\n");
}


static int
test_insert (const int *data, size_t count)
{
	while (count-- > 0) {
		pq_insert (*data++);
	}
	return 1;
}

static int
test_remove (const int *data, size_t count)
{
	while (count-- > 0) {
		if (pq_remove () != *data++) {
			return 0;
		}
	}
	return 1;
}

static int
test_adjust (const int *data, size_t count)
{
	//printf ("adjust start\n");
	pqueue.a[count] = *data;
	pq_adjust (count);
	//printf ("adjust end\n");
	return 1;
}

static int
test_peek (const int *data, size_t count)
{
	return pq_peek () == *data;
}

static int
test_size (const int *data, size_t count)
{
	return pqueue.size == count;
}

static int
test_is_empty (const int *data, size_t count)
{
	return pq_is_empty () == *data;
}

static int
test_is_full (const int *data, size_t count)
{
	return pq_is_full () == *data;
}

static int
dump_queue (const int *data, size_t count)
{
	if (0) {
		dump_array (pqueue.a, pqueue.size);
	}
	return 1;
}

static int true_v = 1;
static int false_v = 0;

static int data[] = { 3, 2, 5, 6, 8, 1, 9 };
static int data1[] = { 3, 7, 5, 6, 8, 1, 9 };
static int data2[] = { 3, 7, 5, 6, 8, 1, 4 };
static int data3[] = { 3, 7, 0, 6, 8, 1, 4 };
static int sort[SIZEOF(data)];
static int sort1[SIZEOF(data1)];
static int sort2[SIZEOF(data2)];
static int sort3[SIZEOF(data3)];

typedef struct {
	const int  *data;
	size_t      nele;
	int       (*test) (const int *data, size_t nele);
} test_t;

static test_t tests[] = {
	{ &true_v,  1, test_is_empty },
	{ &false_v, 1, test_is_full  },
	{ 0,        0, test_size     },

	{ data + 0, 1, test_insert   },
	{ 0,        1, test_size     },
	{ &false_v, 1, test_is_empty },
	{ &false_v, 1, test_is_full  },
	{ data + 0, 1, test_peek     },
	{ data + 0, 1, test_remove   },
	{ &true_v,  1, test_is_empty },
	{ &false_v, 1, test_is_full  },
	{ 0,        0, test_size     },

	{ data + 0, 3, test_insert   },
	{ 0,        3, test_size     },
	{ data + 2, 1, test_peek     },
	{ data + 2, 1, test_remove   },
	{ data + 0, 1, test_remove   },
	{ data + 1, 1, test_remove   },
	{ &true_v,  1, test_is_empty },

	{ data + 0,  SIZEOF(data), test_insert   },
	{ 0,         SIZEOF(data), test_size     },
	{ &false_v,  1,            test_is_empty },
	{ &true_v,   1,            test_is_full  },
	{ 0,         0,            dump_queue    },
	{ sort1 + 2, 3,            test_adjust   },
	{ 0,         0,            dump_queue    },
	{ sort2 + 4, 0,            test_adjust   },
	{ 0,         0,            dump_queue    },
	{ sort3 + 6, 4,            test_adjust   },
	{ 0,         0,            dump_queue    },
	{ sort3 + 0, SIZEOF(sort), test_remove   },
};
#define num_tests ((int) SIZEOF (tests))

static int
run_test (const test_t *test)
{
	return test->test (test->data, test->nele);
}

int
main(int argc, const char **argv)
{
	int         ret = 0;

	memcpy (sort, data, sizeof (sort));
	memcpy (sort1, data1, sizeof (sort1));
	memcpy (sort2, data2, sizeof (sort2));
	memcpy (sort3, data3, sizeof (sort3));
	heapsort_r (sort, SIZEOF(sort), sizeof(int), (__compar_d_fn_t)rev, 0);
	heapsort_r (sort1, SIZEOF(sort1), sizeof(int), (__compar_d_fn_t)rev, 0);
	heapsort_r (sort2, SIZEOF(sort2), sizeof(int), (__compar_d_fn_t)rev, 0);
	heapsort_r (sort3, SIZEOF(sort3), sizeof(int), (__compar_d_fn_t)rev, 0);
	//dump_array (sort, SIZEOF(sort));
	//dump_array (sort1, SIZEOF(sort1));
	//dump_array (sort2, SIZEOF(sort2));
	//dump_array (sort3, SIZEOF(sort3));
	for (int i = 0; i < num_tests; i++) {
		if (!run_test (&tests[i])) {
			printf ("test %d failed\n", i);
			ret = 1;
		}
	}
	return ret;
}
