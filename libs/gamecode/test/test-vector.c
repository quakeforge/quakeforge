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
	{36, 102, 120, 0},
	{52, 70, 136, 0},
	{-1, -2, -3, 4},

	{-1, -2, -3, 4},
	{36, 102, 120, 0},
	{52, 70, 136, 0},
};

static dstatement_t float_vector_statements[] = {
	{ OP(0, 0, 0, OP_DOT_CC_F), 0, 2, 4 },
	{ OP(0, 0, 0, OP_MUL_CC_F), 0, 2, 6 },
	{ OP(0, 0, 0, OP_DOT_VV_F), 8, 12, 16 },
	{ OP(0, 0, 0, OP_CROSS_VV_F), 8, 12, 20 },
	{ OP(0, 0, 0, OP_DOT_QQ_F), 24, 28, 36 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 24, 28, 40 },
	{ OP(0, 0, 0, OP_MUL_QV_F), 24, 32, 44 },
	{ OP(0, 0, 0, OP_MUL_VQ_F), 32, 24, 48 },

	{ OP(0, 0, 0, OP_MUL_QQ_F), 24, 32, 52 },
	{ OP(0, 0, 0, OP_SWIZZLE_F), 24, 0x07e4, 60 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 52, 60, 52 },

	{ OP(0, 0, 0, OP_SWIZZLE_F), 24, 0x07e4, 64 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 64, 32, 56 },
	{ OP(0, 0, 0, OP_MUL_QQ_F), 56, 24, 56 },

	{ OP(0, 0, 0, OP_MUL_QV4_F), 24, 32, 68 },
	{ OP(0, 0, 0, OP_MUL_V4Q_F), 32, 24, 72 },
};

static pr_dvec4_t double_globals_init[] = {
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
	{0, 0, 0, 0},
	{0, 0, 0, 0},
};

static pr_dvec4_t double_globals_expect[] = {
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
	{36, 102, 120, 0},
	{52, 70, 136, 0},
	{-1, -2, -3, 4},

	{-1, -2, -3, 4},
	{36, 102, 120, 0},
	{52, 70, 136, 0},
};

static dstatement_t double_vector_statements[] = {
	{ OP(0, 0, 0, OP_DOT_CC_D), 0, 4, 8 },
	{ OP(0, 0, 0, OP_MUL_CC_D), 0, 4, 12 },
	{ OP(0, 0, 0, OP_DOT_VV_D), 16, 24, 32 },
	{ OP(0, 0, 0, OP_CROSS_VV_D), 16, 24, 40 },
	{ OP(0, 0, 0, OP_DOT_QQ_D), 48, 56, 72 },
	{ OP(0, 0, 0, OP_MUL_QQ_D), 48, 56, 80 },
	{ OP(0, 0, 0, OP_MUL_QV_D), 48, 64, 88 },
	{ OP(0, 0, 0, OP_MUL_VQ_D), 64, 48, 96 },

	{ OP(0, 0, 0, OP_MUL_QQ_D), 48, 64, 104 },
	{ OP(0, 0, 0, OP_SWIZZLE_D), 48, 0x07e4, 120 },
	{ OP(0, 0, 0, OP_MUL_QQ_D), 104, 120, 104 },

	{ OP(0, 0, 0, OP_SWIZZLE_D), 48, 0x07e4, 128 },
	{ OP(0, 0, 0, OP_MUL_QQ_D), 128, 64, 112 },
	{ OP(0, 0, 0, OP_MUL_QQ_D), 112, 48, 112 },

	{ OP(0, 0, 0, OP_MUL_QV4_D), 48, 64, 136 },
	{ OP(0, 0, 0, OP_MUL_V4Q_D), 64, 48, 144 },
};

test_t tests[] = {
	{
		.desc = "float vector",
		.num_globals = 4*num_globals(float_globals_init, float_globals_expect),
		.num_statements = num_statements (float_vector_statements),
		.statements = float_vector_statements,
		.init_globals = (pr_int_t *) float_globals_init,
		.expect_globals = (pr_int_t *) float_globals_expect,
	},
	{
		.desc = "double vector",
		.num_globals = 8*num_globals(double_globals_init,double_globals_expect),
		.num_statements = num_statements (double_vector_statements),
		.statements = double_vector_statements,
		.init_globals = (pr_int_t *) double_globals_init,
		.expect_globals = (pr_int_t *) double_globals_expect,
	},
};

#include "main.c"
