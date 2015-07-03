#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/set.h"
#include "QF/va.h"

#define SIZE (SET_DEFMAP_SIZE * sizeof (set_bits_t) * 8)

typedef set_t *(*setup_func) (void);
typedef set_t *(*op_func) (set_t *s1, const set_t *s2);
typedef int (*test_func) (const set_t *s1, const set_t *s2);

static set_t *
make_empty (void)
{
	set_t      *set = set_new ();
	return set;
}

static set_t *
make_empty2 (void)
{
	set_t      *set = set_new ();
	return set_empty (set);
}

static set_t *
make_everything (void)
{
	set_t      *set = set_new ();
	return set_everything (set);
}

static set_t *
make_empty_invert (void)
{
	set_t      *set = set_new ();
	return set_invert (set);
}

static set_t *
make_everything_invert (void)
{
	set_t      *set = set_new ();
	set_everything (set);
	return set_invert (set);
}

static set_t *
make_SIZE (void)
{
	set_t      *set = set_new ();
	set_add (set, SIZE);
	return set;
}

static set_t *
make_not_SIZE (void)
{
	set_t      *set = set_new ();
	set_add (set, SIZE);
	return set_invert (set);
}

static set_t *
make_0_to_SIZEm1 (void)
{
	set_t      *set = set_new ();
	size_t      i;

	for (i = 0; i < SIZE; i++)
		set_add (set, i);
	return set;
}

static int
check_size (const set_t *set, const set_t *unused)
{
	return set->size;
}

static set_t *
make_5 (void)
{
	set_t      *set = set_new ();
	return set_add (set, 5);
}

static set_t *
make_55 (void)
{
	set_t      *set = set_new ();
	return set_add (set, 55);
}

static set_t *
make_not_5 (void)
{
	return set_invert (make_5 ());
}

static set_t *
make_not_55 (void)
{
	return set_invert (make_55 ());
}

