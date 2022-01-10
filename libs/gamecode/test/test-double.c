#include "head.c"

#include "QF/mathlib.h"

#define sq(x) ((x)*(x))

static pr_dvec4_t double_binop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_dvec4_t double_binop_expect[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  15, -15, -15, 15},
	{  5.0/3, -5.0/3, -5.0/3, 5.0/3},
	{  2, -2, 2, -2},
	{  2,  1, -1, -2},
	{  8, -2, 2, -8},
	{  2, -8, 8, -2},
};

static dstatement_t double_binop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_MUL_D_1),  0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_D_1),  0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_D_1),  0,  8, 32 },
	{ OP(1, 1, 1, OP_MOD_D_1),  0,  8, 40 },
	{ OP(1, 1, 1, OP_ADD_D_1),  0,  8, 48 },
	{ OP(1, 1, 1, OP_SUB_D_1),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t double_binop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE_A),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	{ OP(1, 1, 1, OP_MUL_D_2),  0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_D_2),  0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_D_2),  0,  8, 32 },
	{ OP(1, 1, 1, OP_MOD_D_2),  0,  8, 40 },
	{ OP(1, 1, 1, OP_ADD_D_2),  0,  8, 48 },
	{ OP(1, 1, 1, OP_SUB_D_2),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -10,  0,  0 },
};

static dstatement_t double_binop_3a_statements[] = {
	{ OP(1, 1, 1, OP_MUL_D_3), 0,  8, 16 },
	{ OP(1, 1, 1, OP_MUL_D_1), 6, 14, 22 },
	{ OP(1, 1, 1, OP_DIV_D_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_DIV_D_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_REM_D_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_REM_D_1), 6, 14, 38 },
	{ OP(1, 1, 1, OP_MOD_D_3), 0,  8, 40 },
	{ OP(1, 1, 1, OP_MOD_D_1), 6, 14, 46 },
	{ OP(1, 1, 1, OP_ADD_D_3), 0,  8, 48 },
	{ OP(1, 1, 1, OP_ADD_D_1), 6, 14, 54 },
	{ OP(1, 1, 1, OP_SUB_D_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_SUB_D_1), 6, 14, 62 },
};

static dstatement_t double_binop_3b_statements[] = {
	{ OP(1, 1, 1, OP_MUL_D_1), 0,  8, 16 },
	{ OP(1, 1, 1, OP_MUL_D_3), 2, 10, 18 },
	{ OP(1, 1, 1, OP_DIV_D_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_DIV_D_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_REM_D_1), 0,  8, 32 },
	{ OP(1, 1, 1, OP_REM_D_3), 2, 10, 34 },
	{ OP(1, 1, 1, OP_MOD_D_1), 0,  8, 40 },
	{ OP(1, 1, 1, OP_MOD_D_3), 2, 10, 42 },
	{ OP(1, 1, 1, OP_ADD_D_1), 0,  8, 48 },
	{ OP(1, 1, 1, OP_ADD_D_3), 2, 10, 50 },
	{ OP(1, 1, 1, OP_SUB_D_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_SUB_D_3), 2, 10, 58 },
};

static dstatement_t double_binop_4_statements[] = {
	{ OP(1, 1, 1, OP_MUL_D_4), 0, 8, 16 },
	{ OP(1, 1, 1, OP_DIV_D_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_REM_D_4), 0, 8, 32 },
	{ OP(1, 1, 1, OP_MOD_D_4), 0, 8, 40 },
	{ OP(1, 1, 1, OP_ADD_D_4), 0, 8, 48 },
	{ OP(1, 1, 1, OP_SUB_D_4), 0, 8, 56 },
};

static pr_dvec4_t double_cossin_init[] = {
	{ 1, 2, 3, 4 },						//  0: output
	{ M_PI/6, 0, 0, 0 },				//  4: x
	{ 1, 2, 0, 0 },						//  8: f
	{ 1, 1, 0, 25 },					// 12: f inc and f0 max
	{ 0, 0, 0, 0 },						// 16: x2 -> [xx, xx]
	// { }								// 20: xn
};

