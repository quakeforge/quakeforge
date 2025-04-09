#include "head.c"

#include "QF/mathlib.h"

static pr_vec4_t float_scale_init[] = {
	{  5,  0,  0,  0},
	{  3,  4, 13, 85},
	{  0,  0, -1, -2},
	{  0,  0,  0, -3},
	{  0,  0,  0,  0},
};

static pr_vec4_t float_scale_expect[] = {
	{  5,  0,  0,   0},
	{  3,  4, 13,  85},
	{ 15, 20, -1,  -2},
	{ 15, 20, 65,  -3},
	{ 15, 20, 65, 425},
};

static dstatement_t float_scale_statements[] = {
	{ OP(1, 1, 1, OP_SCALE_F_2), 4, 0,  8 },
	{ OP(1, 1, 1, OP_SCALE_F_3), 4, 0, 12 },
	{ OP(1, 1, 1, OP_SCALE_F_4), 4, 0, 16 },
};

static pr_dvec4_t double_scale_init[] = {
	{  5,  0,  0,  0},
	{  3,  4, 13, 85},
	{  0,  0, -1, -2},
	{  0,  0,  0, -3},
	{  0,  0,  0,  0},
};

static pr_dvec4_t double_scale_expect[] = {
	{  5,  0,  0,   0},
	{  3,  4, 13,  85},
	{ 15, 20, -1,  -2},
	{ 15, 20, 65,  -3},
	{ 15, 20, 65, 425},
};

static dstatement_t double_scale_statements[] = {
	{ OP(1, 1, 1, OP_SCALE_D_2), 8, 0, 16 },
	{ OP(1, 1, 1, OP_SCALE_D_3), 8, 0, 24 },
	{ OP(1, 1, 1, OP_SCALE_D_4), 8, 0, 32 },
};

test_t tests[] = {
	{
		.desc = "float scale",
		.num_globals = num_globals(float_scale_init,float_scale_expect),
		.num_statements = num_statements (float_scale_statements),
		.statements = float_scale_statements,
		.init_globals = (pr_int_t *) float_scale_init,
		.expect_globals = (pr_int_t *) float_scale_expect,
	},
	{
		.desc = "double scale",
		.num_globals = num_globals(double_scale_init,double_scale_expect),
		.num_statements = num_statements (double_scale_statements),
		.statements = double_scale_statements,
		.init_globals = (pr_int_t *) double_scale_init,
		.expect_globals = (pr_int_t *) double_scale_expect,
	},
};

#include "main.c"
