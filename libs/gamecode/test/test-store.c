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
	pr_uint_t   extra_globals;
	pr_uint_t   num_globals;
	pr_uint_t   num_statements;
	dstatement_t *statements;
	pr_int_t   *init_globals;
	pr_int_t   *expect_globals;
} test_t;

static pr_int_t test_globals_init[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// source data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// destination data
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
};

static pr_int_t test_globals_expect[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// source data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// destination data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

#define BASE(b, base) (((base) & 3) << OP_##b##_SHIFT)
#define OP(a, b, c, op) ((op) | BASE(A, a) | BASE(B, b) | BASE(C, c))

static dstatement_t store_A_statements[] = {
	{OP(0, 0, 0, OP_STORE_A_4), 32, 0, 12},
	{OP(0, 0, 0, OP_STORE_A_3), 29, 0, 16},
	{OP(0, 0, 0, OP_STORE_A_1), 28, 0, 19},
	{OP(0, 0, 0, OP_STORE_A_2), 26, 0, 20},
	{OP(0, 0, 0, OP_STORE_A_2), 24, 0, 22},
};

static dstatement_t store_B_statements[] = {
	{OP(0, 0, 0, OP_STORE_B_4), 4, 0, 12},
	{OP(0, 0, 0, OP_STORE_B_3), 3, 0, 16},
	{OP(0, 0, 0, OP_STORE_B_1), 2, 0, 19},
	{OP(0, 0, 0, OP_STORE_B_2), 1, 0, 20},
	{OP(0, 0, 0, OP_STORE_B_2), 0, 0, 22},
};

static dstatement_t store_C_statements[] = {
	{OP(0, 0, 0, OP_STORE_C_4), 2, 4, 12},
	{OP(0, 0, 0, OP_STORE_C_3), 2, 1, 16},
	{OP(0, 0, 0, OP_STORE_C_1), 2, 0, 19},
	{OP(0, 0, 0, OP_STORE_C_2), 2, -2, 20},
	{OP(0, 0, 0, OP_STORE_C_2), 2, -4, 22},
};

static dstatement_t store_D_statements[] = {
	{OP(0, 0, 0, OP_STORE_D_4), 2, 9, 12},
	{OP(0, 0, 0, OP_STORE_D_3), 2, 8, 16},
	{OP(0, 0, 0, OP_STORE_D_1), 2, 7, 19},
	{OP(0, 0, 0, OP_STORE_D_2), 2, 6, 20},
	{OP(0, 0, 0, OP_STORE_D_2), 2, 5, 22},
};

test_t tests[] = {
	{
		.desc = "store A",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (store_A_statements),
		.statements = store_A_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "store B",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (store_B_statements),
		.statements = store_B_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "store C",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (store_C_statements),
		.statements = store_C_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "store D",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (store_D_statements),
		.statements = store_D_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
};

#include "main.c"
