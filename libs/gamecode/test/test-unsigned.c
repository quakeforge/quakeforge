#include "head.c"

#include "QF/mathlib.h"

static pr_uivec4_t uint_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_uivec4_t uint_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0,  0,  0},	// no unsigned EQ (redundant)
	{  0,  0, -1,  0},
	{  0, -1,  0,  0},
	{  0,  0,  0,  0},	// no unsigned NE (redundant)
	{ -1, -1,  0, -1},
	{ -1,  0, -1, -1},
};

static dstatement_t uint_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_1), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t uint_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_2), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t uint_cmpop_3a_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_u_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_GT_u_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_u_1), 3, 7, 19 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_u_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_LE_u_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_u_1), 3, 7, 31 },
};

static dstatement_t uint_cmpop_3b_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_u_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_GT_u_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_u_3), 1, 5, 17 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_u_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_LE_u_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_u_3), 1, 5, 29 },
};

static dstatement_t uint_cmpop_4_statements[] = {
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_4), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_4), 0, 4, 28 },
};

static pr_ulvec4_t ulong_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ulvec4_t ulong_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0,  0,  0},	// no unsigned EQ (redundant)
	{  0,  0, -1,  0},
	{  0, -1,  0,  0},
	{  0,  0,  0,  0},	// no unsigned NE (redundant)
	{ -1, -1,  0, -1},
	{ -1,  0, -1, -1},
};

static dstatement_t ulong_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_1),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_U_1),  0,  8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_1),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_U_1),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t ulong_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_2),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_U_2),  0,  8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_2),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_U_2),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t ulong_cmpop_3a_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_U_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_GT_U_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_U_1), 6, 14, 38 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_3), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_U_1), 6, 14, 54 },
	{ OP(1, 1, 1, OP_LE_U_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_U_1), 6, 14, 62 },
};

static dstatement_t ulong_cmpop_3b_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_U_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_GT_U_1), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_U_3), 2, 10, 34 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_1), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_U_3), 2, 10, 50 },
	{ OP(1, 1, 1, OP_LE_U_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_U_3), 2, 10, 58 },
};

static dstatement_t ulong_cmpop_4_statements[] = {
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_GT_U_4), 0, 8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_4), 0, 8, 48 },
	{ OP(1, 1, 1, OP_LE_U_4), 0, 8, 56 },
};

test_t tests[] = {
	{
		.desc = "uint cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_1_statements),
		.statements = uint_cmpop_1_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_2_statements),
		.statements = uint_cmpop_2_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_3a_statements),
		.statements = uint_cmpop_3a_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_3b_statements),
		.statements = uint_cmpop_3b_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_4_statements),
		.statements = uint_cmpop_4_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_1_statements),
		.statements = ulong_cmpop_1_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_2_statements),
		.statements = ulong_cmpop_2_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_3a_statements),
		.statements = ulong_cmpop_3a_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_3b_statements),
		.statements = ulong_cmpop_3b_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_4_statements),
		.statements = ulong_cmpop_4_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
};

#include "main.c"
