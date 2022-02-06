#include "head.c"

#include "QF/mathlib.h"

static pr_uivec4_t uint_divop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_uivec4_t uint_divop_expect[] = {
	{  5,         -5,   5, -5},
	{  3,          3,  -3, -3},
	{  1, 0x55555553,   0,  0},
	{  2,          2,   5, -5},
};

static dstatement_t uint_divop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_DIV_u_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_REM_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_JUMP_A), -6, 0,  0 },
};

static dstatement_t uint_divop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	{ OP(1, 1, 1, OP_DIV_u_2), 0, 4,  8 },
	{ OP(1, 1, 1, OP_REM_u_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_JUMP_A), -6, 0, 0 },
};

static dstatement_t uint_divop_3a_statements[] = {
	{ OP(1, 1, 1, OP_DIV_u_3), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_u_1), 3, 7, 11 },
	{ OP(1, 1, 1, OP_REM_u_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_u_1), 3, 7, 15 },
};

static dstatement_t uint_divop_3b_statements[] = {
	{ OP(1, 1, 1, OP_DIV_u_1), 0, 4,  8 },
	{ OP(1, 1, 1, OP_DIV_u_3), 1, 5,  9 },
	{ OP(1, 1, 1, OP_REM_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_REM_u_3), 1, 5, 13 },
};

static dstatement_t uint_divop_4_statements[] = {
	{ OP(1, 1, 1, OP_DIV_u_4), 0, 4,  8 },
	{ OP(1, 1, 1, OP_REM_u_4), 0, 4, 12 },
};

static pr_uivec4_t uint_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_uivec4_t uint_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0,  0,  0},	// no unsigned EQ (redundant)
	{  0,  0, -1,  0},
	{  0, -1,  0,  0},
	{  0,  0,  0,  0},	// no unsigned NE (redundant)
	{ -1, -1,  0, -1},
	{ -1,  0, -1, -1},
};

static dstatement_t uint_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -1, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_1), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t uint_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 32 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 32, -2, 32 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 32 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 32, 1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_2), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_2), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_JUMP_A), -8, 0, 0 },
};

static dstatement_t uint_cmpop_3a_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_3), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_u_1), 3, 7, 15 },
	{ OP(1, 1, 1, OP_GT_u_3), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_u_1), 3, 7, 19 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_3), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_u_1), 3, 7, 27 },
	{ OP(1, 1, 1, OP_LE_u_3), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_u_1), 3, 7, 31 },
};

static dstatement_t uint_cmpop_3b_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_LT_u_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_GT_u_1), 0, 4, 16 },
	{ OP(1, 1, 1, OP_GT_u_3), 1, 5, 17 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_1), 0, 4, 24 },
	{ OP(1, 1, 1, OP_GE_u_3), 1, 5, 25 },
	{ OP(1, 1, 1, OP_LE_u_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_LE_u_3), 1, 5, 29 },
};

static dstatement_t uint_cmpop_4_statements[] = {
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_u_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_GT_u_4), 0, 4, 16 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_u_4), 0, 4, 24 },
	{ OP(1, 1, 1, OP_LE_u_4), 0, 4, 28 },
};

static pr_ulvec4_t ulong_divop_init[] = {
	{  5, -5,  5, -5},
	{  3,  3, -3, -3},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ulvec4_t ulong_divop_expect[] = {
	{  5,                           -5,   5, -5},
	{  3,                            3,  -3, -3},
	{  1, UINT64_C(0x5555555555555553),   0,  0},
	{  2,                            2,   5, -5},
};

static dstatement_t ulong_divop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 64 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 64, 1 },
	{ OP(1, 1, 1, OP_DIV_U_1), 0, 8, 16 },
	{ OP(1, 1, 1, OP_REM_U_1), 0, 8, 24 },
	{ OP(1, 1, 1, OP_JUMP_A), -6, 0,  0 },
};

