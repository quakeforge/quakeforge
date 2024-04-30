/*
	expr_dag.c

	Expression dag support code

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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

#include <ctype.h>
#include <string.h>

#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/va.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

typedef struct DARRAY_TYPE (const expr_t *) exprset_t;

static exprset_t expr_dag = DARRAY_STATIC_INIT(32);

void
edag_flush (void)
{
	expr_dag.size = 0;
}

const expr_t *
edag_add_expr (const expr_t *expr)
{
	if (!expr || expr->nodag) {
		return expr;
	}
	for (size_t i = 0; i < expr_dag.size; i++) {
		auto e = expr_dag.a[i];
		if (e->type != expr->type) {
			continue;
		}
		switch (expr->type) {
			case ex_count:
				internal_error (expr, "invalid expression type");
			case ex_label:
			case ex_labelref:
			case ex_error:
			case ex_state:
			case ex_bool:
			case ex_compound:
			case ex_memset:
			case ex_branch:
			case ex_return:
			case ex_adjstk:
			case ex_with:
			case ex_args:
			case ex_type:
				// these are never put in the dag
				return expr;
			case ex_list:
			case ex_block:
				if (has_function_call (expr)) {
					// FIXME const functions can be dagged (and pure if known to be safe)
					return expr;
				}
				return expr;	//FIXME
			case ex_expr:
				if (e->expr.type == expr->expr.type
					&& e->expr.op == expr->expr.op
					&& e->expr.commutative == expr->expr.commutative
					&& ((e->expr.e1 == expr->expr.e1
						 && e->expr.e2 == expr->expr.e2)
						|| (e->expr.commutative
							&& e->expr.e1 == expr->expr.e2
							&& e->expr.e2 == expr->expr.e1))) {
					return e;
				}
				break;
			case ex_uexpr:
				if (e->expr.type == expr->expr.type
					&& e->expr.op == expr->expr.op
					&& e->expr.e1 == expr->expr.e1) {
					return e;
				}
				break;
			case ex_def:
				if (e->def == expr->def) {
					return e;
				}
				break;
			case ex_symbol:
				if (e->symbol == expr->symbol) {
					return e;
				}
				break;
			case ex_temp:
				return expr; //FIXME
			case ex_vector:
				return expr; //FIXME
			case ex_selector:
				if (e->selector.sel_ref == expr->selector.sel_ref
					&& e->selector.sel == expr->selector.sel) {
					return e;
				}
				break;
			case ex_nil:
				if (e->nil == expr->nil) {
					return e;
				}
				break;
			case ex_value:
				if (e->value == expr->value) {
					return e;
				}
				break;
			case ex_alias:
				if (e->alias.type == expr->alias.type
					&& e->alias.expr == expr->alias.expr
					&& e->alias.offset == expr->alias.offset) {
					return e;
				}
				break;
			case ex_address:
				if (e->address.type == expr->address.type
					&& e->address.lvalue == expr->address.lvalue
					&& e->address.offset == expr->address.offset) {
					return e;
				}
				break;
			case ex_assign:
				return expr;	// FIXME?
			case ex_horizontal:
				if (e->hop.type == expr->hop.type
					&& e->hop.vec == expr->hop.vec
					&& e->hop.op == expr->hop.op) {
					return e;
				}
				break;
			case ex_swizzle:
				if (e->swizzle.type == expr->swizzle.type
					&& e->swizzle.src == expr->swizzle.src
					&& e->swizzle.source[0] == expr->swizzle.source[0]
					&& e->swizzle.source[1] == expr->swizzle.source[1]
					&& e->swizzle.source[2] == expr->swizzle.source[2]
					&& e->swizzle.source[3] == expr->swizzle.source[3]
					&& e->swizzle.neg == expr->swizzle.neg
					&& e->swizzle.zero == expr->swizzle.zero) {
					return e;
				}
				break;
			case ex_extend:
				if (e->extend.type == expr->extend.type
					&& e->extend.src == expr->extend.src
					&& e->extend.extend == expr->extend.extend
					&& e->extend.reverse == expr->extend.reverse) {
					return e;
				}
				break;
			case ex_multivec:
				return expr;	//FIXME ?
		}
	}
	DARRAY_APPEND (&expr_dag, expr);
	return expr;
}
