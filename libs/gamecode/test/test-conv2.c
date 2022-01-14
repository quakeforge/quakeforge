#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t long_conv_init[] = {
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

static pr_ivec4_t long_conv_expect[] = {
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
	{          5,          0,         -5, 0xffffffff},	// int
	{ 0x80000000, 0xffffffff, 0x7fffffff,          0},
	{          1,          0,         -1,         -1},	// float
	{          0, 0x80000000,          0, 0x80000000},	// undef?
	{          0,          0,          0,          0},	// long
	{          0,          0,          0,          0},
	{          0, 0x80000000,          0, 0x80000000},	// double undef?
	{          1,          0,         -1,         -1},
	{          5,          0,         -5,          0},	// uint
	{ 0x80000000,          0, 0x7fffffff,          0},
	{          1,          0,          1,          0},	// bool32
	{          1,          0,          0,          0},
	{          0,          0,          0,          0},	// ulong
	{          0,          0,          0,          0},
	{          1,          0,          1,          0},	// bool64
	{          1,          0,          0,          0},
};

static dstatement_t long_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -1, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -2, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0002,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0012,  56 },
	{ OP(2, 1, 2, OP_CONV), 16, 0032,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0042,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0052,  88 },
	{ OP(2, 1, 2, OP_CONV), 40, 0072, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -12, 0, 0 },
};

static dstatement_t long_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -2, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -4, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0102,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0112,  56 },
	{ OP(2, 1, 2, OP_CONV), 16, 0132,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0142,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0152,  88 },
	{ OP(2, 1, 2, OP_CONV), 40, 0172, 104 },
	{ OP(0, 0, 0, OP_JUMP_A), -12, 0, 0 },
};

static dstatement_t long_conv_3a_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0202,  48 },
	{ OP(1, 1, 2, OP_CONV),  3, 0002,  54 },
	{ OP(1, 1, 2, OP_CONV),  4, 0212,  56 },
	{ OP(1, 1, 2, OP_CONV),  7, 0012,  62 },
	{ OP(2, 1, 2, OP_CONV), 16, 0232,  72 },
	{ OP(2, 1, 2, OP_CONV), 22, 0032,  78 },
	{ OP(1, 1, 2, OP_CONV), 24, 0242,  80 },
	{ OP(1, 1, 2, OP_CONV), 27, 0042,  86 },
	{ OP(1, 1, 2, OP_CONV), 28, 0252,  88 },
	{ OP(1, 1, 2, OP_CONV), 31, 0052,  94 },
	{ OP(2, 1, 2, OP_CONV), 40, 0272, 104 },
	{ OP(2, 1, 2, OP_CONV), 46, 0072, 110 },
};

static dstatement_t long_conv_3b_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0002,  48 },
	{ OP(1, 1, 2, OP_CONV),  1, 0202,  50 },
	{ OP(1, 1, 2, OP_CONV),  4, 0012,  56 },
	{ OP(1, 1, 2, OP_CONV),  5, 0212,  58 },
	{ OP(2, 1, 2, OP_CONV), 16, 0032,  72 },
	{ OP(2, 1, 2, OP_CONV), 18, 0232,  74 },
	{ OP(1, 1, 2, OP_CONV), 24, 0042,  80 },
	{ OP(1, 1, 2, OP_CONV), 25, 0242,  82 },
	{ OP(1, 1, 2, OP_CONV), 28, 0052,  88 },
	{ OP(1, 1, 2, OP_CONV), 29, 0252,  90 },
	{ OP(2, 1, 2, OP_CONV), 40, 0072, 104 },
	{ OP(2, 1, 2, OP_CONV), 42, 0272, 106 },
};

static dstatement_t long_conv_4_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0302,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0312,  56 },
	{ OP(2, 1, 2, OP_CONV), 16, 0332,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0342,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0352,  88 },
	{ OP(2, 1, 2, OP_CONV), 40, 0372, 104 },
};

test_t tests[] = {
	{
		.desc = "long conv 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(long_conv_init,long_conv_expect),
		.num_statements = num_statements (long_conv_1_statements),
		.statements = long_conv_1_statements,
		.init_globals = (pr_int_t *) long_conv_init,
		.expect_globals = (pr_int_t *) long_conv_expect,
	},
	{
		.desc = "long conv 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(long_conv_init,long_conv_expect),
		.num_statements = num_statements (long_conv_2_statements),
		.statements = long_conv_2_statements,
		.init_globals = (pr_int_t *) long_conv_init,
		.expect_globals = (pr_int_t *) long_conv_expect,
	},
	{
		.desc = "long conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(long_conv_init,long_conv_expect),
		.num_statements = num_statements (long_conv_3a_statements),
		.statements = long_conv_3a_statements,
		.init_globals = (pr_int_t *) long_conv_init,
		.expect_globals = (pr_int_t *) long_conv_expect,
	},
	{
		.desc = "long conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(long_conv_init,long_conv_expect),
		.num_statements = num_statements (long_conv_3b_statements),
		.statements = long_conv_3b_statements,
		.init_globals = (pr_int_t *) long_conv_init,
		.expect_globals = (pr_int_t *) long_conv_expect,
	},
	{
		.desc = "long conv 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(long_conv_init,long_conv_expect),
		.num_statements = num_statements (long_conv_4_statements),
		.statements = long_conv_4_statements,
		.init_globals = (pr_int_t *) long_conv_init,
		.expect_globals = (pr_int_t *) long_conv_expect,
	},
};

#include "main.c"
