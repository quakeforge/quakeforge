#include "head.c"

static pr_int_t lea_globals_init[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 0xdeadbeef, 0xfeedf00d,

	0, 0, 0, 0,
};

static pr_int_t lea_globals_expect[] = {
	// pointers
	24, 26, 28, 29,
	32, -4, -2, 0,
	1, 4, 6, 32,

	7, 34, 26, 88,
};

static dstatement_t lea_statements[] = {
	{OP(0, 0, 0, OP_LEA_A), 7, 9, 12},
	{OP(0, 0, 0, OP_LEA_C), 2, 6, 13},
	{OP(0, 0, 0, OP_LEA_D), 2, 6, 14},
	{OP(0, 0, 0, OP_LEA_B), 4, 2, 15},
	{OP(0, 0, 0, OP_LEA_E), 4, 2, 10},
	{OP(0, 0, 0, OP_LEA_F), 4, 2, 11},
};

test_t tests[] = {
	{
		.desc = "lea",
		.num_globals = num_globals (lea_globals_init, lea_globals_expect),
		.num_statements = num_statements (lea_statements),
		.statements = lea_statements,
		.init_globals = lea_globals_init,
		.expect_globals = lea_globals_expect,
		.edict_area = 28,
	},
};

#include "main.c"