static dstatement_t ulong_divop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 64 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 64, 1 },
	{ OP(1, 1, 1, OP_DIV_U_2), 0, 8, 16 },
	{ OP(1, 1, 1, OP_REM_U_2), 0, 8, 24 },
	{ OP(1, 1, 1, OP_JUMP_A), -6, 0, 0 },
};

static dstatement_t ulong_divop_3a_statements[] = {
	{ OP(1, 1, 1, OP_DIV_U_3), 0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_U_1), 6, 14, 22 },
	{ OP(1, 1, 1, OP_REM_U_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_U_1), 6, 14, 30 },
};

static dstatement_t ulong_divop_3b_statements[] = {
	{ OP(1, 1, 1, OP_DIV_U_1), 0,  8, 16 },
	{ OP(1, 1, 1, OP_DIV_U_3), 2, 10, 18 },
	{ OP(1, 1, 1, OP_REM_U_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_REM_U_3), 2, 10, 26 },
};

static dstatement_t ulong_divop_4_statements[] = {
	{ OP(1, 1, 1, OP_DIV_U_4), 0, 8, 16 },
	{ OP(1, 1, 1, OP_REM_U_4), 0, 8, 24 },
};

static pr_ulvec4_t ulong_cmpop_init[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
	{  0,  0, 0, 0},
};

static pr_ulvec4_t ulong_cmpop_expect[] = {
	{  5, -5,  5, -5},
	{  5,  5, -5, -5},
	{  0,  0,  0,  0},	// no unsigned EQ (redundant)
	{  0,  0, -1,  0},
	{  0, -1,  0,  0},
	{  0,  0,  0,  0},	// no unsigned NE (redundant)
	{ -1, -1,  0, -1},
	{ -1,  0, -1, -1},
};

static dstatement_t ulong_cmpop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -2, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_1),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_U_1),  0,  8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_1),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_U_1),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t ulong_cmpop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),    8,  0, 64 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C),   64, -4, 64 },	// dec index
	{ OP(0, 0, 0, OP_IFAE),   2,  0, 64 },
	{ OP(0, 0, 0, OP_BREAK),    0,  0,  0 },
	{ OP(0, 0, 0, OP_WITH),     4, 64,  1 },
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_2),  0,  8, 24 },
	{ OP(1, 1, 1, OP_GT_U_2),  0,  8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_2),  0,  8, 48 },
	{ OP(1, 1, 1, OP_LE_U_2),  0,  8, 56 },
	{ OP(1, 1, 1, OP_JUMP_A), -8,  0,  0 },
};

static dstatement_t ulong_cmpop_3a_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_U_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_GT_U_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_U_1), 6, 14, 38 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_3), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_U_1), 6, 14, 54 },
	{ OP(1, 1, 1, OP_LE_U_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_U_1), 6, 14, 62 },
};

static dstatement_t ulong_cmpop_3b_statements[] = {
	// no unsigned EQ (redundant)
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_LT_U_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_GT_U_1), 0,  8, 32 },
	{ OP(1, 1, 1, OP_GT_U_3), 2, 10, 34 },
	// no unsigned NE (redundant)
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_1), 0,  8, 48 },
	{ OP(1, 1, 1, OP_GE_U_3), 2, 10, 50 },
	{ OP(1, 1, 1, OP_LE_U_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_LE_U_3), 2, 10, 58 },
};

static dstatement_t ulong_cmpop_4_statements[] = {
	// no unsigned EQ (redundant)
	{ OP(1, 1, 1, OP_LT_U_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_GT_U_4), 0, 8, 32 },
	// no unsigned NE (redundant)
	{ OP(1, 1, 1, OP_GE_U_4), 0, 8, 48 },
	{ OP(1, 1, 1, OP_LE_U_4), 0, 8, 56 },
};

