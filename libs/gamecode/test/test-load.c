#include "head.c"

static pr_int_t test_globals_init[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// destination data
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	// source data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

static pr_int_t test_globals_expect[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,
	// destination data
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	// source data
	11, 12, 9, 10,
	8, 5, 6, 7,
	1, 2, 3, 4,
};

static dstatement_t load_B_statements[] = {
	{OP(0, 0, 0, OP_LOAD_B_4), 7, 9, 12},
	{OP(0, 0, 0, OP_LOAD_B_3), 7, 8, 16},
	{OP(0, 0, 0, OP_LOAD_B_1), 7, 7, 19},
	{OP(0, 0, 0, OP_LOAD_B_2), 7, 6, 20},
	{OP(0, 0, 0, OP_LOAD_B_2), 7, 5, 22},
};

static dstatement_t load_C_statements[] = {
	{OP(0, 0, 0, OP_LOAD_C_4), 2, 4, 12},
	{OP(0, 0, 0, OP_LOAD_C_3), 2, 1, 16},
	{OP(0, 0, 0, OP_LOAD_C_1), 2, 0, 19},
	{OP(0, 0, 0, OP_LOAD_C_2), 2, -2, 20},
	{OP(0, 0, 0, OP_LOAD_C_2), 2, -4, 22},
};

static dstatement_t load_D_statements[] = {
	{OP(0, 0, 0, OP_LOAD_D_4), 2, 9, 12},
	{OP(0, 0, 0, OP_LOAD_D_3), 2, 8, 16},
	{OP(0, 0, 0, OP_LOAD_D_1), 2, 7, 19},
	{OP(0, 0, 0, OP_LOAD_D_2), 2, 6, 20},
	{OP(0, 0, 0, OP_LOAD_D_2), 2, 5, 22},
};

test_t tests[] = {
	{
		.desc = "load B",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load_B_statements),
		.statements = load_B_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
		// FIXME negative field offsets are not official but work because all
		// offset calculations are done in 32-bit and thus wrap anyway
		.edict_area = 28,
	},
	{
		.desc = "load C",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load_C_statements),
		.statements = load_C_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "load D",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load_D_statements),
		.statements = load_D_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
};

#include "main.c"
