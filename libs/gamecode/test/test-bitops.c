#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t int_bitop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},

	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
};

static pr_ivec4_t int_bitop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},

	{  5,  1,  1, -5},
	{  5, -1, -1, -5},
	{  0, -2, -2,  0},
	{ -6,  4, -6,  4},
};

static dstatement_t int_bitop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 24 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 24, -1, 24 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 24 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 24, 1 },

	{ OP(1, 1, 1, OP_BITAND_I_1), 0, 4,  8},
	{ OP(1, 1, 1, OP_BITOR_I_1),  0, 4, 12},
	{ OP(1, 1, 1, OP_BITXOR_I_1), 0, 4, 16},
	{ OP(1, 1, 1, OP_BITNOT_I_1), 0, 4, 20},

	{ OP(0, 0, 0, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t int_bitop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 24 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 24, -2, 24 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 24 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 24, 1 },

	{ OP(1, 1, 1, OP_BITAND_I_2), 0, 4,  8},
	{ OP(1, 1, 1, OP_BITOR_I_2),  0, 4, 12},
	{ OP(1, 1, 1, OP_BITXOR_I_2), 0, 4, 16},
	{ OP(1, 1, 1, OP_BITNOT_I_2), 0, 4, 20},

	{ OP(0, 0, 0, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t int_bitop_3a_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_I_3), 0, 4,  8},
	{ OP(1, 1, 1, OP_BITAND_I_1), 3, 7, 11},
	{ OP(1, 1, 1, OP_BITOR_I_3),  0, 4, 12},
	{ OP(1, 1, 1, OP_BITOR_I_1),  3, 7, 15},
	{ OP(1, 1, 1, OP_BITXOR_I_3), 0, 4, 16},
	{ OP(1, 1, 1, OP_BITXOR_I_1), 3, 7, 19},
	{ OP(1, 1, 1, OP_BITNOT_I_3), 0, 4, 20},
	{ OP(1, 1, 1, OP_BITNOT_I_1), 3, 7, 23},
};

static dstatement_t int_bitop_3b_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_I_1), 0, 4,  8},
	{ OP(1, 1, 1, OP_BITAND_I_3), 1, 5,  9},
	{ OP(1, 1, 1, OP_BITOR_I_1),  0, 4, 12},
	{ OP(1, 1, 1, OP_BITOR_I_3),  1, 5, 13},
	{ OP(1, 1, 1, OP_BITXOR_I_1), 0, 4, 16},
	{ OP(1, 1, 1, OP_BITXOR_I_3), 1, 5, 17},
	{ OP(1, 1, 1, OP_BITNOT_I_1), 0, 4, 20},
	{ OP(1, 1, 1, OP_BITNOT_I_3), 1, 5, 21},
};

static dstatement_t int_bitop_4_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_I_4), 0, 4,  8},
	{ OP(1, 1, 1, OP_BITOR_I_4),  0, 4, 12},
	{ OP(1, 1, 1, OP_BITXOR_I_4), 0, 4, 16},
	{ OP(1, 1, 1, OP_BITNOT_I_4), 0, 4, 20},
};

static pr_lvec4_t long_bitop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},

	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
	{  0,  0,  0,  0},
};

static pr_lvec4_t long_bitop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},

	{  5,  1,  1, -5},
	{  5, -1, -1, -5},
	{  0, -2, -2,  0},
	{ -6,  4, -6,  4},
};

static dstatement_t long_bitop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 48 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   48, -2, 48 },	// dec index
	{ OP(0, 0, 0, OP_IFAE),   2,  0, 48 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 48,  1 },

	{ OP(1, 1, 1, OP_BITAND_L_1), 0, 8, 16},
	{ OP(1, 1, 1, OP_BITOR_L_1),  0, 8, 24},
	{ OP(1, 1, 1, OP_BITXOR_L_1), 0, 8, 32},
	{ OP(1, 1, 1, OP_BITNOT_L_1), 0, 8, 40},

	{ OP(0, 0, 0, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t long_bitop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 48 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   48, -4, 48 },	// dec index
	{ OP(0, 0, 0, OP_IFAE),   2,  0, 48 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 48,  1 },

	{ OP(1, 1, 1, OP_BITAND_L_2), 0, 8, 16},
	{ OP(1, 1, 1, OP_BITOR_L_2),  0, 8, 24},
	{ OP(1, 1, 1, OP_BITXOR_L_2), 0, 8, 32},
	{ OP(1, 1, 1, OP_BITNOT_L_2), 0, 8, 40},

	{ OP(0, 0, 0, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t long_bitop_3a_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_L_3), 0,  8, 16},
	{ OP(1, 1, 1, OP_BITAND_L_1), 6, 14, 22},
	{ OP(1, 1, 1, OP_BITOR_L_3),  0,  8, 24},
	{ OP(1, 1, 1, OP_BITOR_L_1),  6, 14, 30},
	{ OP(1, 1, 1, OP_BITXOR_L_3), 0,  8, 32},
	{ OP(1, 1, 1, OP_BITXOR_L_1), 6, 14, 38},
	{ OP(1, 1, 1, OP_BITNOT_L_3), 0,  8, 40},
	{ OP(1, 1, 1, OP_BITNOT_L_1), 6, 14, 46},
};