static pr_uivec4_t uint_shiftop_init[] = {
	{ 0x12345678, 0x9abcdef0, 0x80000001, 0xaaaa5555 },
	{         12,         16,          9,          1 },
	{         20,         16,         23,         31 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
};

static pr_uivec4_t uint_shiftop_expect[] = {
	{ 0x12345678, 0x9abcdef0, 0x80000001, 0xaaaa5555 },
	{         12,         16,          9,          1 },//a
	{         20,         16,         23,         31 },//b
	{ 0x45678000, 0xdef00000, 0x00000200, 0x5554aaaa },//shl a
	{ 0x67800000, 0xdef00000, 0x00800000, 0x80000000 },//shl b
	{ 0x00012345, 0x00009abc, 0x00400000, 0x55552aaa },//shr a
	{ 0x00000123, 0x00009abc, 0x00000100, 0x00000001 },//shr b
	{ 0x00012345, 0xffff9abc, 0xffc00000, 0xd5552aaa },//asr a
	{ 0x00000123, 0xffff9abc, 0xffffff00, 0xffffffff },//asr b
};

static dstatement_t uint_shiftop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 36 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 36, -1, 36 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 36 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 36, 1 },
	{ OP(1, 1, 1, OP_SHL_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_SHL_I_1), 0, 8, 16 },
	{ OP(1, 1, 1, OP_SHR_u_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_SHR_u_1), 0, 8, 24 },
	{ OP(1, 1, 1, OP_ASR_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_ASR_I_1), 0, 8, 32 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t uint_shiftop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 4, 0, 36 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 36, -2, 36 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 36 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 36, 1 },
	{ OP(1, 1, 1, OP_SHL_I_2), 0, 4, 12 },
	{ OP(1, 1, 1, OP_SHL_I_2), 0, 8, 16 },
	{ OP(1, 1, 1, OP_SHR_u_2), 0, 4, 20 },
	{ OP(1, 1, 1, OP_SHR_u_2), 0, 8, 24 },
	{ OP(1, 1, 1, OP_ASR_I_2), 0, 4, 28 },
	{ OP(1, 1, 1, OP_ASR_I_2), 0, 8, 32 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t uint_shiftop_3a_statements[] = {
	{ OP(1, 1, 1, OP_SHL_I_3), 0,  4, 12 },
	{ OP(1, 1, 1, OP_SHL_I_1), 3,  7, 15 },
	{ OP(1, 1, 1, OP_SHL_I_3), 0,  8, 16 },
	{ OP(1, 1, 1, OP_SHL_I_1), 3, 11, 19 },
	{ OP(1, 1, 1, OP_SHR_u_3), 0,  4, 20 },
	{ OP(1, 1, 1, OP_SHR_u_1), 3,  7, 23 },
	{ OP(1, 1, 1, OP_SHR_u_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHR_u_1), 3, 11, 27 },
	{ OP(1, 1, 1, OP_ASR_I_3), 0,  4, 28 },
	{ OP(1, 1, 1, OP_ASR_I_1), 3,  7, 31 },
	{ OP(1, 1, 1, OP_ASR_I_3), 0,  8, 32 },
	{ OP(1, 1, 1, OP_ASR_I_1), 3, 11, 35 },
};

static dstatement_t uint_shiftop_3b_statements[] = {
	{ OP(1, 1, 1, OP_SHL_I_1), 0, 4, 12 },
	{ OP(1, 1, 1, OP_SHL_I_3), 1, 5, 13 },
	{ OP(1, 1, 1, OP_SHL_I_1), 0, 8, 16 },
	{ OP(1, 1, 1, OP_SHL_I_3), 1, 9, 17 },
	{ OP(1, 1, 1, OP_SHR_u_1), 0, 4, 20 },
	{ OP(1, 1, 1, OP_SHR_u_3), 1, 5, 21 },
	{ OP(1, 1, 1, OP_SHR_u_1), 0, 8, 24 },
	{ OP(1, 1, 1, OP_SHR_u_3), 1, 9, 25 },
	{ OP(1, 1, 1, OP_ASR_I_1), 0, 4, 28 },
	{ OP(1, 1, 1, OP_ASR_I_3), 1, 5, 29 },
	{ OP(1, 1, 1, OP_ASR_I_1), 0, 8, 32 },
	{ OP(1, 1, 1, OP_ASR_I_3), 1, 9, 33 },
};

static dstatement_t uint_shiftop_4_statements[] = {
	{ OP(1, 1, 1, OP_SHL_I_4), 0, 4, 12 },
	{ OP(1, 1, 1, OP_SHL_I_4), 0, 8, 16 },
	{ OP(1, 1, 1, OP_SHR_u_4), 0, 4, 20 },
	{ OP(1, 1, 1, OP_SHR_u_4), 0, 8, 24 },
	{ OP(1, 1, 1, OP_ASR_I_4), 0, 4, 28 },
	{ OP(1, 1, 1, OP_ASR_I_4), 0, 8, 32 },
};

static pr_ulvec4_t ulong_shiftop_init[] = {
	{ UINT64_C(0x123456789abcdef0), UINT64_C(0x9abcdef012345678),
		UINT64_C(0x8000000180000001), UINT64_C(0xaaaa5555aaaa5555) },
	{         12,         16,          9,          1 },
	{         52,         48,         55,         63 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
	{          0,          0,          0,          0 },
};

static pr_ulvec4_t ulong_shiftop_expect[] = {
	{ UINT64_C(0x123456789abcdef0), UINT64_C(0x9abcdef012345678),
		UINT64_C(0x8000000180000001), UINT64_C(0xaaaa5555aaaa5555) },
	{         12,         16,          9,          1 },//a
	{         52,         48,         55,         63 },//b
	{ UINT64_C(0x456789abcdef0000), UINT64_C(0xdef0123456780000),
		UINT64_C(0x0000030000000200), UINT64_C(0x5554aaab5554aaaa) },//shl a
	{ UINT64_C(0xef00000000000000), UINT64_C(0x5678000000000000),
		UINT64_C(0x0080000000000000), UINT64_C(0x8000000000000000) },//shl b
	{ UINT64_C(0x000123456789abcd), UINT64_C(0x00009abcdef01234),
		UINT64_C(0x0040000000c00000), UINT64_C(0x55552aaad5552aaa) },//shr a
	{ UINT64_C(0x0000000000000123), UINT64_C(0x0000000000009abc),
		UINT64_C(0x0000000000000100), UINT64_C(0x0000000000000001) },//shr b
	{ UINT64_C(0x000123456789abcd), UINT64_C(0xffff9abcdef01234),
		UINT64_C(0xffc0000000c00000), UINT64_C(0xd5552aaad5552aaa) },//asr a
	{ UINT64_C(0x0000000000000123), UINT64_C(0xffffffffffff9abc),
		UINT64_C(0xffffffffffffff00), UINT64_C(0xffffffffffffffff) },//asr b
};

static dstatement_t ulong_shiftop_1_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 72 },	// init index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 72, -2, 72 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 72 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 72, 1 },
	{ OP(1, 1, 1, OP_SHL_L_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHL_L_1), 0, 16, 32 },
	{ OP(1, 1, 1, OP_SHR_U_1), 0,  8, 40 },
	{ OP(1, 1, 1, OP_SHR_U_1), 0, 16, 48 },
	{ OP(1, 1, 1, OP_ASR_L_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_ASR_L_1), 0, 16, 64 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t ulong_shiftop_2_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A), 8, 0, 72 },	// index
