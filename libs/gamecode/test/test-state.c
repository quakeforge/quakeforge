#include "head.c"

#include "QF/mathlib.h"

#define DB 0xdeadbeef

static float float_time = 1;

static pr_ivec4_t float_state_init[] = {
	{ 0,   0, DB, DB },
	{ 10, 11, 20, 21 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
};

#define f1  0x3f800000
#define f11 0x3f8ccccd
#define f2  0x40000000
static pr_ivec4_t float_state_expect[] = {
	{  0,  8,  f1, DB },
	{ 10, 11,  20, 21 },
	{  0,  0,   0,  0 },
	{ 10, 20, f11,  0 },
	{ 11, 21,  f2,  0 },
};

static dstatement_t float_state_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),     4, 0, 1 },
	{ OP(0, 0, 0, OP_STATE_ft),  4, 6, 0 },
	{ OP(0, 0, 0, OP_LEA_A),     8, 0, 1 },
	{ OP(0, 0, 0, OP_STATE_ftt), 5, 7, 2 },
};

static double double_time = 1;

static pr_ivec4_t double_state_init[] = {
	{ 0,   0, DB, DB },
	{ 10, 11, 20, 21 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
	{  0,  0,  0,  0 },
};

#define d1l  0x00000000
#define d1h  0x3ff00000
#define d11l 0x9999999a
#define d11h 0x3ff19999
#define d2l  0x00000000
#define d2h  0x40000000
static pr_ivec4_t double_state_expect[] = {
	{  0,  8,  d1l,  d1h },
	{ 10, 11,   20,   21 },
	{  0,  0,    0,    0 },
	{ 10, 20, d11l, d11h },
	{ 11, 21,  d2l,  d2h },
};

static dstatement_t double_state_statements[] = {
	{ OP(0, 0, 0, OP_LEA_A),     4, 0, 1 },
	{ OP(0, 0, 0, OP_STATE_dt),  4, 6, 0 },
	{ OP(0, 0, 0, OP_LEA_A),     8, 0, 1 },
	{ OP(0, 0, 0, OP_STATE_dtt), 5, 7, 2 },
};

test_t tests[] = {
	{
		.desc = "float state",
		.num_globals = num_globals(float_state_init,float_state_expect),
		.num_statements = num_statements (float_state_statements),
		.statements = float_state_statements,
		.init_globals = (pr_int_t *) float_state_init,
		.expect_globals = (pr_int_t *) float_state_expect,
		.self = 1,
		.ftime = 2,
		.float_time = &float_time,
		.edict_area = 8,
		.frame = 0,
		.think = 1,
		.nextthink = 2,
	},
	{
		.desc = "double state",
		.num_globals = num_globals(double_state_init,double_state_expect),
		.num_statements = num_statements (double_state_statements),
		.statements = double_state_statements,
		.init_globals = (pr_int_t *) double_state_init,
		.expect_globals = (pr_int_t *) double_state_expect,
		.self = 1,
		.dtime = 2,
		.double_time = &double_time,
		.edict_area = 8,
		.frame = 0,
		.think = 1,
		.nextthink = 2,
	},
};

#include "main.c"
