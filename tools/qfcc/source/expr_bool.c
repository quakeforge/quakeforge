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
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

expr_t *
test_expr (expr_t *e)
{
	static float zero[4] = {0, 0, 0, 0};
	expr_t     *new = 0;
	type_t     *type;

	if (e->type == ex_error)
		return e;

	type = get_type (e);
	if (e->type == ex_error)
		return e;
	switch (type->type) {
		case ev_type_count:
			internal_error (e, 0);
		case ev_void:
			if (options.traditional) {
				if (options.warnings.traditional)
					warning (e, "void has no value");
				return e;
			}
			return error (e, "void has no value");
		case ev_string:
			if (!options.code.ifstring)
				return new_alias_expr (type_default, e);
			new = new_string_expr (0);
			break;
		case ev_long:
		case ev_ulong:
		case ev_ushort:
			internal_error (e, "long not implemented");
		case ev_uint:
		case ev_int:
		case ev_short:
			if (!is_int(type_default)) {
				if (is_constant (e)) {
					return cast_expr (type_default, e);
				}
				return new_alias_expr (type_default, e);
			}
			return e;
		case ev_float:
			if (options.code.progsversion < PROG_VERSION
				&& (options.code.fast_float
					|| options.code.progsversion == PROG_ID_VERSION)) {
				if (!is_float(type_default)) {
					if (is_constant (e)) {
						return cast_expr (type_default, e);
					}
					return new_alias_expr (type_default, e);
				}
				return e;
			}
			new = new_float_expr (0);
			break;
		case ev_double:
			new = new_double_expr (0);
			break;
		case ev_vector:
			new = new_vector_expr (zero);
			break;
		case ev_entity:
			return new_alias_expr (type_default, e);
		case ev_field:
			return new_alias_expr (type_default, e);
		case ev_func:
			return new_alias_expr (type_default, e);
		case ev_ptr:
			return new_alias_expr (type_default, e);
		case ev_quaternion:
			new = new_quaternion_expr (zero);
			break;
		case ev_invalid:
			if (is_enum (type)) {
				new = new_nil_expr ();
				break;
			}
			return test_error (e, get_type (e));
	}
	new->line = e->line;
	new->file = e->file;
	new = binary_expr (NE, e, new);
	new->line = e->line;
	new->file = e->file;
	return new;
}

void
backpatch (ex_list_t *list, expr_t *label)
{
	int         i;
	expr_t     *e;

	if (!list)
		return;
	if (!label || label->type != ex_label)
		internal_error (label, "not a label");

	for (i = 0; i < list->size; i++) {
		e = list->e[i];
		if (e->type == ex_branch && e->e.branch.type < pr_branch_call) {
			e->e.branch.target = label;
		} else {
			internal_error (e, 0);
		}
		label->e.label.used++;
	}
}

static ex_list_t *
merge (ex_list_t *l1, ex_list_t *l2)
{
	ex_list_t  *m;

	if (!l1 && !l2)
		internal_error (0, 0);
	if (!l2)
		return l1;
	if (!l1)
		return l2;
	m = malloc ((size_t)&((ex_list_t *)0)->e[l1->size + l2->size]);
	m->size = l1->size + l2->size;
	memcpy (m->e, l1->e, l1->size * sizeof (expr_t *));
	memcpy (m->e + l1->size, l2->e, l2->size * sizeof (expr_t *));
	return m;
}

static ex_list_t *
make_list (expr_t *e)
{
	ex_list_t  *m;

	m = malloc ((size_t)&((ex_list_t *) 0)->e[1]);
	m->size = 1;
	m->e[0] = e;
	return m;
}

expr_t *
bool_expr (int op, expr_t *label, expr_t *e1, expr_t *e2)
{
	expr_t     *block;

	if (!options.code.short_circuit)
		return binary_expr (op, e1, e2);

	e1 = convert_bool (e1, 0);
	if (e1->type == ex_error)
		return e1;

	e2 = convert_bool (e2, 0);
	if (e2->type == ex_error)
		return e2;

	block = new_block_expr ();
	append_expr (block, e1);
	append_expr (block, label);
	append_expr (block, e2);

	switch (op) {
		case OR:
			backpatch (e1->e.boolean.false_list, label);
			return new_bool_expr (merge (e1->e.boolean.true_list,
										 e2->e.boolean.true_list),
								  e2->e.boolean.false_list, block);
			break;
		case AND:
			backpatch (e1->e.boolean.true_list, label);
			return new_bool_expr (e2->e.boolean.true_list,
								  merge (e1->e.boolean.false_list,
										 e2->e.boolean.false_list), block);
			break;
	}
	internal_error (e1, 0);
}

static int __attribute__((pure))
has_block_expr (expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->e.alias.expr;
	}
	return e->type == ex_block;
}

expr_t *
convert_bool (expr_t *e, int block)
{
	expr_t     *b;

	if (e->type == ex_assign) {
		expr_t     *tst;
		if (!e->paren && options.warnings.precedence)
			warning (e, "suggest parentheses around assignment "
					 "used as truth value");
		tst = e->e.assign.src;
		if (has_block_expr (tst) && has_block_expr (e->e.assign.dst)) {
			tst = new_temp_def_expr (get_type (tst));
			e = new_assign_expr (e->e.assign.dst,
								 assign_expr (tst, e->e.assign.src));
		} else if (has_block_expr (tst)) {
			tst = e->e.assign.dst;
		}
		b = convert_bool (tst, 1);
		if (b->type == ex_error)
			return b;
		// insert the assignment into the boolean's block
		e->next = b->e.boolean.e->e.block.head;
		b->e.boolean.e->e.block.head = e;
		if (b->e.boolean.e->e.block.tail == &b->e.boolean.e->e.block.head) {
			// shouldn't happen, but just in case
			b->e.boolean.e->e.block.tail = &e->next;
		}
		return b;
	}

	if (e->type == ex_uexpr && e->e.expr.op == '!'
		&& !is_string(get_type (e->e.expr.e1))) {
		e = convert_bool (e->e.expr.e1, 0);
		if (e->type == ex_error)
			return e;
		e = unary_expr ('!', e);
	}
	if (e->type != ex_bool) {
		e = test_expr (e);
		if (e->type == ex_error)
			return e;
		if (is_constant (e)) {
			int         val;

			b = goto_expr (0);
			if (is_int_val (e)) {
				val = expr_int (e);
			} else {
				val = expr_float (e) != 0;
			}
			if (val)
				e = new_bool_expr (make_list (b), 0, b);
			else
				e = new_bool_expr (0, make_list (b), b);
		} else {
			b = new_block_expr ();
			append_expr (b, branch_expr (NE, e, 0));
			append_expr (b, goto_expr (0));
			e = new_bool_expr (make_list (b->e.block.head),
							   make_list (b->e.block.head->next), b);
		}
	}
	if (block && e->e.boolean.e->type != ex_block) {
		expr_t     *block = new_block_expr ();
		append_expr (block, e->e.boolean.e);
		e->e.boolean.e = block;
	}
	return e;
}