//loop:
	{ OP(0, 0, 0, OP_LEA_C), 72, -4, 72 },	// dec index
	{ OP(0, 0, 0, OP_IFAE), 2, 0, 72 },
	{ OP(0, 0, 0, OP_BREAK), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 4, 72, 1 },
	{ OP(1, 1, 1, OP_SHL_L_2), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHL_L_2), 0, 16, 32 },
	{ OP(1, 1, 1, OP_SHR_U_2), 0,  8, 40 },
	{ OP(1, 1, 1, OP_SHR_U_2), 0, 16, 48 },
	{ OP(1, 1, 1, OP_ASR_L_2), 0,  8, 56 },
	{ OP(1, 1, 1, OP_ASR_L_2), 0, 16, 64 },
	{ OP(1, 1, 1, OP_JUMP_A), -10, 0, 0 },
};

static dstatement_t ulong_shiftop_3a_statements[] = {
	{ OP(1, 1, 1, OP_SHL_L_3), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHL_L_1), 6, 14, 30 },
	{ OP(1, 1, 1, OP_SHL_L_3), 0, 16, 32 },
	{ OP(1, 1, 1, OP_SHL_L_1), 6, 22, 38 },
	{ OP(1, 1, 1, OP_SHR_U_3), 0,  8, 40 },
	{ OP(1, 1, 1, OP_SHR_U_1), 6, 14, 46 },
	{ OP(1, 1, 1, OP_SHR_U_3), 0, 16, 48 },
	{ OP(1, 1, 1, OP_SHR_U_1), 6, 22, 54 },
	{ OP(1, 1, 1, OP_ASR_L_3), 0,  8, 56 },
	{ OP(1, 1, 1, OP_ASR_L_1), 6, 14, 62 },
	{ OP(1, 1, 1, OP_ASR_L_3), 0, 16, 64 },
	{ OP(1, 1, 1, OP_ASR_L_1), 6, 22, 70 },
};

