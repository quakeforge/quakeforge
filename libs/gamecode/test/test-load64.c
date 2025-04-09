#include "head.c"

static pr_int_t test_globals_init[] = {
	// pointers
	24, 26, 48, 29,
	32, -8, -4, 0,
	2, 8, 0xdeadbeef, 0xfeedf00d,
	0xdeadbeef, 0xfeedf00d, 0xdeadbeef, 0xfeedf00d,
	// destination data
	 0,  0,    0,  0,    0,  0,    0,  0,
	 0,  0,    0,  0,    0,  0,    0,  0,
	 0,  0,    0,  0,    0,  0,    0,  0,
	// source data
	21, 22,   23, 24,   17, 18,   19, 20,
	15, 16,    9, 10,   11, 12,   13, 14,
	 1,  2,    3,  4,    5,  6,    7,  8,
};

static pr_int_t test_globals_expect[] = {
	// pointers
	24, 26, 48, 29,
	32, -8, -4, 0,
	2, 8, 0xdeadbeef, 0xfeedf00d,
	0xdeadbeef, 0xfeedf00d, 0xdeadbeef, 0xfeedf00d,
	// destination data
/*16*/	 1,  2,    3,  4,    5,  6,    7,  8,
/*24*/	 9, 10,   11, 12,   13, 14,   15, 16,
/*32*/	17, 18,   19, 20,   21, 22,   23, 24,
	// source data
/*40*/	21, 22,   23, 24,   17, 18,   19, 20,
/*48*/	15, 16,    9, 10,   11, 12,   13, 14,
/*56*/	 1,  2,    3,  4,    5,  6,    7,  8,
};

static dstatement_t load64_B_statements[] = {
	{OP(0, 0, 0, OP_LOAD64_B_4), 7, 9, 16},
	{OP(0, 0, 0, OP_LOAD64_B_3), 7, 8, 24},
	{OP(0, 0, 0, OP_LOAD_B_2),   7, 7, 30},
	{OP(0, 0, 0, OP_LOAD_B_4),   7, 6, 32},
	{OP(0, 0, 0, OP_LOAD_B_4),   7, 5, 36},
};

static dstatement_t load64_C_statements[] = {
	{OP(0, 0, 0, OP_LOAD64_C_4), 2,  8, 16},
	{OP(0, 0, 0, OP_LOAD64_C_3), 2,  2, 24},
	{OP(0, 0, 0, OP_LOAD_C_2),   2,  0, 30},
	{OP(0, 0, 0, OP_LOAD_C_4),   2, -4, 32},
	{OP(0, 0, 0, OP_LOAD_C_4),   2, -8, 36},
};

static dstatement_t load64_D_statements[] = {
	{OP(0, 0, 0, OP_LOAD64_D_4), 2, 9, 16},
	{OP(0, 0, 0, OP_LOAD64_D_3), 2, 8, 24},
	{OP(0, 0, 0, OP_LOAD_D_2),   2, 7, 30},
	{OP(0, 0, 0, OP_LOAD_D_4),   2, 6, 32},
	{OP(0, 0, 0, OP_LOAD_D_4),   2, 5, 36},
};

test_t tests[] = {
	{
		.desc = "load64 B",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load64_B_statements),
		.statements = load64_B_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
		// FIXME negative field offsets are not official but work because all
		// offset calculations are done in 32-bit and thus wrap anyway
		.edict_area = 48,
	},
	{
		.desc = "load64 C",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load64_C_statements),
		.statements = load64_C_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "load64 D",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (load64_D_statements),
		.statements = load64_D_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
};

#include "main.c"
