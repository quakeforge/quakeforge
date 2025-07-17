/*
	expr_array.c

	array expressions

	Copyright (C) 2001, 2025 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

const expr_t *
array_expr (const expr_t *array, const expr_t *index)
{
	auto array_type = get_type (array);
	auto index_type = get_type (index);
	const type_t *ele_type;
	const expr_t *base;
	const expr_t *ptr;
	int         ind = 0;

	if (array->type == ex_error)
		return array;
	if (index->type == ex_error)
		return index;

	if (!is_pointer (array_type) && !is_array (array_type)
		&& !is_nonscalar (array_type) && !is_matrix (array_type))
		return error (array, "not an array");
	if (!is_integral (index_type))
		return error (index, "invalid array index type");
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_int_val (index))
		ind = expr_int (index);
	if (is_array (array_type)
		&& array_type->array.count
		&& is_constant (index)
		&& (ind < array_type->array.base
			|| ind - array_type->array.base >= array_type->array.count)) {
		return error (index, "array index out of bounds");
	}
	if (is_nonscalar (array_type) && !is_matrix (array_type)
		&& is_constant (index)
		&& (ind < 0 || ind >= array_type->width)) {
		return error (index, "array index out of bounds");
	}
	if (is_matrix (array_type)
		&& is_constant (index)
		&& (ind < 0 || ind >= array_type->columns)) {
		return error (index, "array index out of bounds");
	}
	if (is_array (array_type)) {
		ele_type = dereference_type (array_type);
		base = new_int_expr (array_type->array.base, false);
	} else if (is_pointer (array_type)) {
		ele_type = array_type->fldptr.type;
		base = new_int_expr (0, false);
	} else {
		ele_type = base_type (array_type);
		if (is_matrix (array_type)) {
			ele_type = column_type (array_type);
		}
		if (array->type == ex_uexpr && array->expr.op == '.') {
			auto vec = offset_pointer_expr (array->expr.e1, index);
			vec = cast_expr (pointer_type (ele_type), vec);
			return unary_expr ('.', vec);
		}
		base = new_int_expr (0, false);
	}
	auto scale = new_int_expr (type_size (ele_type), false);
	auto offset = binary_expr ('*', base, scale);
	index = binary_expr ('*', index, scale);
	index = binary_expr ('-', index, offset);
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_int_val (index))
		ind = expr_int (index);
	if (is_array (array_type)) {
		if (array->type == ex_uexpr && array->expr.op == '.') {
			ptr = array->expr.e1;
		} else {
			auto alias = new_offset_alias_expr (ele_type, array, 0);
			ptr = new_address_expr (ele_type, alias, 0);
		}
	} else if (is_nonscalar (array_type) || is_matrix (array_type)) {
		auto alias = new_offset_alias_expr (ele_type, array, 0);
		ptr = new_address_expr (ele_type, alias, 0);
	} else {
		ptr = array;
	}
	ptr = offset_pointer_expr (ptr, index);
	ptr = cast_expr (pointer_type (ele_type), ptr);

	auto e = unary_expr ('.', ptr);
	return e;
}

const expr_t *
array_property (const type_t *type, const attribute_t *attr, rua_ctx_t *ctx)
{
	return error (attr->params, "not implemented");
}
