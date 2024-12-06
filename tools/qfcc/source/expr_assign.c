/*
	expr_assign.c

	assignment expression construction and manipulations

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

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
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static const expr_t *
check_assign_logic_precedence (const expr_t *dst, const expr_t *src)
{
	if (src->type == ex_expr && !src->paren && is_logic (src->expr.op)) {
		// traditional QuakeC gives = higher precedence than && and ||
		notice (src, "precedence of `=' and `%s' inverted for "
					 "traditional code", get_op_string (src->expr.op));
		// change {a = (b logic c)} to {(a = b) logic c}
		auto assignment = assign_expr (dst, src->expr.e1);
		// protect assignment from binary_expr
		assignment = paren_expr (assignment);
		return binary_expr (src->expr.op, assignment, src->expr.e2);
	}
	return 0;
}

bool
is_lvalue (const expr_t *expr)
{
	switch (expr->type) {
		case ex_def:
			return !expr->def->constant;
		case ex_symbol:
			return expr->symbol->lvalue;
		case ex_temp:
			return true;
		case ex_expr:
			if (expr->expr.op == '.') {
				return true;
			}
			break;
		case ex_alias:
			return is_lvalue (expr->alias.expr);
		case ex_address:
			return false;
		case ex_assign:
			return false;
		case ex_uexpr:
			if (expr->expr.op == '.') {
				return true;
			}
			break;
		case ex_branch:
		case ex_inout:
		case ex_memset:
		case ex_compound:
		case ex_state:
		case ex_bool:
		case ex_label:
		case ex_labelref:
		case ex_block:
		case ex_vector:
		case ex_nil:
		case ex_value:
		case ex_error:
		case ex_selector:
		case ex_message:
		case ex_return:
		case ex_adjstk:
		case ex_with:
		case ex_args:
		case ex_horizontal:
		case ex_swizzle:
		case ex_extend:
		case ex_multivec:
		case ex_list:
		case ex_type:
		case ex_incop:
		case ex_decl:
		case ex_loop:
		case ex_select:
		case ex_intrinsic:
		case ex_switch:
		case ex_caselabel:
			break;
		case ex_cond:
			return (is_lvalue (expr->cond.true_expr)
					&& is_lvalue (expr->cond.false_expr));
		case ex_field:
			return true;
		case ex_array:
			return true;
		case ex_count:
			internal_error (expr, "invalid expression");
	}
	return false;
}

static const expr_t *
check_valid_lvalue (const expr_t *expr)
{
	if (!is_lvalue (expr)) {
		if (options.traditional) {
			warning (expr, "invalid lvalue in assignment");
			return 0;
		}
		return error (expr, "invalid lvalue in assignment");
	}
	return 0;
}

static const expr_t *
check_types_compatible (const expr_t *dst, const expr_t *src)
{
	auto dst_type = get_type (dst);
	auto src_type = get_type (src);

	if (dst_type == src_type) {
		if (is_algebra (dst_type) || is_algebra (src_type)) {
			return algebra_assign_expr (dst, src);
		}
		return 0;
	}

	if (type_assignable (dst_type, src_type)) {
		if (is_algebra (dst_type) || is_algebra (src_type)) {
			return algebra_assign_expr (dst, src);
		}
		debug (dst, "casting %s to %s", src_type->name, dst_type->name);
		if (!src->implicit && !type_promotes (dst_type, src_type)) {
			if (is_double (src_type)) {
				warning (dst, "assignment of %s to %s (use a cast)\n",
						 src_type->name, dst_type->name);
			}
		}
		// the types are different but cast-compatible
		auto new = cast_expr (dst_type, src);
		// the cast was a no-op, so the types are compatible at the
		// low level (very true for default type <-> enum)
		if (new != src) {
			return assign_expr (dst, new);
		}
		return 0;
	}
	// traditional qcc is a little sloppy
	if (!options.traditional) {
		return type_mismatch (dst, src, '=');
	}
	if (is_func (dst_type) && is_func (src_type)) {
		warning (dst, "assignment between disparate function types");
		return 0;
	}
	if (is_float (dst_type) && is_vector (src_type)) {
		warning (dst, "assignment of vector to float");
		src = field_expr (src, new_name_expr ("x"));
		return assign_expr (dst, src);
	}
	if (is_vector (dst_type) && is_float (src_type)) {
		warning (dst, "assignment of float to vector");
		dst = field_expr (dst, new_name_expr ("x"));
		return assign_expr (dst, src);
	}
	return type_mismatch (dst, src, '=');
}

static int __attribute__((pure))
is_memset (const expr_t *e)
{
	return e->type == ex_memset;
}

const expr_t *
assign_expr (const expr_t *dst, const expr_t *src)
{
	const expr_t *expr;
	const type_t *dst_type, *src_type;

	dst = convert_name (dst);
	if (is_error (dst)) {
		return dst;
	}

	const expr_t *err;
	if ((err = check_valid_lvalue (dst))) {
		return err;
	}

	if (is_reference (get_type (dst))) {
		dst = pointer_deref (dst);
	}
	dst_type = get_type (dst);
	if (!dst_type) {
		internal_error (dst, "dst_type broke in assign_expr");
	}

	if (src && !is_memset (src)) {
		src = convert_name (src);
		if (is_error (src)) {
			return src;
		}

		if (options.traditional
			&& (expr = check_assign_logic_precedence (dst, src))) {
			return expr;
		}
	} else {
		if (!src && is_scalar (dst_type)) {
			return error (dst, "empty scalar initializer");
		}
		src = new_nil_expr ();
	}
	if (src->type == ex_compound) {
		src = current_target.initialized_temp (dst_type, src);
		if (is_error (src)) {
			return src;
		}
	}
	if (is_reference (get_type (src))) {
		src = pointer_deref (src);
	}
	src_type = get_type (src);
	if (!src_type) {
		internal_error (src, "src_type broke in assign_expr");
	}

	if (is_pointer (dst_type) && is_array (src_type)) {
		// assigning an array to a pointer is the same as taking the address of
		// the array but using the type of the array elements
		src = address_expr (src, src_type->fldptr.type);
		src_type = get_type (src);
	}
	if (src->type == ex_bool) {
		// boolean expressions are chains of tests, so extract the result
		// of the tests
		src = convert_from_bool (src, dst_type);
		if (is_error (src)) {
			return src;
		}
		src_type = get_type (src);
	}

	if (!is_nil (src)) {
		if ((expr = check_types_compatible (dst, src))) {
			// expr might be a valid expression, but if so,
			// check_types_compatible will take care of everything
			return expr;
		}
		if (src->type == ex_vector) {
			return current_target.assign_vector (dst, src);
		}
	} else {
		src = convert_nil (src, dst_type);
	}

	expr = new_assign_expr (dst, src);
	return expr;
}