static dstatement_t long_bitop_3b_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_L_1), 0,  8, 16},
	{ OP(1, 1, 1, OP_BITAND_L_3), 2, 10, 18},
	{ OP(1, 1, 1, OP_BITOR_L_1),  0,  8, 24},
	{ OP(1, 1, 1, OP_BITOR_L_3),  2, 10, 26},
	{ OP(1, 1, 1, OP_BITXOR_L_1), 0,  8, 32},
	{ OP(1, 1, 1, OP_BITXOR_L_3), 2, 10, 34},
	{ OP(1, 1, 1, OP_BITNOT_L_1), 0,  8, 40},
	{ OP(1, 1, 1, OP_BITNOT_L_3), 2, 10, 42},
};

static dstatement_t long_bitop_4_statements[] = {
	{ OP(1, 1, 1, OP_BITAND_L_4), 0, 8, 16},
	{ OP(1, 1, 1, OP_BITOR_L_4),  0, 8, 24},
	{ OP(1, 1, 1, OP_BITXOR_L_4), 0, 8, 32},
	{ OP(1, 1, 1, OP_BITNOT_L_4), 0, 8, 40},
};

test_t tests[] = {
	{
		.desc = "int bitop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_bitop_init,int_bitop_expect),
		.num_statements = num_statements (int_bitop_1_statements),
		.statements = int_bitop_1_statements,
		.init_globals = (pr_int_t *) int_bitop_init,
		.expect_globals = (pr_int_t *) int_bitop_expect,
	},
	{
		.desc = "int bitop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_bitop_init,int_bitop_expect),
		.num_statements = num_statements (int_bitop_2_statements),
		.statements = int_bitop_2_statements,
		.init_globals = (pr_int_t *) int_bitop_init,
		.expect_globals = (pr_int_t *) int_bitop_expect,
	},
	{
		.desc = "int bitop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_bitop_init,int_bitop_expect),
		.num_statements = num_statements (int_bitop_3a_statements),
		.statements = int_bitop_3a_statements,
		.init_globals = (pr_int_t *) int_bitop_init,
		.expect_globals = (pr_int_t *) int_bitop_expect,
	},
	{
		.desc = "int bitop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_bitop_init,int_bitop_expect),
		.num_statements = num_statements (int_bitop_3b_statements),
		.statements = int_bitop_3b_statements,
		.init_globals = (pr_int_t *) int_bitop_init,
		.expect_globals = (pr_int_t *) int_bitop_expect,
	},
	{
		.desc = "int bitop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_bitop_init,int_bitop_expect),
		.num_statements = num_statements (int_bitop_4_statements),
		.statements = int_bitop_4_statements,
		.init_globals = (pr_int_t *) int_bitop_init,
		.expect_globals = (pr_int_t *) int_bitop_expect,
	},
	{
		.desc = "long bitop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_bitop_init,long_bitop_expect),
		.num_statements = num_statements (long_bitop_1_statements),
		.statements = long_bitop_1_statements,
		.init_globals = (pr_int_t *) long_bitop_init,
		.expect_globals = (pr_int_t *) long_bitop_expect,
	},
	{
		.desc = "long bitop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_bitop_init,long_bitop_expect),
		.num_statements = num_statements (long_bitop_2_statements),
		.statements = long_bitop_2_statements,
		.init_globals = (pr_int_t *) long_bitop_init,
		.expect_globals = (pr_int_t *) long_bitop_expect,
	},
	{
		.desc = "long bitop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_bitop_init,long_bitop_expect),
		.num_statements = num_statements (long_bitop_3a_statements),
		.statements = long_bitop_3a_statements,
		.init_globals = (pr_int_t *) long_bitop_init,
		.expect_globals = (pr_int_t *) long_bitop_expect,
	},
	{
		.desc = "long bitop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_bitop_init,long_bitop_expect),
		.num_statements = num_statements (long_bitop_3b_statements),
		.statements = long_bitop_3b_statements,
		.init_globals = (pr_int_t *) long_bitop_init,
		.expect_globals = (pr_int_t *) long_bitop_expect,
	},
	{
		.desc = "long bitop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(long_bitop_init,long_bitop_expect),
		.num_statements = num_statements (long_bitop_4_statements),
		.statements = long_bitop_4_statements,
		.init_globals = (pr_int_t *) long_bitop_init,
		.expect_globals = (pr_int_t *) long_bitop_expect,
	},
};

#include "main.c"
