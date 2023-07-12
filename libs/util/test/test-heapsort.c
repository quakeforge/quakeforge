/*
	test-heapsort.c

	Priority heap related tests

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

#include "QF/heapsort.h"

#define SIZEOF(x) (sizeof(x) / sizeof(x[0]))

static int sink_data[] = {
	4, 7, 8, 3, 2, 6, 5,
};
static int sink_expect[] = {
	8, 7, 6, 3, 2, 4, 5,
};

static int swim_data[] = {
	10, 8, 9, 7, 4, 1, 3, 2,
};
static int swim_expect[] = {
	10, 8, 9, 7, 4, 1, 3, 2,
};

static int swim_data2[] = {
	10, 8, 9, 7, 4, 1, 3, 0, 2, 5,
};
static int swim_expect2[] = {
	10, 8, 9, 7, 5, 1, 3, 0, 2, 4,
};

static int build_data[] = {
	1, 4, 3, 7, 8, 9, 10,
};
static int build_expect1[] = {
	10, 8, 9, 7, 4, 1, 3,
};
static int build_expect2[] = {
	9, 8, 3, 7, 4, 1, 10,
};
static int build_expect3[] = {
	8, 7, 3, 1, 4, 9, 10,
};

static int sort_data[] = {
	4, 3, 7, 1, 8, 5,
};
static int sort_expect[] = {
	1, 3, 4, 5, 7, 8,
};

static int
compare (const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}
#if 0
static int
compared (const void *a, const void *b, void *d)
{
	return *(const int *)a - *(const int *)b;
}
#endif

static void
test_sink (int *data, size_t nele)
{
	heap_sink (data, 0, nele, sizeof (int), compare);
}

static void
test_swim (int *data, size_t nele)
{
	heap_swim (data, nele - 1, nele, sizeof (int), compare);
}

static void
test_build (int *data, size_t nele)
{
	heap_build (data, nele, sizeof (int), compare);
}

static void
test_sort (int *data, size_t nele)
{
	heapsort (data, nele, sizeof (int), compare);
}

typedef struct {
	const int  *data;
	const int  *expect;
	size_t      nele;
	void      (*test) (int *data, size_t nele);
	size_t      sub;
} test_t;

static test_t tests[] = {
	{ sink_data,  sink_expect,   SIZEOF (sink_data),  test_sink  },
	{ swim_data,  swim_expect,   SIZEOF (swim_data),  test_swim  },
	{ swim_data2, swim_expect2,  SIZEOF (swim_data2), test_swim  },
	{ build_data, build_expect1, SIZEOF (build_data), test_build },
	{ build_data, build_expect2, SIZEOF (build_data), test_build, 1 },
	{ build_data, build_expect3, SIZEOF (build_data), test_build, 2 },
	{ sort_data,  sort_expect,   SIZEOF (sort_data),  test_sort  },
};
#define num_tests ((int) SIZEOF (tests))

static void
dump_array (const int *arr, size_t n)
{
	while (n-- > 0) {
		printf (" %d", *arr++);
	}
	printf ("\n");
}

static int
run_test (test_t *test)
{
	int         test_data[test->nele];

	memcpy (test_data, test->data, test->nele * sizeof (int));
	printf ("start : ");
	dump_array (test_data, test->nele);
	test->test (test_data, test->nele - test->sub);
	printf ("got   : ");
	dump_array (test_data, test->nele);
	printf ("expect: ");
	dump_array (test->expect, test->nele);
	if (memcmp (test_data, test->expect, test->nele * sizeof (int))) {
		return 0;
	}
	return 1;
}

int
main(int argc, const char **argv)
{
	int         ret = 0;

	for (int i = 0; i < num_tests; i++) {
		if (!run_test (&tests[i])) {
			printf ("test %d failed\n", i);
			ret = 1;
		}
	}
	return ret;
}
