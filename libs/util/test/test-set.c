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

static set_t *
make_range_0_SIZE (void)
{
	set_t      *set = set_new ();
	set_add_range (set, 0, SIZE);
	return set;
}

static set_t *
make_range_0_SIZEm1 (void)
{
	set_t      *set = set_new ();
	set_add_range (set, 0, SIZE - 1);
	return set;
}

static set_t *
make_range_0_SIZEx2 (void)
{
	set_t      *set = set_new ();
	set_add_range (set, 0, SIZE*2);
	return set;
}

static set_t *
make_range_1_SIZE (void)
{
	set_t      *set = set_new ();
	set_add_range (set, 1, SIZE);
	return set;
}

static set_t *
remove_3_9 (set_t *set, const set_t *dummy)
{
	return set_remove_range (set, 3, 9);
}

static int
check_size (const set_t *set, const set_t *unused)
{
	return set->size;
}

static int
check_count (const set_t *set, const set_t *unused)
{
	return set_count (set);
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

static set_t *
make_0_1 (void)
{
	set_t      *set = set_new ();
	set_add (set, 0);
	set_add (set, 1);
	return set;
}

static set_t *
make_not_1_2 (void)
{
	set_t      *set = set_new ();
	set_everything (set);
	set_remove (set, 1);
	set_remove (set, 2);
	return set;
}

static set_t *
expand_3xSIZEm1 (set_t *set, const set_t *x)
{
	set_expand (set, 3 * SIZE - 1);
	return set;
}

static set_t *
expand_3xSIZEp1 (set_t *set, const set_t *x)
{
	set_expand (set, 3 * SIZE + 1);
	return set;
}

static set_t *
expand_3xSIZE (set_t *set, const set_t *x)
{
	set_expand (set, 3 * SIZE);
	return set;
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
	{make_SIZE,              0, expand_3xSIZEm1, check_size, 3 * SIZE,
		"{64}"},
	{make_SIZE,              0, expand_3xSIZE, check_size, 3 * SIZE, "{64}"},
	{make_SIZE,              0, expand_3xSIZEp1, check_size,
		3 * SIZE + SET_BITS, "{64}"},
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
	{make_55, make_5,  set_union, check_count,         2, "{5 55}"},
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
	{make_empty, 0, 0, check_count, 0, 0},
	{make_everything, 0, 0, check_count, 0, 0},
	{make_5, 0, 0, check_count, 1, 0},
	{make_not_5, 0, 0, check_count, 1, 0},
	{make_0_to_SIZEm1, 0, 0, check_size, SIZE, 0},
	{make_0_1, 0, 0, 0, 0, "{0 1}"},
	{make_not_1_2, 0, 0, check_count, 2, "{0 3 ...}"},//68
	{make_0_1, make_not_1_2, set_union, check_count, 1, "{0 1 3 ...}"},
	{make_0_1, make_not_1_2, set_intersection, check_count, 1, "{0}"},
	{make_0_1, make_not_1_2, set_difference, check_count, 1, "{1}"},
	{make_0_1, make_not_1_2, set_reverse_difference, check_count, 3, "{3 ...}"},
	{make_not_1_2, make_0_1, set_union, check_count, 1, "{0 1 3 ...}"},
	{make_not_1_2, make_0_1, set_intersection, check_count, 1, "{0}"},
	{make_not_1_2, make_0_1, set_difference, check_count, 3, "{3 ...}"},
	{make_not_1_2, make_0_1, set_reverse_difference, check_count, 1, "{1}"},//76
	{make_SIZE, make_not_1_2, set_union, check_size, SIZE + SET_BITS,
		"{0 3 ...}"},
	{make_SIZE, make_not_1_2, set_intersection, check_size, SIZE + SET_BITS,
		"{64}"},
	{make_SIZE, make_not_1_2, set_difference, check_size, SIZE + SET_BITS,
		"{}"},
	{make_SIZE, make_not_1_2, set_reverse_difference, check_size,
		SIZE + SET_BITS,
		"{0 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 65 ...}"
	},//80
	{make_not_1_2, make_SIZE, set_union, check_size, SIZE, "{0 3 ...}"},
	{make_not_1_2, make_SIZE, set_intersection, check_size, SIZE + SET_BITS,
		"{64}"},
	{make_not_1_2, make_SIZE, set_difference, check_size, SIZE + SET_BITS,
		"{0 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 65 ...}"
	},
	{make_not_1_2, make_SIZE, set_reverse_difference, check_size, SIZE, "{}"},
	{make_range_0_SIZE,      0, 0, check_size, SIZE,
		"{0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63}"
	},
	{make_range_0_SIZEm1,    0, 0, check_size, SIZE,
		"{0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62}"
	},
	{make_range_0_SIZEx2,    0, 0, check_size, SIZE*2,
		"{0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63"
		" 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79"
		" 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95"
		" 96 97 98 99 100 101 102 103 104 105 106 107 108 109 110 111"
		" 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127}"
	},
	{make_range_1_SIZE,      0, 0, check_size, SIZE + SET_BITS,
		"{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63"
		" 64}"
	},
	{make_everything, 0, remove_3_9, check_size, SIZE, "{0 1 2 12 ...}"},
	{make_range_0_SIZE,      0, remove_3_9, check_size, SIZE,
		"{0 1 2 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63}"
	},
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

	tests[5].str_expect = nva ("{%zd}", SIZE);
	tests[7].str_expect = nva ("{%zd ...}", SIZE);
	tests[8].str_expect = tests[7].str_expect;
	tests[9].str_expect = tests[5].str_expect;
	tests[10].str_expect = tests[5].str_expect;
	tests[11].str_expect = tests[5].str_expect;
	tests[78].str_expect = tests[5].str_expect;
	tests[82].str_expect = tests[5].str_expect;

	str = dstring_new ();
	for (i = 0; i < SIZE; i++) {
		dasprintf (str, "%c%zd", i ? ' ' : '{', i);
	}
	dstring_appendstr (str, "}");
	tests[6].str_expect = dstring_freeze (str);

	str = dstring_new ();
	dasprintf (str, "{0");
	for (i = 3; i < SIZE; i++) {
		dasprintf (str, " %zd", i);
	}
	dasprintf (str, " %zd ...}", SIZE + 1);
	tests[80].str_expect = dstring_freeze (str);
	tests[83].str_expect = tests[80].str_expect;

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
