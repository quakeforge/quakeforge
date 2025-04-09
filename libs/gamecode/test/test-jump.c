#include "head.c"

#define DB 0xdeadbeef

static pr_int_t test_globals_init[] = {
	DB, 1, 2, 3, DB,
};

static pr_int_t test_globals_expect[] = {
	DB, 1, 2, 3, 1,
};

static dstatement_t jump_A_statements[] = {
	{ OP(0, 0, 0, OP_JUMP_A),  4, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 4 },
	{ OP(0, 0, 0, OP_JUMP_A),  2, 0, 0 },
	{ OP(0, 0, 0, OP_JUMP_A), -2, 0, 0 },
};

static dstatement_t jump_B_statements[] = {
	{ OP(0, 0, 0, OP_JUMP_B),  1, 2, 0 },
	{ OP(0, 0, 0, OP_BREAK),   0, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 4 },
};

static dstatement_t jump_C_statements[] = {
	{ OP(0, 0, 0, OP_JUMP_C),  1, 2, 0 },
	{ OP(0, 0, 0, OP_BREAK),   0, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 4 },
};

static dstatement_t jump_D_statements[] = {
	{ OP(0, 0, 0, OP_JUMP_D),  1, 2, 0 },
	{ OP(0, 0, 0, OP_BREAK),   0, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 0 },
	{ OP(0, 0, 0, OP_LEA_A),   1, 0, 4 },
};

test_t tests[] = {
	{
		.desc = "jump A",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (jump_A_statements),
		.statements = jump_A_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "jump B",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (jump_B_statements),
		.statements = jump_B_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "jump C",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (jump_C_statements),
		.statements = jump_C_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
	{
		.desc = "jump D",
		.num_globals = num_globals (test_globals_init, test_globals_expect),
		.num_statements = num_statements (jump_D_statements),
		.statements = jump_D_statements,
		.init_globals = test_globals_init,
		.expect_globals = test_globals_expect,
	},
};

#include "main.c"
