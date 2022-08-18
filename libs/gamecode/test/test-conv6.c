#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t ulong_conv_init[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	//XXX{ 0x3fc00000, 0xbfc00000, 0x7149f2ca, 0xf149f2ca},	//float 1e30, -1e30
	{ 0x3fc00000, 0xbfc00000, 0, 0},	//float 1e30, -1e30
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	//XXX{ 0x39a08cea, 0x46293e59, 0x39a08cea, 0xc6293e59},	//double 1e30, -1e30
	{ 0, 0, 0, 0},	//double 1e30, -1e30
	{          0, 0x3ff80000,          0, 0xbff80000},	//double 1.5, -1.5
	{          5,         -5, 0x80000000, 0x7fffffff},	//uint
	{         ~0,          1, 0x80000000,          0},	//bool32
	{         99, 0x80000000, 0x80000000,         99},	//ulong
	{        256,          0, 0x7fffffff,          0},	//ulong
	{         ~0,         ~0,         ~0,          0},	//bool64
	{          0,         ~0,          0,          0},	//bool64
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

/* Note that these tests fail when compiled with clang due to difference
 * between clang and gcc. However, unlike test-conv4, the failure is
 * consistent between optimized and unoptimized: all tests fail either way.
 * Inspecting the results, it seems that clang sets negative floats and doubles
 * to 0 when casting to unsigned long.
 */
static pr_ivec4_t ulong_conv_expect[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	//XXX{ 0x3fc00000, 0xbfc00000, 0x7149f2ca, 0xf149f2ca},	//float 1e30, -1e30
	{ 0x3fc00000, 0xbfc00000, 0, 0},	//float 1e30, -1e30
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	//XXX{ 0x39a08cea, 0x46293e59, 0x39a08cea, 0xc6293e59},	//double 1e30, -1e30
	{ 0, 0, 0, 0},	//double 1e30, -1e30
	{          0, 0x3ff80000,          0, 0xbff80000},	//double 1.5, -1.5
	{          5,         -5, 0x80000000, 0x7fffffff},	//uint
	{         ~0,          1, 0x80000000,          0},	//bool32
	{         99, 0x80000000, 0x80000000,         99},	//ulong
	{        256,          0, 0x7fffffff,          0},	//ulong
	{         ~0,         ~0,         ~0,          0},	//bool64
	{          0,         ~0,          0,          0},	//bool64
	{          5,          0,         -5, 0xffffffff},	// int
	{ 0x80000000, 0xffffffff, 0x7fffffff,          0},
	{          1,          0,         -1,         -1},	// float
	//XXX{          0,          0,          0, 0x80000000},	// undef?
	{          0,          0,          0, 0},	// undef?
	{         99, 0x80000000, 0x80000000,         99},	// long
	{        256,          0, 0x7fffffff,          0},
	//XXX{          0,          0,          0, 0x80000000},	// double undef?
	{          0,          0,          0, 0},	// double undef?
	{          1,          0,         -1,         -1},
	{          5,          0,         -5,          0},	// uint
	{ 0x80000000,          0, 0x7fffffff,          0},
	{          1,          0,          1,          0},	// bool32
	{          1,          0,          0,          0},
	{         99, 0x80000000, 0x80000000,         99},	// ulong
	{        256,          0, 0x7fffffff,          0},
	{          1,          0,          1,          0},	// bool64
	{          1,          0,          0,          0},
};

