#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t float_conv_init[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	{ 0x3fc00000, 0xbfc00000, 0x7149f2ca, 0xf149f2ca},	//float 1e30, -1e30
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	{ 0x39a08cea, 0x46293e59, 0x39a08cea, 0xc6293e59},	//double 1e30, -1e30
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
};

static pr_ivec4_t float_conv_expect[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	{ 0x3fc00000, 0xbfc00000, 0x7149f2ca, 0xf149f2ca},	//float
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	{ 0x39a08cea, 0x46293e59, 0x39a08cea, 0xc6293e59},	//double 1e30, -1e30
	{          0, 0x3ff80000,          0, 0xbff80000},	//double 1.5, -1.5
	{          5,         -5, 0x80000000, 0x7fffffff},	//uint
	{         ~0,          1, 0x80000000,          0},	//bool32
	{         99, 0x80000000, 0x80000000,         99},	//ulong
	{        256,          0, 0x7fffffff,          0},	//ulong
	{         ~0,         ~0,         ~0,          0},	//bool64
	{          0,         ~0,          0,          0},	//bool64
	{ 0x40a00000, 0xc0a00000, 0xcf000000, 0x4f000000},	// int
	{          0,          0,          0,          0},	// float
	{ 0xdf000000, 0x52c70000, 0x43800000, 0x4f000000},	// long
	{ 0x7149f2ca, 0xf149f2ca, 0x3fc00000, 0xbfc00000},	// double
	{ 0x40a00000, 0x4f800000, 0x4f000000, 0x4f000000},	// uint
	{ 0x3f800000, 0x3f800000, 0x3f800000,          0},	// bool32
	{ 0x5f000000, 0x52c70000, 0x43800000, 0x4f000000},	// ulong
	{ 0x3f800000, 0x3f800000, 0x3f800000,          0},	// bool64
};

static dstatement_t float_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -1, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -2, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0001, 48 },
	{ OP(2, 1, 1, OP_CONV),  8, 0021, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0031, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0041, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0051, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0061, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0071, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t float_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -2, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -4, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0101, 48 },
	{ OP(2, 1, 1, OP_CONV),  8, 0121, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0131, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0141, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0151, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0161, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0171, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t float_conv_3a_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0201, 48 },
	{ OP(1, 1, 1, OP_CONV),  3, 0001, 51 },
	{ OP(2, 1, 1, OP_CONV),  8, 0221, 56 },
	{ OP(2, 1, 1, OP_CONV), 14, 0021, 59 },
	{ OP(2, 1, 1, OP_CONV), 16, 0231, 60 },
	{ OP(2, 1, 1, OP_CONV), 22, 0031, 63 },
	{ OP(1, 1, 1, OP_CONV), 24, 0241, 64 },
	{ OP(1, 1, 1, OP_CONV), 27, 0041, 67 },
	{ OP(1, 1, 1, OP_CONV), 28, 0251, 68 },
	{ OP(1, 1, 1, OP_CONV), 31, 0051, 71 },
	{ OP(2, 1, 1, OP_CONV), 32, 0261, 72 },
	{ OP(2, 1, 1, OP_CONV), 38, 0061, 75 },
	{ OP(2, 1, 1, OP_CONV), 40, 0271, 76 },
	{ OP(2, 1, 1, OP_CONV), 46, 0071, 79 },
};

static dstatement_t float_conv_3b_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0001, 48 },
	{ OP(1, 1, 1, OP_CONV),  1, 0201, 49 },
	{ OP(2, 1, 1, OP_CONV),  8, 0021, 56 },
	{ OP(2, 1, 1, OP_CONV), 10, 0221, 57 },
	{ OP(2, 1, 1, OP_CONV), 16, 0031, 60 },
	{ OP(2, 1, 1, OP_CONV), 18, 0231, 61 },
	{ OP(1, 1, 1, OP_CONV), 24, 0241, 64 },
	{ OP(1, 1, 1, OP_CONV), 27, 0041, 67 },
	{ OP(1, 1, 1, OP_CONV), 28, 0051, 68 },
	{ OP(1, 1, 1, OP_CONV), 29, 0251, 69 },
	{ OP(2, 1, 1, OP_CONV), 32, 0061, 72 },
	{ OP(2, 1, 1, OP_CONV), 34, 0261, 73 },
	{ OP(2, 1, 1, OP_CONV), 40, 0071, 76 },
	{ OP(2, 1, 1, OP_CONV), 42, 0271, 77 },
};

static dstatement_t float_conv_4_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0301, 48 },
	{ OP(2, 1, 1, OP_CONV),  8, 0321, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0331, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0341, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0351, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0361, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0371, 76 },
};

test_t tests[] = {
	{
		.desc = "float conv 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(float_conv_init,float_conv_expect),
		.num_statements = num_statements (float_conv_1_statements),
		.statements = float_conv_1_statements,
		.init_globals = (pr_int_t *) float_conv_init,
		.expect_globals = (pr_int_t *) float_conv_expect,
	},
	{
		.desc = "float conv 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(float_conv_init,float_conv_expect),
		.num_statements = num_statements (float_conv_2_statements),
		.statements = float_conv_2_statements,
		.init_globals = (pr_int_t *) float_conv_init,
		.expect_globals = (pr_int_t *) float_conv_expect,
	},
	{
		.desc = "float conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(float_conv_init,float_conv_expect),
		.num_statements = num_statements (float_conv_3a_statements),
		.statements = float_conv_3a_statements,
		.init_globals = (pr_int_t *) float_conv_init,
		.expect_globals = (pr_int_t *) float_conv_expect,
	},
	{
		.desc = "float conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(float_conv_init,float_conv_expect),
		.num_statements = num_statements (float_conv_3b_statements),
		.statements = float_conv_3b_statements,
		.init_globals = (pr_int_t *) float_conv_init,
		.expect_globals = (pr_int_t *) float_conv_expect,
	},
	{
		.desc = "float conv 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(float_conv_init,float_conv_expect),
		.num_statements = num_statements (float_conv_4_statements),
		.statements = float_conv_4_statements,
		.init_globals = (pr_int_t *) float_conv_init,
		.expect_globals = (pr_int_t *) float_conv_expect,
	},
};

#include "main.c"