struct {
	setup_func  set1;
	setup_func  set2;
	op_func     op;
	test_func   test;
	int         test_expect;
	const char *str_expect;
} tests[] = {
	{make_empty,             0, 0, check_size, SIZE, "{}"},
	{make_empty2,            0, 0, check_size, SIZE, "{}"},
	{make_everything,        0, 0, check_size, SIZE, "{...}"},
	{make_empty_invert,      0, 0, check_size, SIZE, "{...}"},
	{make_everything_invert, 0, 0, check_size, SIZE, "{}"},
	{make_SIZE,              0, 0, check_size, SIZE + SET_BITS, "{64}"},
	{make_0_to_SIZEm1,       0, 0, check_size, SIZE,
		"{0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63}"
	},
	{make_everything,        make_0_to_SIZEm1, set_difference, check_size,
		SIZE, "{64 ...}"
	},
	{make_0_to_SIZEm1,        make_everything, set_reverse_difference,
		check_size, SIZE, "{64 ...}"
	},
	{make_everything, make_empty, 0, set_is_subset,      1, 0},
	{make_everything, make_empty, 0, set_is_equivalent,   0, 0},
	{make_everything, make_empty, 0, set_is_intersecting, 0, 0},
	{make_everything, make_empty, 0, set_is_disjoint,     1, 0},
	{make_empty, make_everything, 0, set_is_subset,       0, 0},
	{make_empty, make_everything, 0, set_is_equivalent,   0, 0},
	{make_empty, make_everything, 0, set_is_intersecting, 0, 0},
	{make_empty, make_everything, 0, set_is_disjoint,     1, 0},
	{make_everything, make_everything, 0, set_is_subset,       1, 0},
	{make_everything, make_everything, 0, set_is_equivalent,   1, 0},
	{make_everything, make_everything, 0, set_is_intersecting, 1, 0},
	{make_everything, make_everything, 0, set_is_disjoint,     0, 0},
	{make_empty, make_empty, 0, set_is_subset,       1, 0},
	{make_empty, make_empty, 0, set_is_equivalent,   1, 0},
	{make_empty, make_empty, 0, set_is_intersecting, 0, 0},
	{make_empty, make_empty, 0, set_is_disjoint,     1, 0},
	{make_5, make_5, 0, set_is_equivalent,   1, 0},
	{make_5, make_5, 0, set_is_intersecting, 1, 0},
	{make_5, make_5, 0, set_is_disjoint,     0, 0},
	{make_5,     make_55, 0, set_is_equivalent,   0, 0},
	{make_5,     make_55, 0, set_is_intersecting, 0, 0},
	{make_5,     make_55, 0, set_is_disjoint,     1, 0},
	{make_not_5, make_55, 0, set_is_equivalent,   0, 0},
	{make_not_5, make_55, 0, set_is_intersecting, 1, 0},
	{make_not_5, make_55, 0, set_is_disjoint,     0, 0},
	{make_5,     make_not_55, 0, set_is_equivalent,   0, 0},
	{make_5,     make_not_55, 0, set_is_intersecting, 1, 0},
	{make_5,     make_not_55, 0, set_is_disjoint,     0, 0},
	{make_not_5, make_not_55, 0, set_is_equivalent,   0, 0},
	{make_not_5, make_not_55, 0, set_is_intersecting, 1, 0},
	{make_not_5, make_not_55, 0, set_is_disjoint,     0, 0},
	{make_5,  make_55, set_union, set_is_equivalent,   0, "{5 55}"},
	{make_5,  make_55, set_union, set_is_intersecting, 1, "{5 55}"},
	{make_5,  make_55, set_union, set_is_disjoint,     0, "{5 55}"},
	{make_55, make_5,  set_union, set_is_equivalent,   0, "{5 55}"},
	{make_55, make_5,  set_union, set_is_intersecting, 1, "{5 55}"},
	{make_55, make_5,  set_union, set_is_disjoint,     0, "{5 55}"},
	{make_not_SIZE, make_everything, 0, set_is_equivalent,   0, 0},
	{make_not_SIZE, make_everything, 0, set_is_intersecting, 1, 0},
	{make_not_SIZE, make_everything, 0, set_is_disjoint,     0, 0},
	{make_SIZE, make_everything, 0, set_is_equivalent,   0, 0},
	{make_SIZE, make_everything, 0, set_is_intersecting, 1, 0},
	{make_SIZE, make_everything, 0, set_is_disjoint,     0, 0},
	{make_not_5, make_everything, 0, set_is_equivalent,   0, 0},
	{make_not_5, make_everything, 0, set_is_intersecting, 1, 0},
	{make_not_5, make_everything, 0, set_is_disjoint,     0, 0},
	{make_5, make_everything, 0, set_is_equivalent,   0, 0},
	{make_5, make_everything, 0, set_is_intersecting, 1, 0},
	{make_5, make_everything, 0, set_is_disjoint,     0, 0},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;
	dstring_t  *str;

	//printf ("set_bits_t: %d, SET_DEFMAP_SIZE: %d, SIZE: %d\n",
	//		sizeof (set_bits_t), SET_DEFMAP_SIZE, SIZE);

	tests[5].str_expect = nva ("{%d}", SIZE);
	tests[7].str_expect = nva ("{%d ...}", SIZE);
	tests[8].str_expect = nva ("{%d ...}", SIZE);

	str = dstring_new ();
	for (i = 0; i < SIZE; i++) {
		dasprintf (str, "%c%d", i ? ' ' : '{', i);
	}
	dstring_appendstr (str, "}");
	tests[6].str_expect = dstring_freeze (str);

	for (i = 0; i < num_tests; i++) {
		set_t      *s1, *s2 = 0;
		const char *set_str;

		s1 = tests[i].set1 ();
		if (tests[i].set2)
			s2 = tests[i].set2 ();
		if (tests[i].op) {
			if (tests[i].op (s1, s2) != s1) {
				res |= 1;
				printf ("test op %d didn't return s1\n", (int) i);
				continue;
			}
		}
		if (tests[i].test) {
			int         test_res;
			test_res = tests[i].test (s1, s2);
			if (test_res != tests[i].test_expect) {
				res |= 1;
				printf ("test %d failed\n", (int) i);
				printf ("expect: %d\n", tests[i].test_expect);
				printf ("got   : %d\n", test_res);
				continue;
			}
		}
		set_str = set_as_string (s1);
		if (tests[i].str_expect && strcmp (set_str, tests[i].str_expect)) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %s\n", tests[i].str_expect);
			printf ("got   : %s\n", set_str);
			continue;
		}
		set_delete (s1);
		if (s2)
			set_delete (s2);
	}
	return res;
}
