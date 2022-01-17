#include "head.c"

#include "QF/mathlib.h"

#define sq(x) ((float)(x)*(float)(x))
#define pi_6 0x3f060a92		// pi/6
#define r3_2 0x3f5db3d7		// sqrt(3)/2
#define f1_2 0x3f000000		// 1/2
#define f1   0x3f800000		// 1
#define f2   0x40000000		// 2
#define shx  0x3f0c4020		// sinh(pi/6)
#define chx  0x3f91f354		// cosh(pi/6)

#define DB 0xdeadbeef

#define STK  (32 * 4)		// stack ptr just after globals

static pr_ivec4_t float_callret_init[32] = {
	{  0, pi_6,  2,  0},
	{ f1,   f2,  0,  0},
	// result
	{ DB,   DB, DB, DB },
	// pre-call with
	{ DB,   DB, DB, DB },
	// post-call with
	{ DB,   DB, DB, DB },
	{ DB,   DB, DB, DB },
};

static pr_ivec4_t float_callret_expect[32] = {
	// constants
	{    0, pi_6,   2,   0 },
	{   f1,   f2,   0,   0 },
	// result
	{ r3_2, f1_2, chx, shx },
	// pre-call with: should be all 0 on progs init
	{    0,    0,   0,   0 },
	// post-call with; should be restored to pre-call values (in this case,
	// progs init)
	{    0,    0,   0,   0 },
	{   DB,   DB,  DB,  DB },
};

static dstatement_t float_callret_statements[] = {
	{ OP_WITH,      8,   0,   0 },			// pushregs
	{ OP_POP_A_4,  12,   0,   0 },
	{ OP_STORE_A_1, 7,   0, STK },			// save stack pointer for check
	{ OP_PUSH_A_1,  1,   0,   0 },
	{ OP_CALL_B,    2,   0,   8 },
	{ OP_LEA_C,   STK,   4, STK },			// discard param
	{ OP_SUB_I_1,   7, STK,   7 },			// check stack restored
	{ OP_WITH,      8,   0,   0 },			// pushregs
	{ OP_POP_A_4,  16,   0,   0 },
	{ OP_BREAK },
// cos_sin_cosh_sinh:
// calculate cos(x), sin(x), cosh(x) and sinh(x) simultaneously
	{ OP_WITH,      2,   0,   1 },			// put params into reg 1
	{ OP_LEA_C,   STK, -24, STK },			// reserve 24 words on the stack
	{ OP_WITH,      2,   0,   2 },			// put locals into reg 2
#define x 0		// in parameters	float
#define xn 0	// in locals		vec4
#define x2 4	// in locals		vec4
#define ac 8	// in locals		vec4
#define fa 12	// in locals		vec4
#define fi 16	// in locals		vec4
#define c  20	// in locals		int
	{ OP(2, 0, 1, OP_STORE_A_1), xn+1,0,  x },		// init xn to [1, x, 0, 0]
	{ OP(2, 0, 0, OP_STORE_A_1), xn,  0,  4 },
	{ OP(2, 0, 2, OP_SWIZZLE_F), xn, 0x0044, xn },	// xn -> [1, x, 1, x]
	{ OP(1, 1, 2, OP_MUL_F_1),    x,  x, x2 },	// x2 -> [x*x, 0, 0, 0]
	{ OP(2, 0, 2, OP_SWIZZLE_F), x2, 0x0300, x2},//x2 -> [-x*x, -x*x, x*x, x*x]
	{ OP(2, 0, 0, OP_STORE_A_1), fa,  0,  4 },		// init factorial
	{ OP(2, 0, 0, OP_STORE_A_1), fa+1,0,  5 },
	{ OP(2, 0, 2, OP_SWIZZLE_F), fa, 0x0044, fa },	// fa -> [1, 2, 1, 2]
	{ OP(0, 0, 2, OP_SWIZZLE_F),  4, 0x0000, fi },	// init fi -> [1, 1, 1, 1]
	{ OP(2, 2, 2, OP_SUB_F_4),   ac, ac, ac },		// init acc (output) to 0
	{ OP(0, 0, 2, OP_LEA_A),     25,  0,  c },		// init count
// loop:
	{ OP(2, 2, 2, OP_ADD_F_4),   ac, xn, ac },		// acc += xn
	{ OP(2, 2, 2, OP_MUL_F_4),   xn, x2, xn },		// xn *= x2
	{ OP(2, 2, 2, OP_DIV_F_4),   xn, fa, xn },		// xn /= f
	{ OP(2, 2, 2, OP_ADD_F_4),   fa, fi, fa },		// f += inc
	{ OP(2, 2, 2, OP_DIV_F_4),   xn, fa, xn },		// xn /= f
	{ OP(2, 2, 2, OP_ADD_F_4),   fa, fi, fa },		// f += inc
	{ OP(2, 0, 2, OP_LEA_C),      c, -1,  c },		// dec count
	{ OP(0, 0, 2, OP_IFA),       -7,  0,  c },		// count > 0
	{ OP(2, 0, 0, OP_RETURN),    ac,  0,  4 },
};
#undef x
#undef xn
#undef x2
#undef ac
#undef fa
#undef fi
#undef c

static bfunction_t callret_functions[] = {
	{ .first_statement = 10 },
};

test_t tests[] = {
	{
		.desc = "callret",
		.num_globals = num_globals(float_callret_init,float_callret_expect),
		.num_statements = num_statements (float_callret_statements),
		.statements = float_callret_statements,
		.init_globals = (pr_int_t *) float_callret_init,
		.expect_globals = (pr_int_t *) float_callret_expect,
		.functions = callret_functions,
		.num_functions = num_functions(callret_functions),
		.stack_size = 128,
	},
};

#include "main.c"
