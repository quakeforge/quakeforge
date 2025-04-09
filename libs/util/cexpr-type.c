/*
	cexpr-type.c

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>
#include <string.h>

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/simd/vec4f.h"

#include "libs/util/cexpr-parse.h"

#undef uint
#define uint unsigned

#define BINOP(pre, opname, type, op)										\
static void																	\
pre##_##opname (const exprval_t *a, const exprval_t *b, exprval_t *c,		\
				exprctx_t *ctx)												\
{																			\
	(*(type *) c->value) = (*(type *) a->value) op (*(type *) b->value);	\
}

#define CASTOP(dst_type, src_type)											\
static void																	\
dst_type##_##cast##_##src_type (const exprval_t *a, const exprval_t *src,	\
							 exprval_t *res, exprctx_t *ctx)				\
{																			\
	(*(dst_type *) res->value) = *(src_type *) src->value;					\
}

#define UNOP(pre, opname, type, op)									\
static void															\
pre##_##opname (const exprval_t *a, exprval_t *b, exprctx_t *ctx)	\
{																	\
	(*(type *) b->value) = op (*(type *) a->value);					\
}

BINOP(bool, and, bool, &)
BINOP(bool, or, bool, |)
BINOP(bool, xor, bool, ^)

static void
bool_cast_int (const exprval_t *val1, const exprval_t *src, exprval_t *result,
			   exprctx_t *ctx)
{
	*(bool *) result->value = !!*(int *) src->value;
}

UNOP(bool, not, bool, !)

static const char *
bool_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return *(bool *) val->value ? "true" : "false";
}

binop_t bool_binops[] = {
	{ '&', &cexpr_bool, &cexpr_bool, bool_and },
	{ '|', &cexpr_bool, &cexpr_bool, bool_or },
	{ '^', &cexpr_bool, &cexpr_bool, bool_xor },
	{ '=', &cexpr_plitem, &cexpr_bool, cexpr_cast_plitem },
	{ '=', &cexpr_int, &cexpr_bool, bool_cast_int },
	{}
};

unop_t bool_unops[] = {
	{ '!', &cexpr_bool, bool_not },
	{}
};

exprtype_t cexpr_bool = {
	.name = "bool",
	.size = sizeof (bool),
	.binops = bool_binops,
	.unops = bool_unops,
	.data = &cexpr_bool_enum,
	.get_string = bool_get_string,
};

static bool bool_values[] = {
	false,
	true,
};
static exprsym_t bool_symbols[] = {
	{"false", &cexpr_bool, bool_values + 0},
	{"true", &cexpr_bool, bool_values + 1},
	{}
};
static exprtab_t bool_symtab = {
	bool_symbols,
};
exprenum_t cexpr_bool_enum = {
	&cexpr_bool,
	&bool_symtab,
};

BINOP(int, shl, int, <<)
BINOP(int, shr, int, >>)
BINOP(int, add, int, +)
BINOP(int, sub, int, -)
BINOP(int, mul, int, *)
BINOP(int, div, int, /)
BINOP(int, band, int, &)
BINOP(int, bor, int, |)
BINOP(int, xor, int, ^)
BINOP(int, rem, int, %)

UNOP(int, pos, int, +)
UNOP(int, neg, int, -)
UNOP(int, tnot, int, !)
UNOP(int, bnot, int, ~)

static void
int_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
		 exprctx_t *ctx)
{
	// implement true modulo for integers:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	int         a = *(int *) val1->value;
	int         b = *(int *) val2->value;
	int         c = a % b;
	// % is really remainder and so has the same sign rules
	// as division: -5 % 3 = -2, so need to add b (3 here)
	// if c's sign is incorrect, but only if c is non-zero
	int         mask = (a ^ b) >> 31;
	mask &= ~(!!c + 0) + 1; // +0 to convert bool to int (gcc)
	*(int *) result->value = c + (mask & b);
}

static const char *
int_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%d", *(int *) val->value);
}

binop_t int_binops[] = {
	{ SHL, &cexpr_int, &cexpr_int, int_shl },
	{ SHR, &cexpr_int, &cexpr_int, int_shr },
	{ '+', &cexpr_int, &cexpr_int, int_add },
	{ '-', &cexpr_int, &cexpr_int, int_sub },
	{ '*', &cexpr_int, &cexpr_int, int_mul },
	{ '/', &cexpr_int, &cexpr_int, int_div },
	{ '&', &cexpr_int, &cexpr_int, int_band },
	{ '|', &cexpr_int, &cexpr_int, int_bor },
	{ '^', &cexpr_int, &cexpr_int, int_xor },
	{ '%', &cexpr_int, &cexpr_int, int_rem },
	{ MOD, &cexpr_int, &cexpr_int, int_mod },
	{ '=', &cexpr_plitem, &cexpr_int, cexpr_cast_plitem },
	{}
};

unop_t int_unops[] = {
	{ '+', &cexpr_int, int_pos },
	{ '-', &cexpr_int, int_neg },
	{ '!', &cexpr_int, int_tnot },
	{ '~', &cexpr_int, int_bnot },
	{}
};

exprtype_t cexpr_int = {
	.name = "int",
	.size = sizeof (int),
	.binops = int_binops,
	.unops = int_unops,
	.get_string = int_get_string,
};

BINOP(uint, shl, unsigned, <<)
BINOP(uint, shr, unsigned, >>)
BINOP(uint, add, unsigned, +)
BINOP(uint, sub, unsigned, -)
BINOP(uint, mul, unsigned, *)
BINOP(uint, div, unsigned, /)
BINOP(uint, band, unsigned, &)
BINOP(uint, bor, unsigned, |)
BINOP(uint, xor, unsigned, ^)
BINOP(uint, rem, unsigned, %)

static void
uint_cast_int (const exprval_t *val1, const exprval_t *src, exprval_t *result,
			   exprctx_t *ctx)
{
	*(unsigned *) result->value = *(int *) src->value;
}

UNOP(uint, pos, unsigned, +)
UNOP(uint, neg, unsigned, -)
UNOP(uint, tnot, unsigned, !)
UNOP(uint, bnot, unsigned, ~)

static const char *
uint_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%u", *(unsigned *) val->value);
}

binop_t uint_binops[] = {
	{ SHL, &cexpr_uint, &cexpr_uint, uint_shl },
	{ SHR, &cexpr_uint, &cexpr_uint, uint_shr },
	{ '+', &cexpr_uint, &cexpr_uint, uint_add },
	{ '-', &cexpr_uint, &cexpr_uint, uint_sub },
	{ '*', &cexpr_uint, &cexpr_uint, uint_mul },
	{ '/', &cexpr_uint, &cexpr_uint, uint_div },
	{ '&', &cexpr_uint, &cexpr_uint, uint_band },
	{ '|', &cexpr_uint, &cexpr_uint, uint_bor },
	{ '^', &cexpr_uint, &cexpr_uint, uint_xor },
	{ '%', &cexpr_uint, &cexpr_uint, uint_rem },
	{ MOD, &cexpr_uint, &cexpr_uint, uint_rem },
	{ '=', &cexpr_int,  &cexpr_uint, uint_cast_int },
	{ '=', &cexpr_plitem, &cexpr_uint, cexpr_cast_plitem },
	{}
};

unop_t uint_unops[] = {
	{ '+', &cexpr_uint, uint_pos },
	{ '-', &cexpr_uint, uint_neg },
	{ '!', &cexpr_uint, uint_tnot },
	{ '~', &cexpr_uint, uint_bnot },
	{}
};

exprtype_t cexpr_uint = {
	.name = "uint",
	.size = sizeof (unsigned),
	.binops = uint_binops,
	.unops = uint_unops,
	.get_string = uint_get_string,
};

BINOP(long, shl, int64_t, <<)
BINOP(long, shr, int64_t, >>)
BINOP(long, add, int64_t, +)
BINOP(long, sub, int64_t, -)
BINOP(long, mul, int64_t, *)
BINOP(long, div, int64_t, /)
BINOP(long, band, int64_t, &)
BINOP(long, bor, int64_t, |)
BINOP(long, xor, int64_t, ^)
BINOP(long, rem, int64_t, %)

UNOP(long, pos, int64_t, +)
UNOP(long, neg, int64_t, -)
UNOP(long, tnot, int64_t, !)
UNOP(long, bnot, int64_t, ~)

static void
long_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
		 exprctx_t *ctx)
{
	// implement true modulo for integers:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	int64_t     a = *(int64_t *) val1->value;
	int64_t     b = *(int64_t *) val2->value;
	int64_t     c = a % b;
	// % is really remainder and so has the same sign rules
	// as division: -5 % 3 = -2, so need to add b (3 here)
	// if c's sign is incorrect, but only if c is non-zero
	int64_t     mask = (a ^ b) >> 31;
	mask &= ~(!!c + INT64_C (0)) + 1; // +0 to convert bool to int (gcc)
	*(int64_t *) result->value = c + (mask & b);
}

static const char *
long_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%"PRId64, *(int64_t *) val->value);
}

binop_t long_binops[] = {
	{ SHL, &cexpr_long, &cexpr_long, long_shl },
	{ SHR, &cexpr_long, &cexpr_long, long_shr },
	{ '+', &cexpr_long, &cexpr_long, long_add },
	{ '-', &cexpr_long, &cexpr_long, long_sub },
	{ '*', &cexpr_long, &cexpr_long, long_mul },
	{ '/', &cexpr_long, &cexpr_long, long_div },
	{ '&', &cexpr_long, &cexpr_long, long_band },
	{ '|', &cexpr_long, &cexpr_long, long_bor },
	{ '^', &cexpr_long, &cexpr_long, long_xor },
	{ '%', &cexpr_long, &cexpr_long, long_rem },
	{ MOD, &cexpr_long, &cexpr_long, long_mod },
	{ '=', &cexpr_plitem, &cexpr_long, cexpr_cast_plitem },
	{}
};

unop_t long_unops[] = {
	{ '+', &cexpr_long, long_pos },
	{ '-', &cexpr_long, long_neg },
	{ '!', &cexpr_long, long_tnot },
	{ '~', &cexpr_long, long_bnot },
	{}
};

exprtype_t cexpr_long = {
	.name = "long",
	.size = sizeof (int64_t),
	.binops = long_binops,
	.unops = long_unops,
	.get_string = long_get_string,
};

BINOP(ulong, shl,  uint64_t, <<)
BINOP(ulong, shr,  uint64_t, >>)
BINOP(ulong, add,  uint64_t, +)
BINOP(ulong, sub,  uint64_t, -)
BINOP(ulong, mul,  uint64_t, *)
BINOP(ulong, div,  uint64_t, /)
BINOP(ulong, band, uint64_t, &)
BINOP(ulong, bor,  uint64_t, |)
BINOP(ulong, xor,  uint64_t, ^)
BINOP(ulong, rem,  uint64_t, %)

static void
ulong_cast_int (const exprval_t *val1, const exprval_t *src, exprval_t *result,
			   exprctx_t *ctx)
{
	*(uint64_t *) result->value = *(int *) src->value;
}

UNOP(ulong, pos,  uint64_t, +)
UNOP(ulong, neg,  uint64_t, -)
UNOP(ulong, tnot, uint64_t, !)
UNOP(ulong, bnot, uint64_t, ~)

static const char *
ulong_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%"PRIu64, *(uint64_t *) val->value);
}

binop_t ulong_binops[] = {
	{ SHL, &cexpr_ulong, &cexpr_ulong, ulong_shl },
	{ SHR, &cexpr_ulong, &cexpr_ulong, ulong_shr },
	{ '+', &cexpr_ulong, &cexpr_ulong, ulong_add },
	{ '-', &cexpr_ulong, &cexpr_ulong, ulong_sub },
	{ '*', &cexpr_ulong, &cexpr_ulong, ulong_mul },
	{ '/', &cexpr_ulong, &cexpr_ulong, ulong_div },
	{ '&', &cexpr_ulong, &cexpr_ulong, ulong_band },
	{ '|', &cexpr_ulong, &cexpr_ulong, ulong_bor },
	{ '^', &cexpr_ulong, &cexpr_ulong, ulong_xor },
	{ '%', &cexpr_ulong, &cexpr_ulong, ulong_rem },
	{ MOD, &cexpr_ulong, &cexpr_ulong, ulong_rem },
	{ '=', &cexpr_int,  &cexpr_ulong, ulong_cast_int },
	{ '=', &cexpr_plitem, &cexpr_ulong, cexpr_cast_plitem },
	{}
};

unop_t ulong_unops[] = {
	{ '+', &cexpr_ulong, ulong_pos },
	{ '-', &cexpr_ulong, ulong_neg },
	{ '!', &cexpr_ulong, ulong_tnot },
	{ '~', &cexpr_ulong, ulong_bnot },
	{}
};

exprtype_t cexpr_ulong = {
	.name = "ulong",
	.size = sizeof (uint64_t),
	.binops = ulong_binops,
	.unops = ulong_unops,
	.get_string = ulong_get_string,
};

BINOP(size_t, shl,  size_t, <<)
BINOP(size_t, shr,  size_t, >>)
BINOP(size_t, add,  size_t, +)
BINOP(size_t, sub,  size_t, -)
BINOP(size_t, mul,  size_t, *)
BINOP(size_t, div,  size_t, /)
BINOP(size_t, band, size_t, &)
BINOP(size_t, bor,  size_t, |)
BINOP(size_t, xor,  size_t, ^)
BINOP(size_t, rem,  size_t, %)

static void
size_t_cast_int (const exprval_t *val1, const exprval_t *src,
				 exprval_t *result, exprctx_t *ctx)
{
	int         val = *(int *) src->value;
	if (val < 0) {
		PL_Message (ctx->messages, ctx->item, "int value clamped to 0: %d",
					val);
		val = 0;
	}
	*(size_t *) result->value = val;
}

static void
size_t_cast_uint (const exprval_t *val1, const exprval_t *src,
				  exprval_t *result, exprctx_t *ctx)
{
	*(size_t *) result->value = *(unsigned *) src->value;
}

UNOP(size_t, pos,  size_t, +)
UNOP(size_t, neg,  size_t, -)
UNOP(size_t, tnot, size_t, !)
UNOP(size_t, bnot, size_t, ~)

static const char *
size_t_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%zd", *(size_t *) val->value);
}

binop_t size_t_binops[] = {
	{ SHL, &cexpr_size_t, &cexpr_size_t, size_t_shl },
	{ SHR, &cexpr_size_t, &cexpr_size_t, size_t_shr },
	{ '+', &cexpr_size_t, &cexpr_size_t, size_t_add },
	{ '-', &cexpr_size_t, &cexpr_size_t, size_t_sub },
	{ '*', &cexpr_size_t, &cexpr_size_t, size_t_mul },
	{ '/', &cexpr_size_t, &cexpr_size_t, size_t_div },
	{ '&', &cexpr_size_t, &cexpr_size_t, size_t_band },
	{ '|', &cexpr_size_t, &cexpr_size_t, size_t_bor },
	{ '^', &cexpr_size_t, &cexpr_size_t, size_t_xor },
	{ '%', &cexpr_size_t, &cexpr_size_t, size_t_rem },
	{ MOD, &cexpr_size_t, &cexpr_size_t, size_t_rem },
	{ '=', &cexpr_int,    &cexpr_size_t, size_t_cast_int },
	{ '=', &cexpr_uint,   &cexpr_size_t, size_t_cast_uint },
	{ '=', &cexpr_plitem, &cexpr_size_t, cexpr_cast_plitem },
	{}
};

unop_t size_t_unops[] = {
	{ '+', &cexpr_size_t, size_t_pos },
	{ '-', &cexpr_size_t, size_t_neg },
	{ '!', &cexpr_size_t, size_t_tnot },
	{ '~', &cexpr_size_t, size_t_bnot },
	{}
};

exprtype_t cexpr_size_t = {
	.name = "size_t",
	.size = sizeof (size_t),
	.binops = size_t_binops,
	.unops = size_t_unops,
	.get_string = size_t_get_string,
};

BINOP(float, add, float, +)
BINOP(float, sub, float, -)
BINOP(float, mul, float, *)
BINOP(float, div, float, /)

static void
float_rem (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
		   exprctx_t *ctx)
{
	float       a = *(float *) val1->value;
	float       b = *(float *) val2->value;
	*(float *) result->value = a - b * truncf (a / b);
}

static void
float_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
		   exprctx_t *ctx)
{
	// implement true modulo for floats:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	float       a = *(float *) val1->value;
	float       b = *(float *) val2->value;
	*(float *) result->value = a - b * floorf (a / b);
}

static void
float_mul_vec4f (const exprval_t *val1, const exprval_t *val2,
				 exprval_t *result, exprctx_t *ctx)
{
	float       s = *(float *) val1->value;
	__auto_type v = (vec4f_t *) val2->value;
	__auto_type r = (vec4f_t *) result->value;
	*r = s * *v;
}

static void
float_div_quat (const exprval_t *val1, const exprval_t *val2,
				exprval_t *result, exprctx_t *ctx)
{
	float       a = *(float *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = a * qconjf (b) / dotf (b, b);
}

CASTOP (float, int)
CASTOP (float, uint)
CASTOP (float, double)

UNOP(float, pos, float, +)
UNOP(float, neg, float, -)
UNOP(float, tnot, float, !)

static const char *
float_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%.9g", *(float *) val->value);
}

binop_t float_binops[] = {
	{ '+', &cexpr_float, &cexpr_float, float_add },
	{ '-', &cexpr_float, &cexpr_float, float_sub },
	{ '*', &cexpr_float, &cexpr_float, float_mul },
	{ '*', &cexpr_vector, &cexpr_vector, float_mul_vec4f },
	{ '*', &cexpr_quaternion, &cexpr_quaternion, float_mul_vec4f },
	{ '/', &cexpr_float, &cexpr_float, float_div },
	{ '/', &cexpr_quaternion, &cexpr_quaternion, float_div_quat },
	{ '%', &cexpr_float, &cexpr_float, float_rem },
	{ MOD, &cexpr_float, &cexpr_float, float_mod },
	{ '=', &cexpr_int,  &cexpr_float, float_cast_int },
	{ '=', &cexpr_uint,  &cexpr_float, float_cast_uint },
	{ '=', &cexpr_double, &cexpr_float, float_cast_double },
	{ '=', &cexpr_plitem, &cexpr_float, cexpr_cast_plitem },
	{}
};

unop_t float_unops[] = {
	{ '+', &cexpr_float, float_pos },
	{ '-', &cexpr_float, float_neg },
	{ '!', &cexpr_float, float_tnot },
	{}
};

exprtype_t cexpr_float = {
	.name = "float",
	.size = sizeof (float),
	.binops = float_binops,
	.unops = float_unops,
	.get_string = float_get_string,
};

BINOP(double, add, double, +)
BINOP(double, sub, double, -)
BINOP(double, mul, double, *)
BINOP(double, div, double, /)

static void
double_rem (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
			exprctx_t *ctx)
{
	double      a = *(double *) val1->value;
	double      b = *(double *) val2->value;
	*(double *) result->value = a - b * trunc (a / b);
}

static void
double_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
			exprctx_t *ctx)
{
	// implement true modulo for doubles:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	double      a = *(double *) val1->value;
	double      b = *(double *) val2->value;
	*(double *) result->value = a - b * floor (a / b);
}

CASTOP (double, int)
CASTOP (double, uint)
CASTOP (double, float)

UNOP(double, pos, double, +)
UNOP(double, neg, double, -)
UNOP(double, tnot, double, !)

static const char *
double_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	return vac (va_ctx, "%.17g", *(double *) val->value);
}

binop_t double_binops[] = {
	{ '+', &cexpr_double, &cexpr_double, double_add },
	{ '-', &cexpr_double, &cexpr_double, double_sub },
	{ '*', &cexpr_double, &cexpr_double, double_mul },
	{ '/', &cexpr_double, &cexpr_double, double_div },
	{ '%', &cexpr_double, &cexpr_double, double_rem },
	{ MOD, &cexpr_double, &cexpr_double, double_mod },
	{ '=', &cexpr_int,    &cexpr_double, double_cast_int },
	{ '=', &cexpr_uint,   &cexpr_double, double_cast_uint },
	{ '=', &cexpr_uint,   &cexpr_double, double_cast_float },
	{ '=', &cexpr_plitem, &cexpr_double, cexpr_cast_plitem },
	{}
};

unop_t double_unops[] = {
	{ '+', &cexpr_double, double_pos },
	{ '-', &cexpr_double, double_neg },
	{ '!', &cexpr_double, double_tnot },
	{}
};

exprtype_t cexpr_double = {
	.name = "double",
	.size = sizeof (double),
	.binops = double_binops,
	.unops = double_unops,
	.get_string = double_get_string,
};

BINOP(vector, add, vec4f_t, +)
BINOP(vector, sub, vec4f_t, -)
BINOP(vector, mul, vec4f_t, *)
BINOP(vector, div, vec4f_t, /)

static void
vector_quaternion_mul (const exprval_t *val1, const exprval_t *val2,
					   exprval_t *result, exprctx_t *ctx)
{
	vec4f_t     a = *(vec4f_t *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = vqmulf (a, b);
}

static void
vector_rem (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
			exprctx_t *ctx)
{
	vec4f_t     a = *(vec4f_t *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = a - b * vtrunc4f (a / b);
}

static void
vector_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
			exprctx_t *ctx)
{
	// implement true modulo for doubles:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	vec4f_t     a = *(vec4f_t *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = a - b * vfloor4f (a / b);
}

static void
vector_swizzle (const exprval_t *val1, const exprval_t *val2,
				exprval_t *result, exprctx_t *ctx)
{
#define m(x) (1 << ((x) - 'a'))
#define v(x, mask) (((x) & 0x60) == 0x60 && (m(x) & (mask)))
#define vind(x) ((x) & 3)
#define cind(x) (-(((x) >> 3) ^ (x)) & 3)
	const int   color = m('r') | m('g') | m('b') | m('a');
	const int   vector = m('x') | m('y') | m('z') | m('w');
	float      *vec = val1->value;
	const char *name = val2->value;
	exprval_t  *val = 0;

	if (v (name[0], vector) && v (name[1], vector)
		&& v (name[2], vector) && v (name[3], vector) && !name[4]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[vind (name[0])];
		res[1] = vec[vind (name[1])];
		res[2] = vec[vind (name[2])];
		res[3] = vec[vind (name[3])];
	} else if (v (name[0], color) && v (name[1], color)
			   && v (name[2], color) && v (name[3], color) && !name[4]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[cind (name[0])];
		res[1] = vec[cind (name[1])];
		res[2] = vec[cind (name[2])];
		res[3] = vec[cind (name[3])];
	} else if (v (name[0], vector) && v (name[1], vector)
			   && v (name[2], vector) && !name[3]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[vind (name[0])];
		res[1] = vec[vind (name[1])];
		res[2] = vec[vind (name[2])];
		res[3] = 0;
	} else if (v (name[0], color) && v (name[1], color)
			   && v (name[2], color) && !name[3]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[cind (name[0])];
		res[1] = vec[cind (name[1])];
		res[2] = vec[cind (name[2])];
		res[3] = 0;
	} else if (v (name[0], vector) && v (name[1], vector) && !name[2]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[vind (name[0])];
		res[1] = vec[vind (name[1])];
		res[2] = 0;
		res[3] = 0;
	} else if (v (name[0], color) && v (name[1], color) && !name[2]) {
		val = cexpr_value (&cexpr_vector, ctx);
		float      *res = val->value;
		res[0] = vec[cind (name[0])];
		res[1] = vec[cind (name[1])];
		res[2] = 0;
		res[3] = 0;
	} else if (v (name[0], vector) && !name[1]) {
		val = cexpr_value (&cexpr_float, ctx);
		float      *res = val->value;
		res[0] = vec[vind (name[0])];
	} else if (v (name[0], color) && !name[1]) {
		val = cexpr_value (&cexpr_float, ctx);
		float      *res = val->value;
		res[0] = vec[cind (name[0])];
	}
	*(exprval_t **) result->value = val;
}

UNOP(vector, pos, vec4f_t, +)
UNOP(vector, neg, vec4f_t, -)

static const char *
vector_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	vec4f_t     vec = *(vec4f_t *) val->value;
	return vac (va_ctx, VEC4F_FMT, VEC4_EXP (vec));
}

static void
vector_tnot (const exprval_t *val, exprval_t *result, exprctx_t *ctx)
{
	vec4f_t     v = *(vec4f_t *) val->value;
	__auto_type c = (vec4i_t *) result->value;
	*c = v != 0;
}

binop_t vector_binops[] = {
	{ '+', &cexpr_vector, &cexpr_vector, vector_add },
	{ '-', &cexpr_vector, &cexpr_vector, vector_sub },
	{ '*', &cexpr_vector, &cexpr_vector, vector_mul },
	{ '*', &cexpr_quaternion, &cexpr_vector, vector_quaternion_mul },
	{ '/', &cexpr_vector, &cexpr_vector, vector_div },
	{ '%', &cexpr_vector, &cexpr_vector, vector_rem },
	{ MOD, &cexpr_vector, &cexpr_vector, vector_mod },
	{ '.', &cexpr_field, &cexpr_exprval, vector_swizzle },
	{ '=', &cexpr_plitem, &cexpr_vector, cexpr_cast_plitem },
	{}
};

unop_t vector_unops[] = {
	{ '+', &cexpr_vector, vector_pos },
	{ '-', &cexpr_vector, vector_neg },
	{ '!', &cexpr_vector, vector_tnot },
	{}
};

exprtype_t cexpr_vector = {
	.name = "vector",
	.size = sizeof (vec4f_t),
	.binops = vector_binops,
	.unops = vector_unops,
	.get_string = vector_get_string,
};

static void
quaternion_mul (const exprval_t *val1, const exprval_t *val2,
				exprval_t *result, exprctx_t *ctx)
{
	vec4f_t     a = *(vec4f_t *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = qmulf (a, b);
}

static void
quaternion_vector_mul (const exprval_t *val1, const exprval_t *val2,
					   exprval_t *result, exprctx_t *ctx)
{
	vec4f_t     a = *(vec4f_t *) val1->value;
	vec4f_t     b = *(vec4f_t *) val2->value;
	__auto_type c = (vec4f_t *) result->value;
	*c = qvmulf (a, b);
}

static const char *
quaternion_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	vec4f_t     vec = *(vec4f_t *) val->value;
	return vac (va_ctx, VEC4F_FMT, VEC4_EXP (vec));
}

binop_t quaternion_binops[] = {
	{ '+', &cexpr_quaternion, &cexpr_quaternion, vector_add },
	{ '-', &cexpr_quaternion, &cexpr_quaternion, vector_sub },
	{ '*', &cexpr_quaternion, &cexpr_quaternion, quaternion_mul },
	{ '*', &cexpr_vector, &cexpr_vector, quaternion_vector_mul },
	{}
};

unop_t quaternion_unops[] = {
	{ '+', &cexpr_vector, vector_pos },
	{ '-', &cexpr_vector, vector_neg },
	{ '!', &cexpr_vector, vector_tnot },
	{}
};

exprtype_t cexpr_quaternion = {
	.name = "quaterion",
	.size = sizeof (vec4f_t),
	.binops = quaternion_binops,
	.unops = quaternion_unops,
	.get_string = quaternion_get_string,
};

exprtype_t cexpr_exprval = {
	.name = "exprval",
	.size = sizeof (exprval_t *),
	.binops = 0,	// can't actually do anything with an exprval
	.unops = 0,
};

exprtype_t cexpr_field = {
	.name = "field",
	.size = 0,	// has no size of its own, rather, it's the length of the name
	.binops = 0,	// can't actually do anything with a field
	.unops = 0,
};

exprtype_t cexpr_function = {
	.name = "function",
	.size = 0,	// has no size of its own
	.binops = 0,// can't actually do anything with a function other than call
	.unops = 0,
};

void
cexpr_cast_plitem (const exprval_t *val1, const exprval_t *src,
				   exprval_t *result, exprctx_t *ctx)
{
	plitem_t   *item = *(plitem_t **) src->value;
	const char *str = PL_String (item);
	if (!str) {
		cexpr_error (ctx, "not a string object: %d", PL_Line (item));
		return;
	}

	exprctx_t   ectx = *ctx;
	ectx.result = result;
	cexpr_eval_string (str, &ectx);
	ctx->errors += ectx.errors;
	if (ectx.errors) {
		cexpr_error (ctx, "could not convert: %d", PL_Line (item));
	}
}

static void
plitem_field (const exprval_t *a, const exprval_t *b, exprval_t *c,
			  exprctx_t *ctx)
{
	__auto_type dict = *(plitem_t **) a->value;
	__auto_type key = (const char *) b->value;

	if (PL_Type (dict) != QFDictionary) {
		cexpr_error(ctx, "not a dictionary object");
		return;
	}
	plitem_t   *item = PL_ObjectForKey (dict, key);
	exprval_t *val = 0;
	if (!item) {
		cexpr_error (ctx, "key not found: %s", key);
	} else {
		val = cexpr_value (&cexpr_plitem, ctx);
		*(plitem_t **) val->value = item;
	}
	*(exprval_t **) c->value = val;
}

static void
plitem_index (const exprval_t *a, int index, exprval_t *c,
			  exprctx_t *ctx)
{
	__auto_type array = *(plitem_t **) a->value;

	if (PL_Type (array) != QFArray) {
		cexpr_error(ctx, "not an array object");
		return;
	}
	plitem_t   *item = PL_ObjectAtIndex (array, index);
	exprval_t  *val = 0;
	if (!item) {
		cexpr_error (ctx, "invalid index: %d", index);
	} else {
		val = cexpr_value (&cexpr_plitem, ctx);
		*(plitem_t **) val->value = item;
	}
	*(exprval_t **) c->value = val;
}

static void
plitem_int (const exprval_t *a, const exprval_t *b, exprval_t *c,
			exprctx_t *ctx)
{
	int         index = *(int *) b->value;
	plitem_index (a, index, c, ctx);
}

static void
plitem_uint (const exprval_t *a, const exprval_t *b, exprval_t *c,
			 exprctx_t *ctx)
{
	int         index = *(unsigned *) b->value;
	plitem_index (a, index, c, ctx);
}

static void
plitem_size_t (const exprval_t *a, const exprval_t *b, exprval_t *c,
			   exprctx_t *ctx)
{
	int         index = *(size_t *) b->value;
	plitem_index (a, index, c, ctx);
}

binop_t plitem_binops[] = {
	{ '.', &cexpr_field, &cexpr_plitem, plitem_field },
	{ '[', &cexpr_int, &cexpr_plitem, plitem_int },
	{ '[', &cexpr_uint, &cexpr_plitem, plitem_uint },
	{ '[', &cexpr_size_t, &cexpr_plitem, plitem_size_t },
	{}
};

exprtype_t cexpr_plitem = {
	.name = "plitem",
	.size = sizeof (plitem_t *),
	.binops = plitem_binops,
	.unops = 0,
};

exprtype_t cexpr_string = {
	.name = "string",
	.size = sizeof (char *),
};

exprtype_t cexpr_voidptr = {
	.name = "voidptr",
	.size = sizeof (void *),
};

VISIBLE binop_t *
cexpr_find_cast (exprtype_t *dst_type, exprtype_t *src_type)
{
	binop_t    *binop = 0;

	for (binop = dst_type->binops; binop && binop->op; binop++) {
		if (binop->op == '=' && binop->other == src_type) {
			break;
		}
	}
	if (binop && binop->op) {
		return binop;
	}
	return 0;
}

VISIBLE int
cexpr_parse_enum (exprenum_t *enm, const char *str, const exprctx_t *ctx,
				  void *data)
{
	exprval_t   result = { enm->type, data };
	exprctx_t   context = *ctx;
	context.symtab = enm->symtab;
	context.result = &result;
	return cexpr_eval_string (str, &context);
}

VISIBLE const char *
cexpr_enum_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	exprenum_t *enm = val->type->data;
	exprsym_t  *symbols = enm->symtab->symbols;
	for (exprsym_t *sym = symbols; sym->name; sym++) {
		// if there are duplicate values, choose the *later* value
		if (sym[1].name
			&& memcmp (sym->value, sym[1].value, val->type->size) == 0) {
			continue;
		}
		if (memcmp (sym->value, val->value, val->type->size) == 0) {
			return sym->name;
		}
	}
	return "";
}

BINOP(flag, and, int, &)
BINOP(flag, or, int, |)
BINOP(flag, xor, int, ^)

UNOP(flag, not, int, ~)

binop_t cexpr_flag_binops[] = {
	{ '&', 0, 0, flag_and },
	{ '|', 0, 0, flag_or },
	{ '^', 0, 0, flag_xor },
	{ '=', &cexpr_int, 0, uint_cast_int },
	{}
};

unop_t cexpr_flag_unops[] = {
	{ '~', 0, flag_not },
	{}
};

VISIBLE const char *
cexpr_flags_get_string (const exprval_t *val, va_ctx_t *va_ctx)
{
	exprenum_t *enm = val->type->data;
	exprsym_t  *symbols = enm->symtab->symbols;
	const char *val_str = 0;

	if (val->type->size != 4) {
		Sys_Error ("cexpr_flags_get_string: only 32-bit values supported");
	}
	uint32_t    flags = *(uint32_t *) val->value;
	for (exprsym_t *sym = symbols; sym->name; sym++) {
		uint32_t    sym_flags = *(uint32_t *) sym->value;
		// if there are duplicate values, choose the *later* value
		if (sym[1].name && sym_flags == *(uint32_t *) sym[1].value) {
			continue;
		}
		if ((flags & sym_flags) && !(sym_flags & ~flags)) {
			if (val_str) {
				val_str = vac (va_ctx, "%s | %s", val_str, sym->name);
			} else {
				val_str = sym->name;
			}
		}
	}
	if (!val_str) {
		val_str = "0";
	}
	return val_str;
}
