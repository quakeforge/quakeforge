#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t bool64_conv_init[] = {
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

static pr_ivec4_t bool64_conv_expect[] = {
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
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// int
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// float
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// long
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// double
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// uint
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// bool32
	{ 0xffffffff, 0xffffffff,          0,          0},
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},	// ulong
	{ 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
	{          0,          0,          0,          0},	// bool64
	{          0,          0,          0,          0},
};

static dstatement_t bool64_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -1, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -2, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0007,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0017,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0027,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0037,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0047,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0057,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0067,  96 },
	{ OP(0, 0, 0, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t bool64_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 112 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 113 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 112, -2, 112 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 113, -4, 113 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 112 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 112, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 113, 2 },
	{ OP(1, 1, 2, OP_CONV),  0, 0107,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0117,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0127,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0137,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0147,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0157,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0167,  96 },
	{ OP(0, 0, 0, OP_JUMP_A), -13, 0, 0 },
};

static dstatement_t bool64_conv_3a_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0207,  48 },
	{ OP(1, 1, 2, OP_CONV),  3, 0007,  54 },
	{ OP(1, 1, 2, OP_CONV),  4, 0217,  56 },
	{ OP(1, 1, 2, OP_CONV),  7, 0017,  62 },
	{ OP(2, 1, 2, OP_CONV),  8, 0227,  64 },
	{ OP(2, 1, 2, OP_CONV), 14, 0027,  70 },
	{ OP(2, 1, 2, OP_CONV), 16, 0237,  72 },
	{ OP(2, 1, 2, OP_CONV), 22, 0037,  78 },
	{ OP(1, 1, 2, OP_CONV), 24, 0247,  80 },
	{ OP(1, 1, 2, OP_CONV), 27, 0047,  86 },
	{ OP(1, 1, 2, OP_CONV), 28, 0257,  88 },
	{ OP(1, 1, 2, OP_CONV), 31, 0057,  94 },
	{ OP(2, 1, 2, OP_CONV), 32, 0267,  96 },
	{ OP(2, 1, 2, OP_CONV), 38, 0067, 102 },
};

static dstatement_t bool64_conv_3b_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0007,  48 },
	{ OP(1, 1, 2, OP_CONV),  1, 0207,  50 },
	{ OP(1, 1, 2, OP_CONV),  4, 0017,  56 },
	{ OP(1, 1, 2, OP_CONV),  5, 0217,  58 },
	{ OP(2, 1, 2, OP_CONV),  8, 0027,  64 },
	{ OP(2, 1, 2, OP_CONV), 10, 0227,  66 },
	{ OP(2, 1, 2, OP_CONV), 16, 0037,  72 },
	{ OP(2, 1, 2, OP_CONV), 18, 0237,  74 },
	{ OP(1, 1, 2, OP_CONV), 24, 0047,  80 },
	{ OP(1, 1, 2, OP_CONV), 25, 0247,  82 },
	{ OP(1, 1, 2, OP_CONV), 28, 0057,  88 },
	{ OP(1, 1, 2, OP_CONV), 29, 0257,  90 },
	{ OP(2, 1, 2, OP_CONV), 32, 0067,  96 },
	{ OP(2, 1, 2, OP_CONV), 34, 0267,  98 },
};

static dstatement_t bool64_conv_4_statements[] = {
	{ OP(1, 1, 2, OP_CONV),  0, 0307,  48 },
	{ OP(1, 1, 2, OP_CONV),  4, 0317,  56 },
	{ OP(2, 1, 2, OP_CONV),  8, 0327,  64 },
	{ OP(2, 1, 2, OP_CONV), 16, 0337,  72 },
	{ OP(1, 1, 2, OP_CONV), 24, 0347,  80 },
	{ OP(1, 1, 2, OP_CONV), 28, 0357,  88 },
	{ OP(2, 1, 2, OP_CONV), 32, 0367,  96 },
};

test_t tests[] = {
	{
		.desc = "bool64 conv 1",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(bool64_conv_init,bool64_conv_expect),
		.num_statements = num_statements (bool64_conv_1_statements),
		.statements = bool64_conv_1_statements,
		.init_globals = (pr_int_t *) bool64_conv_init,
		.expect_globals = (pr_int_t *) bool64_conv_expect,
	},
	{
		.desc = "bool64 conv 2",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(bool64_conv_init,bool64_conv_expect),
		.num_statements = num_statements (bool64_conv_2_statements),
		.statements = bool64_conv_2_statements,
		.init_globals = (pr_int_t *) bool64_conv_init,
		.expect_globals = (pr_int_t *) bool64_conv_expect,
	},
	{
		.desc = "bool64 conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(bool64_conv_init,bool64_conv_expect),
		.num_statements = num_statements (bool64_conv_3a_statements),
		.statements = bool64_conv_3a_statements,
		.init_globals = (pr_int_t *) bool64_conv_init,
		.expect_globals = (pr_int_t *) bool64_conv_expect,
	},
	{
		.desc = "bool64 conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(bool64_conv_init,bool64_conv_expect),
		.num_statements = num_statements (bool64_conv_3b_statements),
		.statements = bool64_conv_3b_statements,
		.init_globals = (pr_int_t *) bool64_conv_init,
		.expect_globals = (pr_int_t *) bool64_conv_expect,
	},
	{
		.desc = "bool64 conv 4",
		.extra_globals = 4 * 1,
		.num_globals = 4*num_globals(bool64_conv_init,bool64_conv_expect),
		.num_statements = num_statements (bool64_conv_4_statements),
		.statements = bool64_conv_4_statements,
		.init_globals = (pr_int_t *) bool64_conv_init,
		.expect_globals = (pr_int_t *) bool64_conv_expect,
	},
};

#include "main.c"
