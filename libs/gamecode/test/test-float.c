#include "head.c"

#include "QF/mathlib.h"

#define sq(x) ((float)(x)*(float)(x))

static pr_vec4_t float_binop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_vec4_t float_binop_expect[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  15, -15, -15, 15},
	{  1.666666627, -1.666666627, -1.666666627, 1.666666627},
	{  2, -2, 2, -2},
	{  2,  1, -1, -2},
	{  8, -2, 2, -8},
	{  2, -8, 8, -2},
};

static dstatement_t float_binop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_MUL_F_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_F_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_F_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_F_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_F_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_F_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t float_binop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_MUL_F_2), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_F_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_F_2), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_F_2), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_F_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_F_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t float_binop_3a_statements[] = {
	{ OP(1, 1, 1, OP_MUL_F_3), 0, 4,  8 },
	{ OP(1, 1, 1, OP_MUL_F_1), 3, 7, 11 },
	{ OP(1, 1, 1, OP_DIV_F_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_DIV_F_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_REM_F_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_REM_F_1), 3, 7, 19 },
	{ OP(1, 1, 1, OP_MOD_F_3), 0, 4, 20 },
	{ OP(1, 1, 1, OP_MOD_F_1), 3, 7, 23 },
	{ OP(1, 1, 1, OP_ADD_F_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_ADD_F_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_SUB_F_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_SUB_F_1), 3, 7, 31 },
};

static dstatement_t float_binop_3b_statements[] = {
	{ OP(1, 1, 1, OP_MUL_F_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_MUL_F_3), 1, 5,  9 },
	{ OP(1, 1, 1, OP_DIV_F_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_DIV_F_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_REM_F_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_REM_F_3), 1, 5, 17 },
	{ OP(1, 1, 1, OP_MOD_F_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_MOD_F_3), 1, 5, 21 },
	{ OP(1, 1, 1, OP_ADD_F_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_ADD_F_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_SUB_F_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_SUB_F_3), 1, 5, 29 },
};

static dstatement_t float_binop_4_statements[] = {
	{ OP(1, 1, 1, OP_MUL_F_4), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_F_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_F_4), 0, 4, 16 },
	{ OP(1, 1, 1, OP_MOD_F_4), 0, 4, 20 },
	{ OP(1, 1, 1, OP_ADD_F_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_SUB_F_4), 0, 4, 28 },
};

static pr_vec4_t float_cossin_init[] = {
	{ 1, 2, 3, 4 },						//  0: output
	{ M_PI/6, 0, 0, 0 },				//  4: x
	{ 1, 2, 0, 0 },						//  8: f
	{ 1, 1, 0, 25 },					// 12: f inc and f0 max
	{ 0, 0, 0, 0 },						// 16: x2 -> [xx, xx]
	// { }								// 20: xn
};

static pr_vec4_t float_cossin_expect[] = {
	{ 0.866025388, 0.5, 0, 0 },			//  0: output
	{ M_PI/6, 0, 0, 0 },				//  4: x
	{ 25, 26, 0, 0 },					//  8: f
	{ 1, 1, 0, 25 },					// 12: f inc and f0 max
	{ -sq(M_PI/6), -sq(M_PI/6), 0, 0 },	// 16: x2 -> [xx, xx]
};

static dstatement_t float_cossin_statements[] = {
	{ OP(0, 0, 0, OP_STORE_A_1), 21, 0, 4 },	// init xn -> [?, x]
	{ OP(0, 0, 0, OP_STORE_A_1), 20, 0, 8 },	// init xn -> [1, x]
	{ OP(0, 0, 0, OP_SWIZZLE_F_4), 4, 0xc000, 16 },// init x2 -> [x, x, 0, 0]
	{ OP(0, 0, 0, OP_MUL_F_2),  16, 16, 16 },	// x2 -> [x*x, x*x, 0, 0]
	{ OP(0, 0, 0, OP_SWIZZLE_F_4), 16, 0xc3e4, 16 },// init x2 -> -x2
	{ OP(0, 0, 0, OP_SUB_F_4), 0, 0, 0 },		// init acc (output) to 0
// loop:
	{ OP(0, 0, 0, OP_ADD_F_2), 0, 20, 0 },		// acc += xn
	{ OP(0, 0, 0, OP_MUL_F_2), 20, 16, 20 },	// xn *= x2
	{ OP(0, 0, 0, OP_DIV_F_2), 20, 8, 20 },		// xn /= f
	{ OP(0, 0, 0, OP_ADD_F_2), 8, 12, 8 },		// f += inc
	{ OP(0, 0, 0, OP_DIV_F_2), 20, 8, 20 },		// xn /= f
	{ OP(0, 0, 0, OP_ADD_F_2), 8, 12, 8 },		// f += inc
	{ OP(0, 0, 0, OP_LT_F_1), 8, 15, 23 },		// f0 < fmax
	{ OP(0, 0, 0, OP_IFNZ), -7, 0, 23 },		// f0 < fmax
};

static pr_vec4_t float_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

// 5.0 as 32-bit int
#define F 0x40a00000
#define mF 0xc0a00000
static pr_ivec4_t float_cmpop_expect[] = {
	{  F, mF,  F, mF},
	{  F,  F, mF, mF},
	{ -1,  0,  0, -1},
	{  0, -1,  0,  0},
	{  0,  0, -1,  0},
	{  0, -1, -1,  0},
	{ -1,  0, -1, -1},
	{ -1, -1,  0, -1},
};

