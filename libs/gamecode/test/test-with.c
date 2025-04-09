#include "head.c"

#include "QF/mathlib.h"

#define DB 0xdeadbeef

static pr_ivec4_t pushregs_init[] = {
	{ DB, DB, DB, DB},
};

static pr_ivec4_t pushregs_expect[] = {
	{ 0, 0, 0, 0},	// initial base regs should all be 0
};

static dstatement_t pushregs_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 0, 0, 0 },
};

static pr_ivec4_t popregs_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t popregs_expect[] = {
	{ 4, 5, 6, 7},
	{ 7, 6, 5, 4},
};

static dstatement_t popregs_statements[] = {
	{ OP(0, 0, 0, OP_PUSH_A_4), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 9, 0, 0 },		// popregs
	{ OP(3, 0, 0, OP_LEA_A), 0, 0, 0 },
	{ OP(2, 0, 1, OP_LEA_A), 0, 0, 0 },
	{ OP(1, 0, 2, OP_LEA_A), 0, 0, 0 },
	{ OP(0, 0, 3, OP_LEA_A), 0, 0, 0 },
};

static pr_ivec4_t with_0_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_0_expect[] = {
	{ 4, 5, 6, 7},
	{ 0, 1, 6, 7},
	{ 4, 5, 0, 1},
	{ 0, 0, 0, 0},
};

static dstatement_t with_0_statements[] = {
	{ OP(0, 0, 0, OP_PUSH_A_4), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH),  9, 0, 0 },		// popregs
	{ OP(0, 0, 0, OP_WITH),  0, 0, 0 },		// set reg 0 to 0
	{ OP(0, 0, 0, OP_WITH),  0, 1, 1 },		// set reg 1 to 1
	{ OP(0, 0, 0, OP_WITH),  8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
	{ OP(0, 0, 0, OP_PUSH_A_4), 0, 0, 0 },
	{ OP(0, 0, 0, OP_WITH),  9, 0, 0 },		// popregs
	{ OP(0, 0, 0, OP_WITH),  0, 0, 2 },		// set reg 2 to 0
	{ OP(0, 0, 0, OP_WITH),  0, 1, 3 },		// set reg 3 to 1
	{ OP(0, 0, 0, OP_WITH),  8, 0, 0 },		// pushregs
	{ OP(2, 0, 0, OP_POP_A_4), 8, 0, 0 },
	{ OP(0, 0, 0, OP_WITH), 10, 0, 0 },		// reset
	{ OP(0, 0, 0, OP_WITH),  8, 0, 0 },		// pushregs
	{ OP(2, 0, 0, OP_POP_A_4), 12, 0, 0 },
};

static pr_ivec4_t with_1_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_1_expect[] = {
	{ 4, 5, 6, 7},
	{ 0, 4, 6, 2},
};

