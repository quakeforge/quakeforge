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

#include "qfcc.h"
#include "class.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "idstuff.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "shared.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"
#include "qc-parse.h"

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
		case ev_uinteger:
		case ev_integer:
		case ev_short:
			if (!is_integer(type_default)) {
				if (is_constant (e)) {
					return cast_expr (type_default, e);
				}
				return new_alias_expr (type_default, e);
			}
			return e;
		case ev_float:
			if (options.code.fast_float
				|| options.code.progsversion == PROG_ID_VERSION) {
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
		case ev_pointer:
			return new_alias_expr (type_default, e);
		case ev_quat:
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
		if (e->type == ex_uexpr && e->e.expr.op == 'g')
			e->e.expr.e1 = label;
		else if (e->type == ex_expr && (e->e.expr.op == 'i'
										|| e->e.expr.op == 'n'))
			e->e.expr.e2 = label;
		else {
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
			backpatch (e1->e.bool.false_list, label);
			return new_bool_expr (merge (e1->e.bool.true_list,
										 e2->e.bool.true_list),
								  e2->e.bool.false_list, block);
			break;
		case AND:
			backpatch (e1->e.bool.true_list, label);
			return new_bool_expr (e2->e.bool.true_list,
								  merge (e1->e.bool.false_list,
										 e2->e.bool.false_list), block);
			break;
	}
	internal_error (e1, 0);
}

expr_t *
convert_bool (expr_t *e, int block)
{
	expr_t     *b;

	if (e->type == ex_expr && e->e.expr.op == '=') {
		expr_t     *src;
		if (!e->paren && options.warnings.precedence)
			warning (e, "suggest parentheses around assignment "
					 "used as truth value");
		src = e->e.expr.e2;
		if (src->type == ex_block) {
			src = new_temp_def_expr (get_type (src));
			e = new_binary_expr (e->e.expr.op, e->e.expr.e1,
								 assign_expr (src, e->e.expr.e2));
		}
		b = convert_bool (src, 1);
		if (b->type == ex_error)
			return b;
		// insert the assignment into the bool's block
		e->next = b->e.bool.e->e.block.head;
		b->e.bool.e->e.block.head = e;
		if (b->e.bool.e->e.block.tail == &b->e.bool.e->e.block.head) {
			// shouldn't happen, but just in case
			b->e.bool.e->e.block.tail = &e->next;
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
			if (is_integer_val (e)) {
				val = expr_integer (e);
			} else {
				val = expr_float (e) != 0;
			}
			if (val)
				e = new_bool_expr (make_list (b), 0, b);
			else
				e = new_bool_expr (0, make_list (b), b);
		} else {
			b = new_block_expr ();
			append_expr (b, branch_expr ('i', e, 0));
			append_expr (b, goto_expr (0));
			e = new_bool_expr (make_list (b->e.block.head),
							   make_list (b->e.block.head->next), b);
		}
	}
	if (block && e->e.bool.e->type != ex_block) {
		expr_t     *block = new_block_expr ();
		append_expr (block, e->e.bool.e);
		e->e.bool.e = block;
	}
	return e;
}