static pr_dvec4_t double_cossin_expect[] = {
	{ 0.8660254037844386, 0.49999999999999994, 0, 0 },			//  0: output
	{ M_PI/6, 0, 0, 0 },				//  4: x
	{ 25, 26, 0, 0 },					//  8: f
	{ 1, 1, 0, 25 },					// 12: f inc and f0 max
	{ -sq(M_PI/6), -sq(M_PI/6), 0, 0 },	// 16: x2 -> [xx, xx]
};

static dstatement_t double_cossin_statements[] = {
	{ OP(0, 0, 0, OP_STORE_A_2), 42,   0,    8 },	// init xn -> [?, x]
	{ OP(0, 0, 0, OP_STORE_A_2), 40,   0,   16 },	// init xn -> [1, x]
	{ OP(0, 0, 0, OP_SWIZZLE_D),  8,0xc000, 32 },	// init x2 -> [x, x, 0, 0]
	{ OP(0, 0, 0, OP_MUL_D_2),   32,  32,   32 },	// x2 -> [x*x, x*x, 0, 0]
	{ OP(0, 0, 0, OP_SWIZZLE_D), 32,0xc3e4, 32 },	// init x2 -> -x2
	{ OP(0, 0, 0, OP_SUB_D_4),    0,   0,    0 },	// init acc (output) to 0
// loop:
	{ OP(0, 0, 0, OP_ADD_D_2),    0,  40,    0 },	// acc += xn
	{ OP(0, 0, 0, OP_MUL_D_2),   40,  32,   40 },	// xn *= x2
	{ OP(0, 0, 0, OP_DIV_D_2),   40,  16,   40 },	// xn /= f
	{ OP(0, 0, 0, OP_ADD_D_2),   16,  24,   16 },	// f += inc
	{ OP(0, 0, 0, OP_DIV_D_2),   40,  16,   40 },	// xn /= f
	{ OP(0, 0, 0, OP_ADD_D_2),   16,  24,   16 },	// f += inc
	{ OP(0, 0, 0, OP_LT_D_1),    16,  30,   46 },	// f0 < fmax
	{ OP(0, 0, 0, OP_IFNZ_A),    -7,   0,   46 },	// f0 < fmax
};

test_t tests[] = {
	{
		.desc = "double binop 1",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_binop_init,double_binop_expect),
		.num_statements = num_statements (double_binop_1_statements),
		.statements = double_binop_1_statements,
		.init_globals = (pr_int_t *) double_binop_init,
		.expect_globals = (pr_int_t *) double_binop_expect,
	},
	{
		.desc = "double binop 2",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_binop_init,double_binop_expect),
		.num_statements = num_statements (double_binop_2_statements),
		.statements = double_binop_2_statements,
		.init_globals = (pr_int_t *) double_binop_init,
		.expect_globals = (pr_int_t *) double_binop_expect,
	},
	{
		.desc = "double binop 3a",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_binop_init,double_binop_expect),
		.num_statements = num_statements (double_binop_3a_statements),
		.statements = double_binop_3a_statements,
		.init_globals = (pr_int_t *) double_binop_init,
		.expect_globals = (pr_int_t *) double_binop_expect,
	},
	{
		.desc = "double binop 3b",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_binop_init,double_binop_expect),
		.num_statements = num_statements (double_binop_3b_statements),
		.statements = double_binop_3b_statements,
		.init_globals = (pr_int_t *) double_binop_init,
		.expect_globals = (pr_int_t *) double_binop_expect,
	},
	{
		.desc = "double binop 4",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_binop_init,double_binop_expect),
		.num_statements = num_statements (double_binop_4_statements),
		.statements = double_binop_4_statements,
		.init_globals = (pr_int_t *) double_binop_init,
		.expect_globals = (pr_int_t *) double_binop_expect,
	},
	{
		.desc = "double cos sin",
		.extra_globals = 8 * 1,
		.num_globals = 8*num_globals(double_cossin_init,double_cossin_expect),
		.num_statements = num_statements (double_cossin_statements),
		.statements = double_cossin_statements,
		.init_globals = (pr_int_t *) double_cossin_init,
		.expect_globals = (pr_int_t *) double_cossin_expect,
	},
};

#include "main.c"
