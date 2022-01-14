#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t bool32_conv_init[] = {
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

static pr_ivec4_t bool32_conv_expect[] = {
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
	{         -1,         -1,         -1,         -1},	// int
	{         -1,         -1,         -1,         -1},	// float
	{         -1,         -1,         -1,         -1},	// long
	{         -1,         -1,         -1,         -1},	// double
	{         -1,         -1,         -1,         -1},	// uint
	{          0,          0,          0,          0},	// bool32
	{         -1,         -1,         -1,         -1},	// ulong
	{         -1,         -1,         -1,          0},	// bool64
};

static dstatement_t bool32_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -1, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -2, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0005, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0015, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0025, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0035, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0045, 64 },
	{ OP(2, 1, 1, OP_CONV), 32, 0065, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0075, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t bool32_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -2, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -4, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0105, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0115, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0125, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0135, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0145, 64 },
	{ OP(2, 1, 1, OP_CONV), 32, 0165, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0175, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t bool32_conv_3a_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0205, 48 },
	{ OP(1, 1, 1, OP_CONV),  3, 0005, 51 },
	{ OP(1, 1, 1, OP_CONV),  4, 0215, 52 },
	{ OP(1, 1, 1, OP_CONV),  7, 0015, 55 },
	{ OP(2, 1, 1, OP_CONV),  8, 0225, 56 },
	{ OP(2, 1, 1, OP_CONV), 14, 0025, 59 },
	{ OP(2, 1, 1, OP_CONV), 16, 0235, 60 },
	{ OP(2, 1, 1, OP_CONV), 22, 0035, 63 },
	{ OP(1, 1, 1, OP_CONV), 24, 0245, 64 },
	{ OP(1, 1, 1, OP_CONV), 27, 0045, 67 },
	{ OP(2, 1, 1, OP_CONV), 32, 0265, 72 },
	{ OP(2, 1, 1, OP_CONV), 38, 0065, 75 },
	{ OP(2, 1, 1, OP_CONV), 40, 0275, 76 },
	{ OP(2, 1, 1, OP_CONV), 46, 0075, 79 },
};

static dstatement_t bool32_conv_3b_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0005, 48 },
	{ OP(1, 1, 1, OP_CONV),  1, 0205, 49 },
	{ OP(1, 1, 1, OP_CONV),  4, 0015, 52 },
	{ OP(1, 1, 1, OP_CONV),  5, 0215, 53 },
	{ OP(2, 1, 1, OP_CONV),  8, 0025, 56 },
	{ OP(2, 1, 1, OP_CONV), 10, 0225, 57 },
	{ OP(2, 1, 1, OP_CONV), 16, 0035, 60 },
	{ OP(2, 1, 1, OP_CONV), 18, 0235, 61 },
	{ OP(1, 1, 1, OP_CONV), 24, 0045, 64 },
	{ OP(1, 1, 1, OP_CONV), 25, 0245, 65 },
	{ OP(2, 1, 1, OP_CONV), 32, 0065, 72 },
	{ OP(2, 1, 1, OP_CONV), 34, 0265, 73 },
	{ OP(2, 1, 1, OP_CONV), 40, 0075, 76 },
	{ OP(2, 1, 1, OP_CONV), 42, 0275, 77 },
};

static dstatement_t bool32_conv_4_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0305, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0315, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0325, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0335, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0345, 64 },
	{ OP(2, 1, 1, OP_CONV), 32, 0365, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0375, 76 },
};

test_t tests[] = {
	{
		.desc = "bool32 conv 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(bool32_conv_init,bool32_conv_expect),
		.num_statements = num_statements (bool32_conv_1_statements),
		.statements = bool32_conv_1_statements,
		.init_globals = (pr_int_t *) bool32_conv_init,
		.expect_globals = (pr_int_t *) bool32_conv_expect,
	},
	{
		.desc = "bool32 conv 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(bool32_conv_init,bool32_conv_expect),
		.num_statements = num_statements (bool32_conv_2_statements),
		.statements = bool32_conv_2_statements,
		.init_globals = (pr_int_t *) bool32_conv_init,
		.expect_globals = (pr_int_t *) bool32_conv_expect,
	},
	{
		.desc = "bool32 conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(bool32_conv_init,bool32_conv_expect),
		.num_statements = num_statements (bool32_conv_3a_statements),
		.statements = bool32_conv_3a_statements,
		.init_globals = (pr_int_t *) bool32_conv_init,
		.expect_globals = (pr_int_t *) bool32_conv_expect,
	},
	{
		.desc = "bool32 conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(bool32_conv_init,bool32_conv_expect),
		.num_statements = num_statements (bool32_conv_3b_statements),
		.statements = bool32_conv_3b_statements,
		.init_globals = (pr_int_t *) bool32_conv_init,
		.expect_globals = (pr_int_t *) bool32_conv_expect,
	},
	{
		.desc = "bool32 conv 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(bool32_conv_init,bool32_conv_expect),
		.num_statements = num_statements (bool32_conv_4_statements),
		.statements = bool32_conv_4_statements,
		.init_globals = (pr_int_t *) bool32_conv_init,
		.expect_globals = (pr_int_t *) bool32_conv_expect,
	},
};

#include "main.c"
