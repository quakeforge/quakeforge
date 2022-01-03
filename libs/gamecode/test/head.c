#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/progs.h"

static int verbose = 0;

// both calculates the number of globals in the test, and ensures that both
// init and expect are the same size (will product a "void value not ignored"
// error if the sizes differ)
#define num_globals(init, expect) \
	__builtin_choose_expr ( \
		sizeof (init) == sizeof (expect), sizeof (init) / sizeof (init[0]), \
		(void) 0\
	)

// calculate the numver of statements in the test
#define num_statements(statements) \
		(sizeof (statements) / sizeof (statements[0]))

typedef struct {
	const char *desc;
	pr_uint_t   edict_area;
	pr_uint_t   extra_globals;
	pr_uint_t   num_globals;
	pr_uint_t   num_statements;
	dstatement_t *statements;
	pr_int_t   *init_globals;
	pr_int_t   *expect_globals;
} test_t;
