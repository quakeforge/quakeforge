/*
	expr_cast.c

	expression casting

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/04/27

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

#include <string.h>
#include <stdlib.h>

#include "QF/mathlib.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static expr_t *
cast_error (expr_t *e, type_t *t1, type_t *t2)
{
	e = error (e, "cannot cast from %s to %s", get_type_string (t1),
			   get_type_string (t2));
	return e;
}

static void
do_conversion (pr_type_t *dst_value, type_t *dstType,
			   pr_type_t *src_value, type_t *srcType, expr_t *expr)
{
	int         from = type_cast_map[base_type (srcType)->type];
	int         to = type_cast_map[base_type (dstType)->type];
	int         width = type_width (srcType) - 1;
	int         conversion = TYPE_CAST_CODE (from, to, width);
#define OPA(type) (*((pr_##type##_t *) (src_value)))
#define OPC(type) (*((pr_##type##_t *) (dst_value)))
	switch (conversion) {
#include "libs/gamecode/pr_convert.cinc"
		default:
			internal_error (expr, "invalid conversion code: %04o", conversion);
	}
}

static expr_t *
cast_math (type_t *dstType, type_t *srcType, expr_t *expr)
{
	pr_type_t   src_value[type_size (srcType)];
	pr_type_t   dst_value[type_size (dstType)];

	value_store (src_value, srcType, expr);

	do_conversion (dst_value, dstType, src_value, srcType, expr);

	expr_t     *val = new_expr ();
	val->type = ex_value;
	val->e.value = new_type_value (dstType, dst_value);
	return val;
}

expr_t *
cast_expr (type_t *dstType, expr_t *e)
{
	expr_t    *c;
	type_t    *srcType;

	convert_name (e);

	if (e->type == ex_error)
		return e;

	dstType = (type_t *) unalias_type (dstType); //FIXME cast
	srcType = get_type (e);

	if (dstType == srcType)
		return e;

	if ((dstType == type_default && is_enum (srcType))
		|| (is_enum (dstType) && srcType == type_default))
		return e;
	if ((is_ptr (dstType) && is_string (srcType))
		|| (is_string (dstType) && is_ptr (srcType))) {
		c = new_alias_expr (dstType, e);
		return c;
	}
	if (!(is_ptr (dstType) && (is_ptr (srcType) || is_integral (srcType)
							   || is_array (srcType)))
		&& !(is_integral (dstType) && is_ptr (srcType))
		&& !(is_func (dstType) && is_func (srcType))
		&& !(is_math (dstType) && is_math (srcType)
			 && type_width (dstType) == type_width (srcType))
		&& !((is_int (dstType) || is_uint (dstType))
			 && (is_short (srcType) || is_ushort (srcType))
			 // [u]short is always width 0
			 && type_width (dstType) == 1)) {
		return cast_error (e, srcType, dstType);
	}
	if (is_array (srcType)) {
		return address_expr (e, dstType->t.fldptr.type);
	}
	if (is_short (srcType)) {
		e = new_int_expr (expr_short (e));
		srcType = &type_int;
	} else if (is_ushort (srcType)) {
		e = new_int_expr (expr_ushort (e));
		srcType = &type_int;
	}
	if (is_constant (e) && is_math (dstType) && is_math (srcType)) {
		return cast_math (dstType, srcType, e);
	} else if (is_integral (dstType) && is_integral (srcType)) {
		c = new_alias_expr (dstType, e);
	} else if (is_scalar (dstType) && is_scalar (srcType)) {
		c = new_unary_expr ('C', e);
		c->e.expr.type = dstType;
	} else if (e->type == ex_uexpr && e->e.expr.op == '.') {
		e->e.expr.type = dstType;
		c = e;
	} else {
		c = new_alias_expr (dstType, e);
	}
	return c;
}
