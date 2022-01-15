#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t double_conv_init[] = {
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
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ivec4_t double_conv_expect[] = {
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
	{ 0x00000000, 0x40140000, 0x00000000, 0xc0140000},	// int
	{ 0x00000000, 0xc1e00000, 0xffc00000, 0x41dfffff},
	{ 0x00000000, 0x3ff80000, 0x00000000, 0xbff80000},	// float
	{ 0x40000000, 0x46293e59, 0x40000000, 0xc6293e59},
	{ 0x00000000, 0xc3e00000, 0x00000000, 0x4258e000},	// long
	{ 0x00000000, 0x40700000, 0xffc00000, 0x41dfffff},
	{ 0x39a08cea, 0x46293e59, 0x39a08cea, 0xc6293e59},	// double
	{          0, 0x3ff80000,          0, 0xbff80000},
	{ 0x00000000, 0x40140000, 0xff600000, 0x41efffff},	// uint
	{ 0x00000000, 0x41e00000, 0xffc00000, 0x41dfffff},
	{ 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000},	// bool32
	{ 0x00000000, 0x3ff00000, 0x00000000, 0x00000000},
	{ 0x00000000, 0x43e00000, 0x00000000, 0x4258e000},	// long
	{ 0x00000000, 0x40700000, 0xffc00000, 0x41dfffff},
	{ 0x00000000, 0x3ff00000, 0x00000000, 0x3ff00000},	// bool64
	{ 0x00000000, 0x3ff00000, 0x00000000, 0x00000000},
};

static dstatement_t double_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -1, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -2, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0003,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0013,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0023,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0033,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0043,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0053,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0063,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0073, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t double_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -2, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -4, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0103,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0113,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0123,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0133,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0143,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0153,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0163,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0173, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t double_conv_3a_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0203,  48 },
	{ OP(1, 1, 2, OP_CONV),  3, 0003,  54 },
	{ OP(1, 1, 2, OP_CONV),  4, 0213,  56 },
	{ OP(1, 1, 2, OP_CONV),  7, 0013,  62 },
	{ OP(2, 1, 2, OP_CONV),  8, 0223,  64 },
	{ OP(2, 1, 2, OP_CONV), 14, 0023,  70 },
	{ OP(2, 1, 2, OP_CONV), 16, 0233,  72 },
	{ OP(2, 1, 2, OP_CONV), 22, 0033,  78 },
	{ OP(1, 1, 2, OP_CONV), 24, 0243,  80 },
	{ OP(1, 1, 2, OP_CONV), 27, 0043,  86 },
	{ OP(1, 1, 2, OP_CONV), 28, 0253,  88 },
	{ OP(1, 1, 2, OP_CONV), 31, 0053,  94 },
	{ OP(2, 1, 2, OP_CONV), 32, 0263,  96 },
	{ OP(2, 1, 2, OP_CONV), 38, 0063, 102 },
	{ OP(2, 1, 2, OP_CONV), 40, 0273, 104 },
	{ OP(2, 1, 2, OP_CONV), 46, 0073, 110 },
};

static dstatement_t double_conv_3b_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0003,  48 },
	{ OP(1, 1, 2, OP_CONV),  1, 0203,  50 },
	{ OP(1, 1, 2, OP_CONV),  4, 0013,  56 },
	{ OP(1, 1, 2, OP_CONV),  5, 0213,  58 },
	{ OP(2, 1, 2, OP_CONV),  8, 0023,  64 },
	{ OP(2, 1, 2, OP_CONV), 10, 0223,  66 },
	{ OP(2, 1, 2, OP_CONV), 16, 0033,  72 },
	{ OP(2, 1, 2, OP_CONV), 18, 0233,  74 },
	{ OP(1, 1, 2, OP_CONV), 24, 0043,  80 },
	{ OP(1, 1, 2, OP_CONV), 25, 0243,  82 },
	{ OP(1, 1, 2, OP_CONV), 28, 0053,  88 },
	{ OP(1, 1, 2, OP_CONV), 29, 0253,  90 },
	{ OP(2, 1, 2, OP_CONV), 32, 0063,  96 },
	{ OP(2, 1, 2, OP_CONV), 34, 0263,  98 },
	{ OP(2, 1, 2, OP_CONV), 40, 0073, 104 },
	{ OP(2, 1, 2, OP_CONV), 42, 0273, 106 },
};

static dstatement_t double_conv_4_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0303,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0313,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0323,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0333,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0343,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0353,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0363,  96 },
	{ OP(2, 1, 2, OP_CONV), 40, 0373, 104 },
};

test_t tests[] = {
	{
		.desc = "double conv 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(double_conv_init,double_conv_expect),
		.num_statements = num_statements (double_conv_1_statements),
		.statements = double_conv_1_statements,
		.init_globals = (pr_int_t *) double_conv_init,
		.expect_globals = (pr_int_t *) double_conv_expect,
	},
	{
		.desc = "double conv 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(double_conv_init,double_conv_expect),
		.num_statements = num_statements (double_conv_2_statements),
		.statements = double_conv_2_statements,
		.init_globals = (pr_int_t *) double_conv_init,
		.expect_globals = (pr_int_t *) double_conv_expect,
	},
	{
		.desc = "double conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(double_conv_init,double_conv_expect),
		.num_statements = num_statements (double_conv_3a_statements),
		.statements = double_conv_3a_statements,
		.init_globals = (pr_int_t *) double_conv_init,
		.expect_globals = (pr_int_t *) double_conv_expect,
	},
	{
		.desc = "double conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(double_conv_init,double_conv_expect),
		.num_statements = num_statements (double_conv_3b_statements),
		.statements = double_conv_3b_statements,
		.init_globals = (pr_int_t *) double_conv_init,
		.expect_globals = (pr_int_t *) double_conv_expect,
	},
	{
		.desc = "double conv 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(double_conv_init,double_conv_expect),
		.num_statements = num_statements (double_conv_4_statements),
		.statements = double_conv_4_statements,
		.init_globals = (pr_int_t *) double_conv_init,
		.expect_globals = (pr_int_t *) double_conv_expect,
	},
};

#include "main.c"
