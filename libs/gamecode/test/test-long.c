#include "head.c"

#include "QF/mathlib.h"

#define sq(x) ((x)*(x))

static pr_lvec4_t long_binop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_lvec4_t long_binop_expect[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  15, -15, -15, 15},
	{  5.0/3, -5.0/3, -5.0/3, 5.0/3},
	{  2, -2, 2, -2},
	{  2,  1, -1, -2},
	{  8, -2, 2, -8},
	{  2, -8, 8, -2},
};

static dstatement_t long_binop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_MUL_L_1),  0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_L_1),  0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_L_1),  0,  8, 32 },
	{ OP(1, 1, 1, OP_MOD_L_1),  0,  8, 40 },
	{ OP(1, 1, 1, OP_ADD_L_1),  0,  8, 48 },
	{ OP(1, 1, 1, OP_SUB_L_1),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t long_binop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_MUL_L_2),  0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_L_2),  0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_L_2),  0,  8, 32 },
	{ OP(1, 1, 1, OP_MOD_L_2),  0,  8, 40 },
	{ OP(1, 1, 1, OP_ADD_L_2),  0,  8, 48 },
	{ OP(1, 1, 1, OP_SUB_L_2),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t long_binop_3a_statements[] = {
	{ OP(1, 1, 1, OP_MUL_L_3), 0,  8, 16 },
	{ OP(1, 1, 1, OP_MUL_L_1), 6, 14, 22 },
	{ OP(1, 1, 1, OP_DIV_L_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_DIV_L_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_REM_L_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_REM_L_1), 6, 14, 38 },
	{ OP(1, 1, 1, OP_MOD_L_3), 0,  8, 40 },
	{ OP(1, 1, 1, OP_MOD_L_1), 6, 14, 46 },
	{ OP(1, 1, 1, OP_ADD_L_3), 0,  8, 48 },
	{ OP(1, 1, 1, OP_ADD_L_1), 6, 14, 54 },
	{ OP(1, 1, 1, OP_SUB_L_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_SUB_L_1), 6, 14, 62 },
};

static dstatement_t long_binop_3b_statements[] = {
	{ OP(1, 1, 1, OP_MUL_L_1), 0,  8, 16 },
	{ OP(1, 1, 1, OP_MUL_L_3), 2, 10, 18 },
	{ OP(1, 1, 1, OP_DIV_L_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_DIV_L_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_REM_L_1), 0,  8, 32 },
	{ OP(1, 1, 1, OP_REM_L_3), 2, 10, 34 },
	{ OP(1, 1, 1, OP_MOD_L_1), 0,  8, 40 },
	{ OP(1, 1, 1, OP_MOD_L_3), 2, 10, 42 },
	{ OP(1, 1, 1, OP_ADD_L_1), 0,  8, 48 },
	{ OP(1, 1, 1, OP_ADD_L_3), 2, 10, 50 },
	{ OP(1, 1, 1, OP_SUB_L_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_SUB_L_3), 2, 10, 58 },
};

static dstatement_t long_binop_4_statements[] = {
	{ OP(1, 1, 1, OP_MUL_L_4), 0, 8, 16 },
	{ OP(1, 1, 1, OP_DIV_L_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_REM_L_4), 0, 8, 32 },
	{ OP(1, 1, 1, OP_MOD_L_4), 0, 8, 40 },
	{ OP(1, 1, 1, OP_ADD_L_4), 0, 8, 48 },
	{ OP(1, 1, 1, OP_SUB_L_4), 0, 8, 56 },
};

static pr_lvec4_t long_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_lvec4_t long_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{ -1,  0,  0, -1},
	{  0, -1,  0,  0},
	{  0,  0, -1,  0},
	{  0, -1, -1,  0},
	{ -1,  0, -1, -1},
	{ -1, -1,  0, -1},
};

static dstatement_t long_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_EQ_L_1),  0,  8, 16 },
	{ OP(1, 1, 1, OP_LT_L_1),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_L_1),  0,  8, 32 },
	{ OP(1, 1, 1, OP_NE_L_1),  0,  8, 40 },
	{ OP(1, 1, 1, OP_GE_L_1),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_L_1),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t long_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_EQ_L_2),  0,  8, 16 },
	{ OP(1, 1, 1, OP_LT_L_2),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_L_2),  0,  8, 32 },
	{ OP(1, 1, 1, OP_NE_L_2),  0,  8, 40 },
	{ OP(1, 1, 1, OP_GE_L_2),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_L_2),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t long_cmpop_3a_statements[] = {
	{ OP(1, 1, 1, OP_EQ_L_3), 0,  8, 16 },
	{ OP(1, 1, 1, OP_EQ_L_1), 6, 14, 22 },
	{ OP(1, 1, 1, OP_LT_L_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_L_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_GT_L_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_L_1), 6, 14, 38 },
	{ OP(1, 1, 1, OP_NE_L_3), 0,  8, 40 },
	{ OP(1, 1, 1, OP_NE_L_1), 6, 14, 46 },
	{ OP(1, 1, 1, OP_GE_L_3), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_L_1), 6, 14, 54 },
	{ OP(1, 1, 1, OP_LE_L_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_L_1), 6, 14, 62 },
};

