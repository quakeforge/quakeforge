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
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static expr_t *
check_assign_logic_precedence (expr_t *dst, expr_t *src)
{
	if (src->type == ex_expr && !src->paren && is_logic (src->e.expr.op)) {
		// traditional QuakeC gives = higher precedence than && and ||
		expr_t     *assignment;
		notice (src, "precedence of `=' and `%s' inverted for "
					 "traditional code", get_op_string (src->e.expr.op));
		// change {a = (b logic c)} to {(a = b) logic c}
		assignment = assign_expr (dst, src->e.expr.e1);
		assignment->paren = 1;	// protect assignment from binary_expr
		return binary_expr (src->e.expr.op, assignment, src->e.expr.e2);
	}
	return 0;
}

int
is_lvalue (const expr_t *expr)
{
	switch (expr->type) {
		case ex_def:
			return !expr->e.def->constant;
		case ex_symbol:
			switch (expr->e.symbol->sy_type) {
				case sy_name:
					break;
				case sy_var:
					return 1;
				case sy_const:
					break;
				case sy_type:
					break;
				case sy_expr:
					break;
				case sy_func:
					break;
				case sy_class:
					break;
				case sy_convert:
					break;
			}
			break;
		case ex_temp:
			return 1;
		case ex_expr:
			if (expr->e.expr.op == '.') {
				return 1;
			}
			break;
		case ex_alias:
			return is_lvalue (expr->e.alias.expr);
		case ex_address:
			return 0;
		case ex_assign:
			return 0;
		case ex_uexpr:
			if (expr->e.expr.op == '.') {
				return 1;
			}
			break;
		case ex_branch:
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
		case ex_return:
		case ex_adjstk:
		case ex_with:
		case ex_args:
		case ex_horizontal:
		case ex_swizzle:
		case ex_extend:
		case ex_multivec:
			break;
		case ex_count:
			internal_error (expr, "invalid expression");
	}
	return 0;
}

static expr_t *
check_valid_lvalue (expr_t *expr)
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

static expr_t *
check_types_compatible (expr_t *dst, expr_t *src)
{
	type_t     *dst_type = get_type (dst);
	type_t     *src_type = get_type (src);

	if (dst_type == src_type) {
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
		expr_t     *new = cast_expr (dst_type, src);
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

static void
copy_qv_elements (expr_t *block, expr_t *dst, expr_t *src)
{
	expr_t     *dx, *sx;
	expr_t     *dy, *sy;
	expr_t     *dz, *sz;
	expr_t     *dw, *sw;
	expr_t     *ds, *ss;
	expr_t     *dv, *sv;

	if (is_vector (src->e.vector.type)) {
		// guaranteed to have three elements
		sx = src->e.vector.list;
		sy = sx->next;
		sz = sy->next;
		dx = field_expr (dst, new_name_expr ("x"));
		dy = field_expr (dst, new_name_expr ("y"));
		dz = field_expr (dst, new_name_expr ("z"));
		append_expr (block, assign_expr (dx, sx));
		append_expr (block, assign_expr (dy, sy));
		append_expr (block, assign_expr (dz, sz));
	} else {
		// guaranteed to have two or four elements
		if (src->e.vector.list->next->next) {
			// four vals: x, y, z, w
			sx = src->e.vector.list;
			sy = sx->next;
			sz = sy->next;
			sw = sz->next;
			dx = field_expr (dst, new_name_expr ("x"));
			dy = field_expr (dst, new_name_expr ("y"));
			dz = field_expr (dst, new_name_expr ("z"));
			dw = field_expr (dst, new_name_expr ("w"));
			append_expr (block, assign_expr (dx, sx));
			append_expr (block, assign_expr (dy, sy));
			append_expr (block, assign_expr (dz, sz));
			append_expr (block, assign_expr (dw, sw));
		} else {
			// v, s
			sv = src->e.vector.list;
			ss = sv->next;
			dv = field_expr (dst, new_name_expr ("v"));
			ds = field_expr (dst, new_name_expr ("s"));
			append_expr (block, assign_expr (dv, sv));
			append_expr (block, assign_expr (ds, ss));
		}
	}
}

static int
copy_elements (expr_t *block, expr_t *dst, expr_t *src, int base)
{
	int         index = 0;
	for (expr_t *e = src->e.vector.list; e; e = e->next) {
		if (e->type == ex_vector) {
			index += copy_elements (block, dst, e, index + base);
		} else {
			expr_t     *dst_ele = array_expr (dst, new_int_expr (index + base));
			append_expr (block, assign_expr (dst_ele, e));
			index += type_width (get_type (e));
		}
	}
	return index;
}

static expr_t *
assign_vector_expr (expr_t *dst, expr_t *src)
{
	if (src->type == ex_vector && dst->type != ex_vector) {
		expr_t     *block = new_block_expr ();

		if (options.code.progsversion <= PROG_VERSION) {
			copy_qv_elements (block, dst, src);
		} else {
			copy_elements (block, dst, src, 0);
		}
		block->e.block.result = dst;
		return block;
	}
	return 0;
}

static __attribute__((pure)) int
is_memset (expr_t *e)
{
	return e->type == ex_memset;
}

expr_t *
assign_expr (expr_t *dst, expr_t *src)
{
	expr_t     *expr;
	type_t     *dst_type, *src_type;

	convert_name (dst);
	if (dst->type == ex_error) {
		return dst;
	}
	if ((expr = check_valid_lvalue (dst))) {
		return expr;
	}
	dst_type = get_type (dst);
	if (!dst_type) {
		internal_error (dst, "dst_type broke in assign_expr");
	}

	if (src && !is_memset (src)) {
		convert_name (src);
		if (src->type == ex_error) {
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
		src = initialized_temp_expr (dst_type, src);
		if (src->type == ex_error) {
			return src;
		}
	}
	src_type = get_type (src);
	if (!src_type) {
		internal_error (src, "src_type broke in assign_expr");
	}

	if (is_ptr (dst_type) && is_array (src_type)) {
		// assigning an array to a pointer is the same as taking the address of
		// the array but using the type of the array elements
		src = address_expr (src, src_type->t.fldptr.type);
		src_type = get_type (src);
	}
	if (src->type == ex_bool) {
		// boolean expressions are chains of tests, so extract the result
		// of the tests
		src = convert_from_bool (src, dst_type);
		if (src->type == ex_error) {
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
		if ((expr = assign_vector_expr (dst, src))) {
			return expr;
		}
	} else {
		convert_nil (src, dst_type);
	}

	expr = new_assign_expr (dst, src);
	return expr;
}
