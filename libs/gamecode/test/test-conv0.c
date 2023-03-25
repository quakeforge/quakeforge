#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t int_conv_init[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	{ 0x3fc00000, 0xbfc00000, 0x40100000, 0xc0100000},	//float 1.5, -1.5, 2.25, -2.25
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	{          0, 0x3ff80000,          0, 0xbff80000},	//double 1.5, -1.5
	{          0, 0x40020000,          0, 0xc0020000},	//double 2.25, -2.25
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

static pr_ivec4_t int_conv_expect[] = {
	{          5,         -5, 0x80000000, 0x7fffffff},	//int
	{ 0x3fc00000, 0xbfc00000, 0x40100000, 0xc0100000},	//float 1.5, -1.5, 2.25, -2.25
	{         99, 0x80000000, 0x80000000,         99},	//long
	{        256,          0, 0x7fffffff,          0},	//long
	{          0, 0x3ff80000,          0, 0xbff80000},	//double 1.5, -1.5
	{          0, 0x40020000,          0, 0xc0020000},	//double 2.25, -2.25
	{          5,         -5, 0x80000000, 0x7fffffff},	//uint
	{         ~0,          1, 0x80000000,          0},	//bool32
	{         99, 0x80000000, 0x80000000,         99},	//ulong
	{        256,          0, 0x7fffffff,          0},	//ulong
	{         ~0,         ~0,         ~0,          0},	//bool64
	{          0,         ~0,          0,          0},	//bool64
	{          5,         -5, 0x80000000, 0x7fffffff},	// int
	{          1,         -1,          2,         -2},	// float
	{         99, 0x80000000,        256, 0x7fffffff},	// long
	{          1,         -1,          2,         -2},	// double
	{          5,         -5, 0x80000000, 0x7fffffff},	// uint
	{          1,          1,          1,          0},	// bool32
	{         99, 0x80000000,        256, 0x7fffffff},	// ulong
	{          1,          1,          1,          0},	// bool64
};

static dstatement_t int_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -1, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -2, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0000, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0010, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0020, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0030, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0040, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0050, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0060, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0070, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t int_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -2, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -4, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0100, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0110, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0120, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0130, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0140, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0150, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0160, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0170, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t int_conv_3a_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0200, 48 },
	{ OP(1, 1, 1, OP_CONV),  3, 0200, 51 },
	{ OP(1, 1, 1, OP_CONV),  4, 0210, 52 },
	{ OP(1, 1, 1, OP_CONV),  7, 0010, 55 },
	{ OP(2, 1, 1, OP_CONV),  8, 0220, 56 },
	{ OP(2, 1, 1, OP_CONV), 14, 0020, 59 },
	{ OP(2, 1, 1, OP_CONV), 16, 0230, 60 },
	{ OP(2, 1, 1, OP_CONV), 22, 0030, 63 },
	{ OP(1, 1, 1, OP_CONV), 24, 0240, 64 },
	{ OP(1, 1, 1, OP_CONV), 27, 0240, 67 },
	{ OP(1, 1, 1, OP_CONV), 28, 0250, 68 },
	{ OP(1, 1, 1, OP_CONV), 31, 0050, 71 },
	{ OP(2, 1, 1, OP_CONV), 32, 0260, 72 },
	{ OP(2, 1, 1, OP_CONV), 38, 0060, 75 },
	{ OP(2, 1, 1, OP_CONV), 40, 0270, 76 },
	{ OP(2, 1, 1, OP_CONV), 46, 0070, 79 },
};

static dstatement_t int_conv_3b_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0200, 48 },
	{ OP(1, 1, 1, OP_CONV),  1, 0200, 49 },
	{ OP(1, 1, 1, OP_CONV),  4, 0010, 52 },
	{ OP(1, 1, 1, OP_CONV),  5, 0210, 53 },
	{ OP(2, 1, 1, OP_CONV),  8, 0020, 56 },
	{ OP(2, 1, 1, OP_CONV), 10, 0220, 57 },
	{ OP(2, 1, 1, OP_CONV), 16, 0030, 60 },
	{ OP(2, 1, 1, OP_CONV), 18, 0230, 61 },
	{ OP(1, 1, 1, OP_CONV), 24, 0240, 64 },
	{ OP(1, 1, 1, OP_CONV), 25, 0240, 65 },
	{ OP(1, 1, 1, OP_CONV), 28, 0050, 68 },
	{ OP(1, 1, 1, OP_CONV), 29, 0250, 69 },
	{ OP(2, 1, 1, OP_CONV), 32, 0060, 72 },
	{ OP(2, 1, 1, OP_CONV), 34, 0260, 73 },
	{ OP(2, 1, 1, OP_CONV), 40, 0070, 76 },
	{ OP(2, 1, 1, OP_CONV), 42, 0270, 77 },
};

static dstatement_t int_conv_4_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0300, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0310, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0320, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0330, 60 },
	{ OP(1, 1, 1, OP_CONV), 28, 0350, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0360, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0370, 76 },
	{ OP(1, 1, 1, OP_CONV), 24, 0340, 64 },
};

test_t tests[] = {
	{
		.desc = "int conv 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_conv_init,int_conv_expect),
		.num_statements = num_statements (int_conv_1_statements),
		.statements = int_conv_1_statements,
		.init_globals = (pr_int_t *) int_conv_init,
		.expect_globals = (pr_int_t *) int_conv_expect,
	},
	{
		.desc = "int conv 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_conv_init,int_conv_expect),
		.num_statements = num_statements (int_conv_2_statements),
		.statements = int_conv_2_statements,
		.init_globals = (pr_int_t *) int_conv_init,
		.expect_globals = (pr_int_t *) int_conv_expect,
	},
	{
		.desc = "int conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_conv_init,int_conv_expect),
		.num_statements = num_statements (int_conv_3a_statements),
		.statements = int_conv_3a_statements,
		.init_globals = (pr_int_t *) int_conv_init,
		.expect_globals = (pr_int_t *) int_conv_expect,
	},
	{
		.desc = "int conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_conv_init,int_conv_expect),
		.num_statements = num_statements (int_conv_3b_statements),
		.statements = int_conv_3b_statements,
		.init_globals = (pr_int_t *) int_conv_init,
		.expect_globals = (pr_int_t *) int_conv_expect,
	},
	{
		.desc = "int conv 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(int_conv_init,int_conv_expect),
		.num_statements = num_statements (int_conv_4_statements),
		.statements = int_conv_4_statements,
		.init_globals = (pr_int_t *) int_conv_init,
		.expect_globals = (pr_int_t *) int_conv_expect,
	},
};

#include "main.c"
