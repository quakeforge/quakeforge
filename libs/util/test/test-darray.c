#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#define remove remove_renamed
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "QF/darray.h"
#include "QF/sys.h"
#undef remove

typedef int (*test_func) (int a, int b);
typedef struct intarray_s DARRAY_TYPE(int) intarray_t;

intarray_t  intarray = {0, 0, 16, 0};

static int
append (int val, int b)
{
	return DARRAY_APPEND (&intarray, val);
}

static int
check_size (int a, int b)
{
	return intarray.size;
}

static int
check_maxSize (int a, int b)
{
	return intarray.maxSize;
}

static  int
check_grow (int a, int b)
{
	return intarray.grow;
}

static int
check_maxSizeGrowth (int a, int b)
{
	return intarray.maxSize % intarray.grow;
}

static int
check_array (int a, int b)
{
	return !!intarray.a;
}

static int __attribute__((pure))
check_value (int index, int b)
{
	if ((size_t) index >= intarray.size) {
		Sys_Error ("indexing beyond array");
	}
	return intarray.a[index];
}

static int
insert_at (int val, int pos)
{
	return DARRAY_INSERT_AT (&intarray, val, pos);
}

static int
open_at (int pos, int size)
{
	return DARRAY_OPEN_AT (&intarray, pos, size) - intarray.a;
}

static const char text[] = "Aloy is an awesome huntress.";
static int
open_at2 (int pos, int size)
{
	memcpy(DARRAY_OPEN_AT (&intarray, pos, size), text, size * sizeof (int));
	return strcmp((char*) (intarray.a + pos), text);
}

static int
remove_at (int pos, int b)
{
	return DARRAY_REMOVE_AT (&intarray, pos);
}

static int
remove (int pos, int b)
{
	return DARRAY_REMOVE (&intarray);
}

static int
close_at (int pos, int size)
{
	return DARRAY_CLOSE_AT (&intarray, pos, size);
}

static int
clear (int a, int b)
{
	DARRAY_CLEAR (&intarray);
	return 0;
}

static int
resize (int size, int b)
{
	DARRAY_RESIZE (&intarray, size);
	return 0;
}

struct {
	test_func   test;
	int         param1, param2;
	int         test_expect;
} tests[] = {
	{check_size,              0,  0,  0},	// confirm array empty but can grow
	{check_maxSize,           0,  0,  0},
	{check_grow,              0,  0, 16},
	{check_array,             0,  0,  0},
	{append,                  5,  0,  5},	// test first append to emtpty array
	{check_size,              0,  0,  1},
	{check_maxSizeGrowth,     0,  0,  0},
	{check_maxSize,           0,  0, 16},
	{check_array,             0,  0,  1},
	{check_value,             0,  0,  5},
	{append,                 42,  0, 42},	// test a second append
	{check_size,              0,  0,  2},
	{check_maxSize,           0,  0, 16},
	{check_value,             0,  0,  5},
	{check_value,             1,  0, 42},
	{insert_at,              69,  1, 69},	// test insertions
	{insert_at,              96,  0, 96},
	{check_size,              0,  0,  4},
	{check_maxSize,           0,  0, 16},
	{check_value,             0,  0, 96},
	{check_value,             1,  0,  5},
	{check_value,             2,  0, 69},
	{check_value,             3,  0, 42},
	{open_at,                 2, 14,  2},	// test opening a large hole
	{check_maxSizeGrowth,     0,  0,  0},
	{check_maxSize,           0,  0, 32},
	{check_size,              0,  0, 18},
	{check_value,             0,  0, 96},
	{check_value,             1,  0,  5},
	{check_value,            16,  0, 69},
	{check_value,            17,  0, 42},
	{close_at,                1, 15,  5},	// test block removal
	{check_maxSize,           0,  0, 32},
	{check_size,              0,  0,  3},
	{check_value,             0,  0, 96},
	{check_value,             1,  0, 69},
	{check_value,             2,  0, 42},
	{remove,                  0,  0, 42},	// test "pop"
	{check_maxSize,           0,  0, 32},
	{check_size,              0,  0,  2},
	{check_value,             0,  0, 96},
	{check_value,             1,  0, 69},
	{remove_at,               0,  0, 96},	// test remove at
	{check_maxSize,           0,  0, 32},
	{check_size,              0,  0,  1},
	{check_value,             0,  0, 69},
	{insert_at,              71,  1, 71},	// test insertion at end
	{resize,                  48, 0,  0},	// test resize bigger
	{check_maxSizeGrowth,     0,  0,  0},
	{check_maxSize,           0,  0, 48},
	{check_size,              0,  0, 48},
	{check_value,             0,  0, 69},
	{check_value,             1,  0, 71},
	{resize,                  24, 0,  0},	// test resize smaller
	{check_maxSizeGrowth,     0,  0,  0},
	{check_maxSize,           0,  0, 48},
	{check_size,              0,  0, 24},
	{check_value,             0,  0, 69},
	{check_value,             1,  0, 71},
	{open_at2,                1,  (sizeof (text) + sizeof (int) - 1) / sizeof (int), 0},
	{check_value,             0,  0, 69},
	{check_value,             1,  0, 0x796f6c41},
	{check_value,             9,  0, 71},
	{clear,		              0,  0,  0},	// test clearing
	{check_size,              0,  0,  0},
	{check_maxSize,           0,  0,  0},
	{check_array,             0,  0,  0},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))
int test_start_line = __LINE__ - num_tests - 2;

int
main (int argc, const char **argv)
{
	int         res = 0;

	size_t      i;

	for (i = 0; i < num_tests; i++) {
		if (tests[i].test) {
			int         test_res;
			test_res = tests[i].test (tests[i].param1, tests[i].param2);
			if (test_res != tests[i].test_expect) {
				res |= 1;
				printf ("test %d (line %d) failed\n", (int) i,
						(int) i + test_start_line);
				printf ("expect: %d\n", tests[i].test_expect);
				printf ("got   : %d\n", test_res);
				continue;
			}
		}
	}
	return res;
}