static dstatement_t ulong_shiftop_3b_statements[] = {
	{ OP(1, 1, 1, OP_SHL_L_1), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHL_L_3), 2, 10, 26 },
	{ OP(1, 1, 1, OP_SHL_L_1), 0, 16, 32 },
	{ OP(1, 1, 1, OP_SHL_L_3), 2, 18, 34 },
	{ OP(1, 1, 1, OP_SHR_U_1), 0,  8, 40 },
	{ OP(1, 1, 1, OP_SHR_U_3), 2, 10, 42 },
	{ OP(1, 1, 1, OP_SHR_U_1), 0, 16, 48 },
	{ OP(1, 1, 1, OP_SHR_U_3), 2, 18, 50 },
	{ OP(1, 1, 1, OP_ASR_L_1), 0,  8, 56 },
	{ OP(1, 1, 1, OP_ASR_L_3), 2, 10, 58 },
	{ OP(1, 1, 1, OP_ASR_L_1), 0, 16, 64 },
	{ OP(1, 1, 1, OP_ASR_L_3), 2, 18, 66 },
};

static dstatement_t ulong_shiftop_4_statements[] = {
	{ OP(1, 1, 1, OP_SHL_L_4), 0,  8, 24 },
	{ OP(1, 1, 1, OP_SHL_L_4), 0, 16, 32 },
	{ OP(1, 1, 1, OP_SHR_U_4), 0,  8, 40 },
	{ OP(1, 1, 1, OP_SHR_U_4), 0, 16, 48 },
	{ OP(1, 1, 1, OP_ASR_L_4), 0,  8, 56 },
	{ OP(1, 1, 1, OP_ASR_L_4), 0, 16, 64 },
};

