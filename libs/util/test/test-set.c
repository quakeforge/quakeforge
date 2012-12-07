#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>

#include "QF/set.h"

typedef set_t *(*setup_func) (void);
typedef set_t *(*test_func) (set_t *s1, set_t *s2);

static set_t *
make_empty1 (void)
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

struct {
	setup_func set1;
	setup_func set2;
	test_func  test;
	const char *expect;
} tests[] = {
	{make_empty1, 0, 0, "[empty]"},
	{make_empty2, 0, 0, "[empty]"},
	{make_everything, 0, 0, "[everything]"},
	{make_empty_invert, 0, 0, "[everything]"},
	{make_everything_invert, 0, 0, "[empty]"},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_tests; i++) {
		set_t      *s1, *s2 = 0;
		const char *set_str;

		s1 = tests[i].set1 ();
		if (tests[i].set2)
			s2 = tests[i].set2 ();
		if (tests[i].test) {
			if (tests[i].test (s1, s2) != s1) {
				res |= 1;
				printf ("test %d didn't return s1\n", (int) i);
				continue;
			}
		}
		set_str = set_as_string (s1);
		if (strcmp (set_str, tests[i].expect)) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %s\n", tests[i].expect);
			printf ("got   : %s\n", set_str);
		}
	}
	return res;
}