static dstatement_t long_cmpop_3b_statements[] = {
	{ OP(1, 1, 1, OP_EQ_L_1), 0,  8, 16 },
	{ OP(1, 1, 1, OP_EQ_L_3), 2, 10, 18 },
	{ OP(1, 1, 1, OP_LT_L_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_L_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_GT_L_1), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_L_3), 2, 10, 34 },
	{ OP(1, 1, 1, OP_NE_L_1), 0,  8, 40 },
	{ OP(1, 1, 1, OP_NE_L_3), 2, 10, 42 },
	{ OP(1, 1, 1, OP_GE_L_1), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_L_3), 2, 10, 50 },
	{ OP(1, 1, 1, OP_LE_L_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_L_3), 2, 10, 58 },
};

static dstatement_t long_cmpop_4_statements[] = {
	{ OP(1, 1, 1, OP_EQ_L_4), 0, 8, 16 },
	{ OP(1, 1, 1, OP_LT_L_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_GT_L_4), 0, 8, 32 },
	{ OP(1, 1, 1, OP_NE_L_4), 0, 8, 40 },
	{ OP(1, 1, 1, OP_GE_L_4), 0, 8, 48 },
	{ OP(1, 1, 1, OP_LE_L_4), 0, 8, 56 },
};

test_t tests[] = {
	{
		.desc = "long binop 1",
		.extra_globals = 8 * 1,
		.num_globals = num_globals(long_binop_init,long_binop_expect),
		.num_statements = num_statements (long_binop_1_statements),
		.statements = long_binop_1_statements,
		.init_globals = (pr_int_t *) long_binop_init,
		.expect_globals = (pr_int_t *) long_binop_expect,
	},
	{
		.desc = "long binop 2",
		.extra_globals = 8 * 1,
		.num_globals = num_globals(long_binop_init,long_binop_expect),
		.num_statements = num_statements (long_binop_2_statements),
		.statements = long_binop_2_statements,
		.init_globals = (pr_int_t *) long_binop_init,
		.expect_globals = (pr_int_t *) long_binop_expect,
	},
	{
		.desc = "long binop 3a",
		.extra_globals = 8 * 1,
		.num_globals = num_globals(long_binop_init,long_binop_expect),
		.num_statements = num_statements (long_binop_3a_statements),
		.statements = long_binop_3a_statements,
		.init_globals = (pr_int_t *) long_binop_init,
		.expect_globals = (pr_int_t *) long_binop_expect,
	},
	{
		.desc = "long binop 3b",
		.extra_globals = 8 * 1,
		.num_globals = num_globals(long_binop_init,long_binop_expect),
		.num_statements = num_statements (long_binop_3b_statements),
		.statements = long_binop_3b_statements,
		.init_globals = (pr_int_t *) long_binop_init,
		.expect_globals = (pr_int_t *) long_binop_expect,
	},
	{
		.desc = "long binop 4",
		.extra_globals = 8 * 1,
		.num_globals = num_globals(long_binop_init,long_binop_expect),
		.num_statements = num_statements (long_binop_4_statements),
		.statements = long_binop_4_statements,
		.init_globals = (pr_int_t *) long_binop_init,
		.expect_globals = (pr_int_t *) long_binop_expect,
	},
	{
		.desc = "long cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_cmpop_init,long_cmpop_expect),
		.num_statements = num_statements (long_cmpop_1_statements),
		.statements = long_cmpop_1_statements,
		.init_globals = (pr_int_t *) long_cmpop_init,
		.expect_globals = (pr_int_t *) long_cmpop_expect,
	},
	{
		.desc = "long cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_cmpop_init,long_cmpop_expect),
		.num_statements = num_statements (long_cmpop_2_statements),
		.statements = long_cmpop_2_statements,
		.init_globals = (pr_int_t *) long_cmpop_init,
		.expect_globals = (pr_int_t *) long_cmpop_expect,
	},
	{
		.desc = "long cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_cmpop_init,long_cmpop_expect),
		.num_statements = num_statements (long_cmpop_3a_statements),
		.statements = long_cmpop_3a_statements,
		.init_globals = (pr_int_t *) long_cmpop_init,
		.expect_globals = (pr_int_t *) long_cmpop_expect,
	},
	{
		.desc = "long cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_cmpop_init,long_cmpop_expect),
		.num_statements = num_statements (long_cmpop_3b_statements),
		.statements = long_cmpop_3b_statements,
		.init_globals = (pr_int_t *) long_cmpop_init,
		.expect_globals = (pr_int_t *) long_cmpop_expect,
	},
	{
		.desc = "long cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_cmpop_init,long_cmpop_expect),
		.num_statements = num_statements (long_cmpop_4_statements),
		.statements = long_cmpop_4_statements,
		.init_globals = (pr_int_t *) long_cmpop_init,
		.expect_globals = (pr_int_t *) long_cmpop_expect,
	},
};

#include "main.c"