test_t tests[] = {
	{
		.desc = "uint divop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_divop_init,uint_divop_expect),
		.num_statements = num_statements (uint_divop_1_statements),
		.statements = uint_divop_1_statements,
		.init_globals = (pr_int_t *) uint_divop_init,
		.expect_globals = (pr_int_t *) uint_divop_expect,
	},
	{
		.desc = "uint divop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_divop_init,uint_divop_expect),
		.num_statements = num_statements (uint_divop_2_statements),
		.statements = uint_divop_2_statements,
		.init_globals = (pr_int_t *) uint_divop_init,
		.expect_globals = (pr_int_t *) uint_divop_expect,
	},
	{
		.desc = "uint divop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_divop_init,uint_divop_expect),
		.num_statements = num_statements (uint_divop_3a_statements),
		.statements = uint_divop_3a_statements,
		.init_globals = (pr_int_t *) uint_divop_init,
		.expect_globals = (pr_int_t *) uint_divop_expect,
	},
	{
		.desc = "uint divop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_divop_init,uint_divop_expect),
		.num_statements = num_statements (uint_divop_3b_statements),
		.statements = uint_divop_3b_statements,
		.init_globals = (pr_int_t *) uint_divop_init,
		.expect_globals = (pr_int_t *) uint_divop_expect,
	},
	{
		.desc = "uint divop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_divop_init,uint_divop_expect),
		.num_statements = num_statements (uint_divop_4_statements),
		.statements = uint_divop_4_statements,
		.init_globals = (pr_int_t *) uint_divop_init,
		.expect_globals = (pr_int_t *) uint_divop_expect,
	},
	{
		.desc = "uint cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_1_statements),
		.statements = uint_cmpop_1_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_2_statements),
		.statements = uint_cmpop_2_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_3a_statements),
		.statements = uint_cmpop_3a_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_3b_statements),
		.statements = uint_cmpop_3b_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "uint cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_cmpop_init,uint_cmpop_expect),
		.num_statements = num_statements (uint_cmpop_4_statements),
		.statements = uint_cmpop_4_statements,
		.init_globals = (pr_int_t *) uint_cmpop_init,
		.expect_globals = (pr_int_t *) uint_cmpop_expect,
	},
	{
		.desc = "ulong divop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_divop_init,ulong_divop_expect),
		.num_statements = num_statements (ulong_divop_1_statements),
		.statements = ulong_divop_1_statements,
		.init_globals = (pr_int_t *) ulong_divop_init,
		.expect_globals = (pr_int_t *) ulong_divop_expect,
	},
	{
		.desc = "ulong divop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_divop_init,ulong_divop_expect),
		.num_statements = num_statements (ulong_divop_2_statements),
		.statements = ulong_divop_2_statements,
		.init_globals = (pr_int_t *) ulong_divop_init,
		.expect_globals = (pr_int_t *) ulong_divop_expect,
	},
	{
		.desc = "ulong divop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_divop_init,ulong_divop_expect),
		.num_statements = num_statements (ulong_divop_3a_statements),
		.statements = ulong_divop_3a_statements,
		.init_globals = (pr_int_t *) ulong_divop_init,
		.expect_globals = (pr_int_t *) ulong_divop_expect,
	},
	{
		.desc = "ulong divop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_divop_init,ulong_divop_expect),
		.num_statements = num_statements (ulong_divop_3b_statements),
		.statements = ulong_divop_3b_statements,
		.init_globals = (pr_int_t *) ulong_divop_init,
		.expect_globals = (pr_int_t *) ulong_divop_expect,
	},
	{
		.desc = "ulong divop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_divop_init,ulong_divop_expect),
		.num_statements = num_statements (ulong_divop_4_statements),
		.statements = ulong_divop_4_statements,
		.init_globals = (pr_int_t *) ulong_divop_init,
		.expect_globals = (pr_int_t *) ulong_divop_expect,
	},
	{
		.desc = "ulong cmpop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_1_statements),
		.statements = ulong_cmpop_1_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_2_statements),
		.statements = ulong_cmpop_2_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_3a_statements),
		.statements = ulong_cmpop_3a_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_3b_statements),
		.statements = ulong_cmpop_3b_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "ulong cmpop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_cmpop_init,ulong_cmpop_expect),
		.num_statements = num_statements (ulong_cmpop_4_statements),
		.statements = ulong_cmpop_4_statements,
		.init_globals = (pr_int_t *) ulong_cmpop_init,
		.expect_globals = (pr_int_t *) ulong_cmpop_expect,
	},
	{
		.desc = "uint shiftop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_shiftop_init,uint_shiftop_expect),
		.num_statements = num_statements (uint_shiftop_1_statements),
		.statements = uint_shiftop_1_statements,
		.init_globals = (pr_int_t *) uint_shiftop_init,
		.expect_globals = (pr_int_t *) uint_shiftop_expect,
	},
	{
		.desc = "uint shiftop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_shiftop_init,uint_shiftop_expect),
		.num_statements = num_statements (uint_shiftop_2_statements),
		.statements = uint_shiftop_2_statements,
		.init_globals = (pr_int_t *) uint_shiftop_init,
		.expect_globals = (pr_int_t *) uint_shiftop_expect,
	},
	{
		.desc = "uint shiftop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_shiftop_init,uint_shiftop_expect),
		.num_statements = num_statements (uint_shiftop_3a_statements),
		.statements = uint_shiftop_3a_statements,
		.init_globals = (pr_int_t *) uint_shiftop_init,
		.expect_globals = (pr_int_t *) uint_shiftop_expect,
	},
	{
		.desc = "uint shiftop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_shiftop_init,uint_shiftop_expect),
		.num_statements = num_statements (uint_shiftop_3b_statements),
		.statements = uint_shiftop_3b_statements,
		.init_globals = (pr_int_t *) uint_shiftop_init,
		.expect_globals = (pr_int_t *) uint_shiftop_expect,
	},
	{
		.desc = "uint shiftop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(uint_shiftop_init,uint_shiftop_expect),
		.num_statements = num_statements (uint_shiftop_4_statements),
		.statements = uint_shiftop_4_statements,
		.init_globals = (pr_int_t *) uint_shiftop_init,
		.expect_globals = (pr_int_t *) uint_shiftop_expect,
	},
	{
		.desc = "ulong shiftop 1",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_shiftop_init,ulong_shiftop_expect),
		.num_statements = num_statements (ulong_shiftop_1_statements),
		.statements = ulong_shiftop_1_statements,
		.init_globals = (pr_int_t *) ulong_shiftop_init,
		.expect_globals = (pr_int_t *) ulong_shiftop_expect,
	},
	{
		.desc = "ulong shiftop 2",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_shiftop_init,ulong_shiftop_expect),
		.num_statements = num_statements (ulong_shiftop_2_statements),
		.statements = ulong_shiftop_2_statements,
		.init_globals = (pr_int_t *) ulong_shiftop_init,
		.expect_globals = (pr_int_t *) ulong_shiftop_expect,
	},
	{
		.desc = "ulong shiftop 3a",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_shiftop_init,ulong_shiftop_expect),
		.num_statements = num_statements (ulong_shiftop_3a_statements),
		.statements = ulong_shiftop_3a_statements,
		.init_globals = (pr_int_t *) ulong_shiftop_init,
		.expect_globals = (pr_int_t *) ulong_shiftop_expect,
	},
	{
		.desc = "ulong shiftop 3b",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_shiftop_init,ulong_shiftop_expect),
		.num_statements = num_statements (ulong_shiftop_3b_statements),
		.statements = ulong_shiftop_3b_statements,
		.init_globals = (pr_int_t *) ulong_shiftop_init,
		.expect_globals = (pr_int_t *) ulong_shiftop_expect,
	},
	{
		.desc = "ulong shiftop 4",
		.extra_globals = 4 * 1,
		.num_globals = num_globals(ulong_shiftop_init,ulong_shiftop_expect),
		.num_statements = num_statements (ulong_shiftop_4_statements),
		.statements = ulong_shiftop_4_statements,
		.init_globals = (pr_int_t *) ulong_shiftop_init,
		.expect_globals = (pr_int_t *) ulong_shiftop_expect,
	},
};

#include "main.c"