static dstatement_t ulong_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -1, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -2, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0006,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0016,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0026,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0036,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0046,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0056,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0066,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0076, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t ulong_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -2, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -4, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0106,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0116,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0126,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0136,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0146,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0156,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0166,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0176, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t ulong_conv_3a_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0206,  48 },
	{ OP(1, 1, 2, OP_CONV),  3, 0006,  54 },
	{ OP(1, 1, 2, OP_CONV),  4, 0216,  56 },
	{ OP(1, 1, 2, OP_CONV),  7, 0016,  62 },
	{ OP(2, 1, 2, OP_CONV),  8, 0226,  64 },
	{ OP(2, 1, 2, OP_CONV), 14, 0026,  70 },
	{ OP(2, 1, 2, OP_CONV), 16, 0236,  72 },
	{ OP(2, 1, 2, OP_CONV), 22, 0036,  78 },
	{ OP(1, 1, 2, OP_CONV), 24, 0246,  80 },
	{ OP(1, 1, 2, OP_CONV), 27, 0046,  86 },
	{ OP(1, 1, 2, OP_CONV), 28, 0256,  88 },
	{ OP(1, 1, 2, OP_CONV), 31, 0056,  94 },
	{ OP(2, 1, 2, OP_CONV), 32, 0266,  96 },
	{ OP(2, 1, 2, OP_CONV), 38, 0066, 102 },
	{ OP(2, 1, 2, OP_CONV), 40, 0276, 104 },
	{ OP(2, 1, 2, OP_CONV), 46, 0076, 110 },
};

static dstatement_t ulong_conv_3b_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0006,  48 },
	{ OP(1, 1, 2, OP_CONV),  1, 0206,  50 },
	{ OP(1, 1, 2, OP_CONV),  4, 0016,  56 },
	{ OP(1, 1, 2, OP_CONV),  5, 0216,  58 },
	{ OP(2, 1, 2, OP_CONV),  8, 0026,  64 },
	{ OP(2, 1, 2, OP_CONV), 10, 0226,  66 },
	{ OP(2, 1, 2, OP_CONV), 16, 0036,  72 },
	{ OP(2, 1, 2, OP_CONV), 18, 0236,  74 },
	{ OP(1, 1, 2, OP_CONV), 24, 0046,  80 },
	{ OP(1, 1, 2, OP_CONV), 25, 0246,  82 },
	{ OP(1, 1, 2, OP_CONV), 28, 0056,  88 },
	{ OP(1, 1, 2, OP_CONV), 29, 0256,  90 },
	{ OP(2, 1, 2, OP_CONV), 32, 0066,  96 },
	{ OP(2, 1, 2, OP_CONV), 34, 0266,  98 },
	{ OP(2, 1, 2, OP_CONV), 40, 0076, 104 },
	{ OP(2, 1, 2, OP_CONV), 42, 0276, 106 },
};

static dstatement_t ulong_conv_4_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0306,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0316,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0326,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0336,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0346,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0356,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0366,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0376, 104 },
};

test_t tests[] = {
	{
		.desc = "ulong conv 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_conv_init,ulong_conv_expect),
		.num_statements = num_statements (ulong_conv_1_statements),
		.statements = ulong_conv_1_statements,
		.init_globals = (pr_int_t *) ulong_conv_init,
		.expect_globals = (pr_int_t *) ulong_conv_expect,
	},
	{
		.desc = "ulong conv 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_conv_init,ulong_conv_expect),
		.num_statements = num_statements (ulong_conv_2_statements),
		.statements = ulong_conv_2_statements,
		.init_globals = (pr_int_t *) ulong_conv_init,
		.expect_globals = (pr_int_t *) ulong_conv_expect,
	},
	{
		.desc = "ulong conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_conv_init,ulong_conv_expect),
		.num_statements = num_statements (ulong_conv_3a_statements),
		.statements = ulong_conv_3a_statements,
		.init_globals = (pr_int_t *) ulong_conv_init,
		.expect_globals = (pr_int_t *) ulong_conv_expect,
	},
	{
		.desc = "ulong conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_conv_init,ulong_conv_expect),
		.num_statements = num_statements (ulong_conv_3b_statements),
		.statements = ulong_conv_3b_statements,
		.init_globals = (pr_int_t *) ulong_conv_init,
		.expect_globals = (pr_int_t *) ulong_conv_expect,
	},
	{
		.desc = "ulong conv 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_conv_init,ulong_conv_expect),
		.num_statements = num_statements (ulong_conv_4_statements),
		.statements = ulong_conv_4_statements,
		.init_globals = (pr_int_t *) ulong_conv_init,
		.expect_globals = (pr_int_t *) ulong_conv_expect,
	},
};

#include "main.c"