static dstatement_t with_1_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 0,  4, 1 },
	{ OP(0, 1, 0, OP_WITH), 1,  2, 2 },
	{ OP(0, 1, 0, OP_WITH), 1, -2, 3 },
	{ OP(0, 0, 0, OP_WITH), 8,  0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_2_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_2_expect[] = {
	{  4,  5,  6,  7},
	{  0, 44, 48, 40},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
};

static dstatement_t with_2_statements[] = {
	{ OP(0, 0, 0, OP_PUSH_A_4), 0, 0, 0 },	// so something is on the stack
	{ OP(0, 0, 0, OP_WITH), 2,  0, 1 },
	{ OP(0, 1, 0, OP_WITH), 2,  4, 2 },
	{ OP(0, 1, 0, OP_WITH), 2, -4, 3 },
	{ OP(0, 0, 0, OP_WITH), 8,  0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_3_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_3_expect[] = {
	{  4,  5,  6,     7},
	{  0, 64, 68, 65596},	// edict-area relative is only +ve
};

static dstatement_t with_3_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 3,  0, 1 },
	{ OP(0, 1, 0, OP_WITH), 3,  4, 2 },
	{ OP(0, 1, 0, OP_WITH), 3, -4, 3 },
	{ OP(0, 0, 0, OP_WITH), 8,  0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_4_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_4_expect[] = {
	{ 4, 5, 6, 7},
	{ 0, 4, 5, 6},
};

static dstatement_t with_4_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 4, 0, 1 },
	{ OP(0, 1, 0, OP_WITH), 4, 1, 2 },
	{ OP(0, 1, 0, OP_WITH), 4, 2, 3 },
	{ OP(0, 0, 0, OP_WITH), 8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_5_init[] = {
	{  4,  5,  6,  7},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_5_expect[] = {
	{ 4, 5, 6, 7},
	{ 0, 2, 6, 7},
};

static dstatement_t with_5_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 0, 2, 1 },
	{ OP(0, 1, 0, OP_WITH), 5, 0, 2 },
	{ OP(0, 1, 0, OP_WITH), 5, 1, 3 },
	{ OP(0, 0, 0, OP_WITH), 8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_6_init[] = {
	{  4,  5,  6, -4},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_6_expect[] = {
	{  4,  5,  6, -4},
	{  0, 44, 48, 40},
	{ DB, DB, DB, DB},
	{ DB, DB, DB, DB},
};

static dstatement_t with_6_statements[] = {
	{ OP(0, 0, 0, OP_PUSH_A_4), 0, 0, 0 },	// so something is on the stack
	{ OP(0, 0, 0, OP_WITH), 2, 0, 1 },
	{ OP(0, 1, 0, OP_WITH), 6, 0, 2 },
	{ OP(0, 1, 0, OP_WITH), 6, 3, 3 },
	{ OP(0, 0, 0, OP_WITH), 8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

static pr_ivec4_t with_7_init[] = {
	{  4,  5,  6, -4},
	{ DB, DB, DB, DB},
};

static pr_ivec4_t with_7_expect[] = {
	{ 4, 5,  6, -4},
	{ 0, 2, 70, 60},	// edict-area relative is only +ve, but 32-bit wrap
};

static dstatement_t with_7_statements[] = {
	{ OP(0, 0, 0, OP_WITH), 0, 2, 1 },
	{ OP(0, 1, 0, OP_WITH), 7, 0, 2 },
	{ OP(0, 1, 0, OP_WITH), 7, 1, 3 },
	{ OP(0, 0, 0, OP_WITH), 8, 0, 0 },		// pushregs
	{ OP(0, 0, 0, OP_POP_A_4), 4, 0, 0 },
};

test_t tests[] = {
	{
		.desc = "pushregs",
		.num_globals = num_globals(pushregs_init,pushregs_expect),
		.num_statements = num_statements (pushregs_statements),
		.statements = pushregs_statements,
		.init_globals = (pr_int_t *) pushregs_init,
		.expect_globals = (pr_int_t *) pushregs_expect,
		.stack_size = 32,
	},
	{
		.desc = "popregs",
		.num_globals = num_globals(popregs_init,popregs_expect),
		.num_statements = num_statements (popregs_statements),
		.statements = popregs_statements,
		.init_globals = (pr_int_t *) popregs_init,
		.expect_globals = (pr_int_t *) popregs_expect,
		.stack_size = 32,
	},
	{
		.desc = "with 0",
		.num_globals = num_globals(with_0_init,with_0_expect),
		.num_statements = num_statements (with_0_statements),
		.statements = with_0_statements,
		.init_globals = (pr_int_t *) with_0_init,
		.expect_globals = (pr_int_t *) with_0_expect,
		.stack_size = 32,
	},
	{
		.desc = "with 1",
		.num_globals = num_globals(with_1_init,with_1_expect),
		.num_statements = num_statements (with_1_statements),
		.statements = with_1_statements,
		.init_globals = (pr_int_t *) with_1_init,
		.expect_globals = (pr_int_t *) with_1_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 2",
		.num_globals = num_globals(with_2_init,with_2_expect),
		.num_statements = num_statements (with_2_statements),
		.statements = with_2_statements,
		.init_globals = (pr_int_t *) with_2_init,
		.expect_globals = (pr_int_t *) with_2_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 3",
		.num_globals = num_globals(with_3_init,with_3_expect),
		.num_statements = num_statements (with_3_statements),
		.statements = with_3_statements,
		.init_globals = (pr_int_t *) with_3_init,
		.expect_globals = (pr_int_t *) with_3_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 4",
		.num_globals = num_globals(with_4_init,with_4_expect),
		.num_statements = num_statements (with_4_statements),
		.statements = with_4_statements,
		.init_globals = (pr_int_t *) with_4_init,
		.expect_globals = (pr_int_t *) with_4_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 5",
		.num_globals = num_globals(with_5_init,with_5_expect),
		.num_statements = num_statements (with_5_statements),
		.statements = with_5_statements,
		.init_globals = (pr_int_t *) with_5_init,
		.expect_globals = (pr_int_t *) with_5_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 6",
		.num_globals = num_globals(with_6_init,with_6_expect),
		.num_statements = num_statements (with_6_statements),
		.statements = with_6_statements,
		.init_globals = (pr_int_t *) with_6_init,
		.expect_globals = (pr_int_t *) with_6_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
	{
		.desc = "with 7",
		.num_globals = num_globals(with_7_init,with_7_expect),
		.num_statements = num_statements (with_7_statements),
		.statements = with_7_statements,
		.init_globals = (pr_int_t *) with_7_init,
		.expect_globals = (pr_int_t *) with_7_expect,
		.stack_size = 32,
		.edict_area = 64,
	},
};

#include "main.c"