static dstatement_t float_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_EQ_F_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_F_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_F_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_F_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_F_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_F_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t float_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_EQ_F_2), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_F_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_F_2), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_F_2), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_F_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_F_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t float_cmpop_3a_statements[] = {
	{ OP(1, 1, 1, OP_EQ_F_3), 0, 4,  8 },
	{ OP(1, 1, 1, OP_EQ_F_1), 3, 7, 11 },
	{ OP(1, 1, 1, OP_LT_F_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_F_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_GT_F_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_F_1), 3, 7, 19 },
	{ OP(1, 1, 1, OP_NE_F_3), 0, 4, 20 },
	{ OP(1, 1, 1, OP_NE_F_1), 3, 7, 23 },
	{ OP(1, 1, 1, OP_GE_F_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_F_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_LE_F_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_F_1), 3, 7, 31 },
};

static dstatement_t float_cmpop_3b_statements[] = {
	{ OP(1, 1, 1, OP_EQ_F_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_EQ_F_3), 1, 5,  9 },
	{ OP(1, 1, 1, OP_LT_F_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_F_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_GT_F_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_F_3), 1, 5, 17 },
	{ OP(1, 1, 1, OP_NE_F_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_NE_F_3), 1, 5, 21 },
	{ OP(1, 1, 1, OP_GE_F_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_F_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_LE_F_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_F_3), 1, 5, 29 },
};

static dstatement_t float_cmpop_4_statements[] = {
	{ OP(1, 1, 1, OP_EQ_F_4), 0, 4,  8 },
	{ OP(1, 1, 1, OP_LT_F_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_F_4), 0, 4, 16 },
	{ OP(1, 1, 1, OP_NE_F_4), 0, 4, 20 },
	{ OP(1, 1, 1, OP_GE_F_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_F_4), 0, 4, 28 },
};

test_t tests[] = {
	{
		.desc = "float binop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_binop_init,float_binop_expect),
		.num_statements = num_statements (float_binop_1_statements),
		.statements = float_binop_1_statements,
		.init_globals = (pr_int_t *) float_binop_init,
		.expect_globals = (pr_int_t *) float_binop_expect,
	},
	{
		.desc = "float binop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_binop_init,float_binop_expect),
		.num_statements = num_statements (float_binop_2_statements),
		.statements = float_binop_2_statements,
		.init_globals = (pr_int_t *) float_binop_init,
		.expect_globals = (pr_int_t *) float_binop_expect,
	},
	{
		.desc = "float binop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_binop_init,float_binop_expect),
		.num_statements = num_statements (float_binop_3a_statements),
		.statements = float_binop_3a_statements,
		.init_globals = (pr_int_t *) float_binop_init,
		.expect_globals = (pr_int_t *) float_binop_expect,
	},
	{
		.desc = "float binop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_binop_init,float_binop_expect),
		.num_statements = num_statements (float_binop_3b_statements),
		.statements = float_binop_3b_statements,
		.init_globals = (pr_int_t *) float_binop_init,
		.expect_globals = (pr_int_t *) float_binop_expect,
	},
	{
		.desc = "float binop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_binop_init,float_binop_expect),
		.num_statements = num_statements (float_binop_4_statements),
		.statements = float_binop_4_statements,
		.init_globals = (pr_int_t *) float_binop_init,
		.expect_globals = (pr_int_t *) float_binop_expect,
	},
	{
		.desc = "float cos sin",
		.extra_globals = 4 * 1,
		.num_globals = num_globals (float_cossin_init, float_cossin_expect),
		.num_statements = num_statements (float_cossin_statements),
		.statements = float_cossin_statements,
		.init_globals = (pr_int_t *) float_cossin_init,
		.expect_globals = (pr_int_t *) float_cossin_expect,
	},
	{
		.desc = "float cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_cmpop_init,float_cmpop_expect),
		.num_statements = num_statements (float_cmpop_1_statements),
		.statements = float_cmpop_1_statements,
		.init_globals = (pr_int_t *) float_cmpop_init,
		.expect_globals = (pr_int_t *) float_cmpop_expect,
	},
	{
		.desc = "float cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_cmpop_init,float_cmpop_expect),
		.num_statements = num_statements (float_cmpop_2_statements),
		.statements = float_cmpop_2_statements,
		.init_globals = (pr_int_t *) float_cmpop_init,
		.expect_globals = (pr_int_t *) float_cmpop_expect,
	},
	{
		.desc = "float cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_cmpop_init,float_cmpop_expect),
		.num_statements = num_statements (float_cmpop_3a_statements),
		.statements = float_cmpop_3a_statements,
		.init_globals = (pr_int_t *) float_cmpop_init,
		.expect_globals = (pr_int_t *) float_cmpop_expect,
	},
	{
		.desc = "float cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_cmpop_init,float_cmpop_expect),
		.num_statements = num_statements (float_cmpop_3b_statements),
		.statements = float_cmpop_3b_statements,
		.init_globals = (pr_int_t *) float_cmpop_init,
		.expect_globals = (pr_int_t *) float_cmpop_expect,
	},
	{
		.desc = "float cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(float_cmpop_init,float_cmpop_expect),
		.num_statements = num_statements (float_cmpop_4_statements),
		.statements = float_cmpop_4_statements,
		.init_globals = (pr_int_t *) float_cmpop_init,
		.expect_globals = (pr_int_t *) float_cmpop_expect,
	},
};

#include "main.c"
