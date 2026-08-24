/*
	expr_bool.c

	short-circuit boolean expressions

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
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static int __attribute__((pure))
has_block_expr (const expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->alias.expr;
	}
	return e->type == ex_block;
}

const expr_t *
test_expr (const expr_t *e)
{
	if (is_error (e)) {
		return e;
	}
	if (e->type == ex_bool) {
		return e;
	}
	if (e->type == ex_assign) {
		if (!e->paren && options.warnings.precedence)
			warning (e, "suggest parentheses around assignment "
					 "used as truth value");
		auto tst = e->assign.src;
		if (has_block_expr (tst) && has_block_expr (e->assign.dst)) {
			tst = new_temp_def_expr (get_type (tst));
			e = new_assign_expr (e->assign.dst,
								 assign_expr (tst, e->assign.src));
		} else if (has_block_expr (tst)) {
			tst = e->assign.dst;
		}
		tst = test_expr (tst);
		auto block = new_block_expr (nullptr);
		append_expr (block, e);
		//append_expr (block, tst);
		block->block.result = tst;
		return block;
	}
	auto type = get_type (e);
	if (!type) {
		// an error occured while getting the type and was already reported
		return new_error_expr ();
	}
	if (is_void (type)) {
		if (options.traditional) {
			if (options.warnings.traditional) {
				warning (e, "void has no value");
			}
			return e;
		}
		return error (e, "void has no value");
	}
	if (is_reference (type)) {
		e = pointer_deref (e);
	}
	e = current_target.test_expr (e);
	fold_constants (e);
	return edag_add_expr (e);
}

static void
bool_check_precedence (const expr_t *e)
{
	if (e->type == ex_bool && e->boolean.e->type == ex_expr &&
		e->boolean.e->expr.op == QC_AND && !e->paren) {
		warning (e, "suggest parentheses around '&&' within '||'");
	}
}

static bool
bool_compat (const expr_t *e, int op)
{
	if (e->type == ex_bool && e->boolean.e->expr.op == op) {
		return !e->boolean.not;
	}
	return false;
}

const expr_t *
bool_expr (int op, const expr_t *e1, const expr_t *e2)
{
	if (!options.code.short_circuit) {
		return binary_expr (op, e1, e2);
	}

	e1 = convert_bool (e1, false);
	if (e1->type == ex_error) {
		return e1;
	}

	e2 = convert_bool (e2, false);
	if (e2->type == ex_error) {
		return e2;
	}

	if (op == QC_OR) {
		bool_check_precedence (e1);
		bool_check_precedence (e2);
	}
	if (bool_compat (e1, op)) {
		e1 = e1->boolean.e;
	}
	if (bool_compat (e2, op)) {
		e2 = e2->boolean.e;
	}
	auto type = base_type (get_type (e1));
	auto e = typed_binary_expr (type, op, e1, e2);
	return new_boolean_expr (false, e);
}

static bool
bool_is_bool (const expr_t *e, int op)
{
	if (e->type == ex_expr && is_logic (e->expr.op)) {
		if (e->expr.op != op) {
			internal_error (e, "mixed bool operands: %s in %s tree",
							get_op_string (e->expr.op), get_op_string (op));
		}
		return true;
	}
	return false;
}

void
flatten_bool (expr_t *block, const expr_t *e, int op)
{
	if (bool_is_bool (e, op)) {
		flatten_bool (block, e->expr.e1, op);
		flatten_bool (block, e->expr.e2, op);
		return;
	}
	append_expr (block, e);
}

static void
bool_block (expr_t *block, const expr_t *e, int op,
			const expr_t *true_label, const expr_t *false_label)
{
	if (is_constant (e)) {
		int         val;

		if (is_integral_val (e)) {
			val = expr_integral (e);
		} else if (is_floating_val (e)) {
			val = expr_floating (e) != 0;
		} else {
			internal_error (e, "unexpedted bool value");
		}
		auto b = goto_expr (val ? true_label : false_label);
		append_expr (block, b);
	} else {
		auto b = branch_expr (op == QC_OR ? QC_NE : QC_EQ, e,
							  op == QC_OR ? true_label : false_label);
		append_expr (block, b);
	}
}

void
make_bool (expr_t *if_block, const expr_t *e,
		   const expr_t *true_label, const expr_t *false_label)
{
	if (e->type != ex_bool) {
		bool_block (if_block, e, QC_OR, true_label, false_label);
		auto g = goto_expr (false_label);
		append_expr (if_block, g);
		return;
	}
	auto block = new_block_expr (nullptr);
	int op = e->boolean.e->expr.op;
	bool inv = (op == QC_OR) ^ e->boolean.not;
	flatten_bool (block, e->boolean.e, op);
	int count = list_count (&block->block.list);
	if (!count) {
		internal_error (e, "empty boolean expression");
	}

	const expr_t *tests[count];
	list_scatter (&block->block.list, tests);

	for (int i = 0; i < count; i++) {
		if (tests[i]->type == ex_bool) {
			auto cont = new_label_expr ();
			auto tl = inv ? true_label : cont;
			auto fl = inv ? cont : false_label;
			make_bool (if_block, tests[i], tl, fl);
			append_expr (if_block, cont);
		} else {
			bool_block (if_block, tests[i], op, true_label, false_label);
		}
	}
	auto g = goto_expr (inv ? false_label : true_label);
	append_expr (if_block, g);
}

const expr_t *
convert_bool (const expr_t *e, bool block)
{
	if (e->type == ex_uexpr && e->expr.op == '!'
		&& !is_string(get_type (e->expr.e1))) {
		e = convert_bool (e->expr.e1, false);
		if (e->type == ex_error)
			return e;
		e = unary_expr ('!', e);
	}
	auto type = get_type (e);
	if (is_compare (e) && is_scalar (type) && type_size (type) == 1) {
		return e;
	}
	if (e->type != ex_bool) {
		e = test_expr (e);
		if (e->type == ex_error) {
			return e;
		}
	}
	return edag_add_expr (e);
}

void
build_bool_block (expr_t *block, const expr_t *e)
{
	switch (e->type) {
		case ex_bool:
			build_bool_block (block, e->boolean.e);
			return;
		case ex_label:
			append_expr (block, e);
			return;
		case ex_assign:
			append_expr (block, e);
			return;
		case ex_branch:
			append_expr (block, e);
			return;
		case ex_expr:
			if (e->expr.op == QC_OR || e->expr.op == QC_AND) {
				build_bool_block (block, e->expr.e1);
				build_bool_block (block, e->expr.e2);
			} else {
				append_expr (block, e);
			}
			return;
		case ex_uexpr:
			break;
		case ex_block:
			if (!e->block.result) {
				for (auto t = e->block.list.head; t; t = t->next) {
					build_bool_block (block, t->expr);
				}
				return;
			}
			break;
		default:
			;
	}
	internal_error (e, "bad boolean");
}
