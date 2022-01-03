#include "head.c"

static pr_vec4_t float_globals_init[] = {
	{3, 4, 5, 12},
	{0, 0, 0, 0},
	{1, 2, 3, 8},
	{4, 5, 6, 8},
	{0, 0, 0, 7},
	{0, 0, 0, 7},
	{1, 2, 3, 4},
	{5, 6, 7, 8},
	{2, 3, 4, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 7},
	{0, 0, 0, 7},
	{0, 0, 0, 7},
	{0, 0, 0, 7},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
};

static pr_vec4_t float_globals_expect[] = {
	{3, 4, 5, 12},
	{63, 63, -33, 56},
	{1, 2, 3, 8},
	{4, 5, 6, 8},

	{32, 32, 32, 7},
	{-3, 6, -3, 7},
	{1, 2, 3, 4},
	{5, 6, 7, 8},

	{2, 3, 4, 0},
	{70, 70, 70, 70},
	{24, 48, 48, -6},
	{36, 102, 120, 7},

	{52, 70, 136, 7},
	{160, 160, 160, 160},
	{9, 10, 17, 20},
	{-1, -2, -3, 4},

	{-1, -2, -3, 4},
};

static dstatement_t store_A_statements[] = {
	{ OP(0, 0, 0, OP_DOT_CC_F), 0, 2, 4 },
	{ OP(0, 0, 0, OP_MUL_CC_F), 0, 2, 6 },
	{ OP(0, 0, 0, OP_DOT_VV_F), 8, 12, 16 },
	{ OP(0, 0, 0, OP_CROSS_VV_F), 8, 12, 20 },
	{ OP(0, 0, 0, OP_DOT_QQ_F), 24, 28, 36 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 24, 28, 40 },
	{ OP(0, 0, 0, OP_MUL_QV_F), 24, 32, 44 },
	{ OP(0, 0, 0, OP_MUL_VQ_F), 32, 24, 48 },

	{ OP(0, 0, 0, OP_DOT_QQ_F), 24, 32, 52 },
	{ OP(0, 0, 0, OP_SWIZZLE_F), 24, 0x07e4, 60 },
	{ OP(0, 0, 0, OP_DOT_QQ_F), 52, 60, 52 },

	{ OP(0, 0, 0, OP_SWIZZLE_F), 24, 0x07e4, 64 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 60, 32, 56 },
	{ OP(0, 0, 0, OP_DOT_QQ_F), 56, 24, 52 },
};

test_t tests[] = {
	{
		.desc = "float vector",
		.num_globals = 4*num_globals(float_globals_init, float_globals_expect),
		.num_statements = num_statements (store_A_statements),
		.statements = store_A_statements,
		.init_globals = (pr_int_t *) float_globals_init,
		.expect_globals = (pr_int_t *) float_globals_expect,
	},
};

#include "main.c"
