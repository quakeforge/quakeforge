#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t int_binop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ivec4_t int_binop_expect[] = {
	{  5,  -5,   5, -5},
	{  3,   3,  -3, -3},
	{ 15, -15, -15, 15},
	{  1,  -1,  -1,  1},
	{  2,  -2,   2, -2},
	{  2,   1,  -1, -2},
	{  8,  -2,   2, -8},
	{  2,  -8,   8, -2},
};

static dstatement_t int_binop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_MUL_I_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_I_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_I_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_I_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t int_binop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_MUL_I_2), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_I_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_I_2), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_I_2), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_I_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_I_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t int_binop_3a_statements[] = {
	{ OP(1, 1, 1, OP_MUL_I_3), 0, 4,  8 },
	{ OP(1, 1, 1, OP_MUL_I_1), 3, 7, 11 },
	{ OP(1, 1, 1, OP_DIV_I_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_DIV_I_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_REM_I_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_REM_I_1), 3, 7, 19 },
	{ OP(1, 1, 1, OP_MOD_I_3), 0, 4, 20 },
	{ OP(1, 1, 1, OP_MOD_I_1), 3, 7, 23 },
	{ OP(1, 1, 1, OP_ADD_I_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_ADD_I_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_SUB_I_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_SUB_I_1), 3, 7, 31 },
};

static dstatement_t int_binop_3b_statements[] = {
	{ OP(1, 1, 1, OP_MUL_I_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_MUL_I_3), 1, 5,  9 },
	{ OP(1, 1, 1, OP_DIV_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_DIV_I_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_REM_I_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_REM_I_3), 1, 5, 17 },
	{ OP(1, 1, 1, OP_MOD_I_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_MOD_I_3), 1, 5, 21 },
	{ OP(1, 1, 1, OP_ADD_I_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_ADD_I_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_SUB_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_SUB_I_3), 1, 5, 29 },
};

static dstatement_t int_binop_4_statements[] = {
	{ OP(1, 1, 1, OP_MUL_I_4), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_I_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_I_4), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_I_4), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_I_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_I_4), 0, 4, 28 },
};

static pr_ivec4_t int_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ivec4_t int_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{ -1,  0,  0, -1},
	{  0, -1,  0,  0},
	{  0,  0, -1,  0},
	{  0, -1, -1,  0},
	{ -1,  0, -1, -1},
	{ -1, -1,  0, -1},
};

static dstatement_t int_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_EQ_I_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_I_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_I_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_I_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t int_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_EQ_I_2), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_I_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_I_2), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_I_2), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_I_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_I_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t int_cmpop_3a_statements[] = {
	{ OP(1, 1, 1, OP_EQ_I_3), 0, 4,  8 },
	{ OP(1, 1, 1, OP_EQ_I_1), 3, 7, 11 },
	{ OP(1, 1, 1, OP_LT_I_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_I_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_GT_I_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_I_1), 3, 7, 19 },
	{ OP(1, 1, 1, OP_NE_I_3), 0, 4, 20 },
	{ OP(1, 1, 1, OP_NE_I_1), 3, 7, 23 },
	{ OP(1, 1, 1, OP_GE_I_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_I_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_LE_I_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_I_1), 3, 7, 31 },
};

static dstatement_t int_cmpop_3b_statements[] = {
	{ OP(1, 1, 1, OP_EQ_I_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_EQ_I_3), 1, 5,  9 },
	{ OP(1, 1, 1, OP_LT_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_I_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_GT_I_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_I_3), 1, 5, 17 },
	{ OP(1, 1, 1, OP_NE_I_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_NE_I_3), 1, 5, 21 },
	{ OP(1, 1, 1, OP_GE_I_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_I_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_LE_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_I_3), 1, 5, 29 },
};

static dstatement_t int_cmpop_4_statements[] = {
	{ OP(1, 1, 1, OP_EQ_I_4), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_I_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_I_4), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_I_4), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_I_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_I_4), 0, 4, 28 },
};

test_t tests[] = {
	{
		.desc = "int binop 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_binop_init,int_binop_expect),
		.num_statements = num_statements (int_binop_1_statements),
		.statements = int_binop_1_statements,
		.init_globals = (pr_int_t *) int_binop_init,
		.expect_globals = (pr_int_t *) int_binop_expect,
	},
	{
		.desc = "int binop 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_binop_init,int_binop_expect),
		.num_statements = num_statements (int_binop_2_statements),
		.statements = int_binop_2_statements,
		.init_globals = (pr_int_t *) int_binop_init,
		.expect_globals = (pr_int_t *) int_binop_expect,
	},
	{
		.desc = "int binop 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_binop_init,int_binop_expect),
		.num_statements = num_statements (int_binop_3a_statements),
		.statements = int_binop_3a_statements,
		.init_globals = (pr_int_t *) int_binop_init,
		.expect_globals = (pr_int_t *) int_binop_expect,
	},
	{
		.desc = "int binop 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_binop_init,int_binop_expect),
		.num_statements = num_statements (int_binop_3b_statements),
		.statements = int_binop_3b_statements,
		.init_globals = (pr_int_t *) int_binop_init,
		.expect_globals = (pr_int_t *) int_binop_expect,
	},
	{
		.desc = "int binop 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_binop_init,int_binop_expect),
		.num_statements = num_statements (int_binop_4_statements),
		.statements = int_binop_4_statements,
		.init_globals = (pr_int_t *) int_binop_init,
		.expect_globals = (pr_int_t *) int_binop_expect,
	},
	{
		.desc = "int cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_cmpop_init,int_cmpop_expect),
		.num_statements = num_statements (int_cmpop_1_statements),
		.statements = int_cmpop_1_statements,
		.init_globals = (pr_int_t *) int_cmpop_init,
		.expect_globals = (pr_int_t *) int_cmpop_expect,
	},
	{
		.desc = "int cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_cmpop_init,int_cmpop_expect),
		.num_statements = num_statements (int_cmpop_2_statements),
		.statements = int_cmpop_2_statements,
		.init_globals = (pr_int_t *) int_cmpop_init,
		.expect_globals = (pr_int_t *) int_cmpop_expect,
	},
	{
		.desc = "int cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_cmpop_init,int_cmpop_expect),
		.num_statements = num_statements (int_cmpop_3a_statements),
		.statements = int_cmpop_3a_statements,
		.init_globals = (pr_int_t *) int_cmpop_init,
		.expect_globals = (pr_int_t *) int_cmpop_expect,
	},
	{
		.desc = "int cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_cmpop_init,int_cmpop_expect),
		.num_statements = num_statements (int_cmpop_3b_statements),
		.statements = int_cmpop_3b_statements,
		.init_globals = (pr_int_t *) int_cmpop_init,
		.expect_globals = (pr_int_t *) int_cmpop_expect,
	},
	{
		.desc = "int cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(int_cmpop_init,int_cmpop_expect),
		.num_statements = num_statements (int_cmpop_4_statements),
		.statements = int_cmpop_4_statements,
		.init_globals = (pr_int_t *) int_cmpop_init,
		.expect_globals = (pr_int_t *) int_cmpop_expect,
	},
};

#include "main.c"
