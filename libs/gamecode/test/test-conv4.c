#include "head.c"

#include "QF/mathlib.h"

static pr_ivec4_t uint_conv_init[] = {
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

static pr_ivec4_t uint_conv_expect[] = {
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
	{          5,         -5, 0x80000000, 0x7fffffff},	// int
	{          1, 0xffffffff,          0,          0},	// float undef?
	{         99, 0x80000000,        256, 0x7fffffff},	// long
	{          0,          0,          1, 0xffffffff},	// double undef?
	{          5,         -5, 0x80000000, 0x7fffffff},	// uint
	{          1,          1,          1,          0},	// bool32
	{         99, 0x80000000,        256, 0x7fffffff},	// ulong
	{          1,          1,          1,          0},	// bool64
};

static dstatement_t uint_conv_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// init index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -1, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -2, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0004, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0014, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0024, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0034, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0044, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0054, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0064, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0074, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t uint_conv_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 80 },	// index
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 81 },	// init index for 64-bits
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 80, -2, 80 },	// dec index
	{ OP(0, 0, 0, OP_LEA_C), 81, -4, 81 },	// dec index for 64-bits
	{ OP(0, 0, 0, OP_IFAE_A), 2, 0, 80 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 80, 1 },
	{ OP(0, 0, 0, OP_WITH), 4, 81, 2 },
	{ OP(1, 1, 1, OP_CONV),  0, 0104, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0114, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0124, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0134, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0144, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0154, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0164, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0174, 76 },
	{ OP(1, 1, 1, OP_JUMP_A), -14, 0, 0 },
};

static dstatement_t uint_conv_3a_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0204, 48 },
	{ OP(1, 1, 1, OP_CONV),  3, 0004, 51 },
	{ OP(1, 1, 1, OP_CONV),  4, 0214, 52 },
	{ OP(1, 1, 1, OP_CONV),  7, 0014, 55 },
	{ OP(2, 1, 1, OP_CONV),  8, 0224, 56 },
	{ OP(2, 1, 1, OP_CONV), 14, 0024, 59 },
	{ OP(2, 1, 1, OP_CONV), 16, 0234, 60 },
	{ OP(2, 1, 1, OP_CONV), 22, 0034, 63 },
	{ OP(1, 1, 1, OP_CONV), 24, 0244, 64 },
	{ OP(1, 1, 1, OP_CONV), 27, 0044, 67 },
	{ OP(1, 1, 1, OP_CONV), 28, 0254, 68 },
	{ OP(1, 1, 1, OP_CONV), 31, 0054, 71 },
	{ OP(2, 1, 1, OP_CONV), 32, 0264, 72 },
	{ OP(2, 1, 1, OP_CONV), 38, 0064, 75 },
	{ OP(2, 1, 1, OP_CONV), 40, 0274, 76 },
	{ OP(2, 1, 1, OP_CONV), 46, 0074, 79 },
};

static dstatement_t uint_conv_3b_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0004, 48 },
	{ OP(1, 1, 1, OP_CONV),  1, 0204, 49 },
	{ OP(1, 1, 1, OP_CONV),  4, 0014, 52 },
	{ OP(1, 1, 1, OP_CONV),  5, 0214, 53 },
	{ OP(2, 1, 1, OP_CONV),  8, 0024, 56 },
	{ OP(2, 1, 1, OP_CONV), 10, 0224, 57 },
	{ OP(2, 1, 1, OP_CONV), 16, 0034, 60 },
	{ OP(2, 1, 1, OP_CONV), 18, 0234, 61 },
	{ OP(1, 1, 1, OP_CONV), 24, 0044, 64 },
	{ OP(1, 1, 1, OP_CONV), 25, 0244, 65 },
	{ OP(1, 1, 1, OP_CONV), 28, 0054, 68 },
	{ OP(1, 1, 1, OP_CONV), 29, 0254, 69 },
	{ OP(2, 1, 1, OP_CONV), 32, 0064, 72 },
	{ OP(2, 1, 1, OP_CONV), 34, 0264, 73 },
	{ OP(2, 1, 1, OP_CONV), 40, 0074, 76 },
	{ OP(2, 1, 1, OP_CONV), 42, 0274, 77 },
};

static dstatement_t uint_conv_4_statements[] = {
	{ OP(1, 1, 1, OP_CONV),  0, 0304, 48 },
	{ OP(1, 1, 1, OP_CONV),  4, 0314, 52 },
	{ OP(2, 1, 1, OP_CONV),  8, 0324, 56 },
	{ OP(2, 1, 1, OP_CONV), 16, 0334, 60 },
	{ OP(1, 1, 1, OP_CONV), 24, 0344, 64 },
	{ OP(1, 1, 1, OP_CONV), 28, 0354, 68 },
	{ OP(2, 1, 1, OP_CONV), 32, 0364, 72 },
	{ OP(2, 1, 1, OP_CONV), 40, 0374, 76 },
};

test_t tests[] = {
	{
		.desc = "uint conv 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_conv_init,uint_conv_expect),
		.num_statements = num_statements (uint_conv_1_statements),
		.statements = uint_conv_1_statements,
		.init_globals = (pr_int_t *) uint_conv_init,
		.expect_globals = (pr_int_t *) uint_conv_expect,
	},
	{
		.desc = "uint conv 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_conv_init,uint_conv_expect),
		.num_statements = num_statements (uint_conv_2_statements),
		.statements = uint_conv_2_statements,
		.init_globals = (pr_int_t *) uint_conv_init,
		.expect_globals = (pr_int_t *) uint_conv_expect,
	},
	{
		.desc = "uint conv 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_conv_init,uint_conv_expect),
		.num_statements = num_statements (uint_conv_3a_statements),
		.statements = uint_conv_3a_statements,
		.init_globals = (pr_int_t *) uint_conv_init,
		.expect_globals = (pr_int_t *) uint_conv_expect,
	},
	{
		.desc = "uint conv 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_conv_init,uint_conv_expect),
		.num_statements = num_statements (uint_conv_3b_statements),
		.statements = uint_conv_3b_statements,
		.init_globals = (pr_int_t *) uint_conv_init,
		.expect_globals = (pr_int_t *) uint_conv_expect,
	},
	{
		.desc = "uint conv 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_conv_init,uint_conv_expect),
		.num_statements = num_statements (uint_conv_4_statements),
		.statements = uint_conv_4_statements,
		.init_globals = (pr_int_t *) uint_conv_init,
		.expect_globals = (pr_int_t *) uint_conv_expect,
	},
};

#include "main.c"
