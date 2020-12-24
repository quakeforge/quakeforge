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

#include "QF/cexpr.h"

#include "libs/util/cexpr-parse.h"

#define BINOP(pre, opname, type, op)										\
static void																	\
pre##_##opname (const exprval_t *a, const exprval_t *b, exprval_t *c,		\
				exprctx_t *ctx)												\
{																			\
	(*(type *) c->value) = (*(type *) a->value) op (*(type *) b->value);	\
}

#define UNOP(pre, opname, type, op)									\
static void															\
pre##_##opname (const exprval_t *a, exprval_t *b, exprctx_t *ctx)	\
{																	\
	(*(type *) b->value) = op (*(type *) a->value);					\
}

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
	"int",
	sizeof (int),
	int_binops,
	int_unops,
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
	"uint",
	sizeof (unsigned),
	uint_binops,
	uint_unops,
};

BINOP(size_t, shl, unsigned, <<)
BINOP(size_t, shr, unsigned, >>)
BINOP(size_t, add, unsigned, +)
BINOP(size_t, sub, unsigned, -)
BINOP(size_t, mul, unsigned, *)
BINOP(size_t, div, unsigned, /)
BINOP(size_t, band, unsigned, &)
BINOP(size_t, bor, unsigned, |)
BINOP(size_t, xor, unsigned, ^)
BINOP(size_t, rem, unsigned, %)

UNOP(size_t, pos, unsigned, +)
UNOP(size_t, neg, unsigned, -)
UNOP(size_t, tnot, unsigned, !)
UNOP(size_t, bnot, unsigned, ~)

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
	"size_t",
	sizeof (size_t),
	size_t_binops,
	size_t_unops,
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
	// implement true modulo for integers:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	float       a = *(float *) val1->value;
	float       b = *(float *) val2->value;
	*(float *) result->value = a - b * floorf (a / b);
}

UNOP(float, pos, float, +)
UNOP(float, neg, float, -)
UNOP(float, tnot, float, !)

binop_t float_binops[] = {
	{ '+', &cexpr_float, &cexpr_float, float_add },
	{ '-', &cexpr_float, &cexpr_float, float_sub },
	{ '*', &cexpr_float, &cexpr_float, float_mul },
	{ '/', &cexpr_float, &cexpr_float, float_div },
	{ '%', &cexpr_float, &cexpr_float, float_rem },
	{ MOD, &cexpr_float, &cexpr_float, float_mod },
	{}
};

unop_t float_unops[] = {
	{ '+', &cexpr_float, float_pos },
	{ '-', &cexpr_float, float_neg },
	{ '!', &cexpr_float, float_tnot },
	{}
};

exprtype_t cexpr_float = {
	"float",
	sizeof (float),
	float_binops,
	float_unops,
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
	*(double *) result->value = a - b * truncf (a / b);
}

static void
double_mod (const exprval_t *val1, const exprval_t *val2, exprval_t *result,
			exprctx_t *ctx)
{
	// implement true modulo for integers:
	//  5 mod  3 = 2
	// -5 mod  3 = 1
	//  5 mod -3 = -1
	// -5 mod -3 = -2
	double      a = *(double *) val1->value;
	double      b = *(double *) val2->value;
	*(double *) result->value = a - b * floorf (a / b);
}

UNOP(double, pos, double, +)
UNOP(double, neg, double, -)
UNOP(double, tnot, double, !)

binop_t double_binops[] = {
	{ '+', &cexpr_double, &cexpr_double, double_add },
	{ '-', &cexpr_double, &cexpr_double, double_sub },
	{ '*', &cexpr_double, &cexpr_double, double_mul },
	{ '/', &cexpr_double, &cexpr_double, double_div },
	{ '%', &cexpr_double, &cexpr_double, double_rem },
	{ MOD, &cexpr_double, &cexpr_double, double_mod },
	{}
};

unop_t double_unops[] = {
	{ '+', &cexpr_double, double_pos },
	{ '-', &cexpr_double, double_neg },
	{ '!', &cexpr_double, double_tnot },
	{}
};

exprtype_t cexpr_double = {
	"double",
	sizeof (double),
	double_binops,
	double_unops,
};

exprtype_t cexpr_exprval = {
	"exprval",
	sizeof (exprval_t *),
	0,	// can't actually do anything with an exprval
	0,
};

exprtype_t cexpr_field = {
	"field",
	0,	// has no size of its own, rather, it's the length of the name
	0,	// can't actually do anything with a field
	0,
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
