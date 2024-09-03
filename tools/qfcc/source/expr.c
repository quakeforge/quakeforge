/*
	expr.c

	expression construction and manipulations

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
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/algebra.h"
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
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

ALLOC_STATE (expr_t, exprs);
ALLOC_STATE (ex_listitem_t, listitems);

const expr_t *
convert_name (const expr_t *e)
{
	symbol_t   *sym;

	if (!e || e->type != ex_symbol)
		return e;

	sym = e->symbol;

	if (!strcmp (sym->name, "__PRETTY_FUNCTION__")
		&& current_func) {
		return new_string_expr (current_func->name);
	}
	if (!strcmp (sym->name, "__FUNCTION__")
		&& current_func) {
		return new_string_expr (current_func->def->name);
	}
	if (!strcmp (sym->name, "__LINE__")
		&& current_func) {
		return new_int_expr (e->loc.line, false);
	}
	if (!strcmp (sym->name, "__INFINITY__")
		&& current_func) {
		return new_float_expr (INFINITY, false);
	}
	if (!strcmp (sym->name, "__FILE__")
		&& current_func) {
		return new_string_expr (GETSTR (e->loc.file));
	}
	if (!sym->table) {
		e = error (e, "%s undefined", sym->name);
		sym->type = type_default;
		//FIXME need a def
		return e;
	}
	if (sym->sy_type == sy_convert) {
		return sym->convert.conv (sym, sym->convert.data);
	}
	if (sym->sy_type == sy_expr) {
		return sym->expr;
	}
	if (sym->sy_type == sy_type)
		internal_error (e, "unexpected typedef");
	// var, const and func shouldn't need extra handling
	return e;
}

const type_t *
get_type (const expr_t *e)
{
	const type_t *type = 0;
	e = convert_name (e);
	switch (e->type) {
		case ex_inout:
			if (!e->inout.out) {
				internal_error (e, "inout with no out");
			}
			type = get_type (e->inout.out);
			break;
		case ex_branch:
			type = e->branch.ret_type;
			break;
		case ex_labelref:
		case ex_adjstk:
		case ex_with:
			return &type_void;
		case ex_memset:
			return 0;
		case ex_error:
			return 0;
		case ex_return:
			internal_error (e, "unexpected expression type");
		case ex_label:
		case ex_compound:
			return 0;
		case ex_bool:
			if (options.code.progsversion == PROG_ID_VERSION)
				return &type_float;
			return &type_int;
		case ex_nil:
			if (e->nil) {
				return e->nil;
			}
			// fall through
		case ex_state:
			return &type_void;
		case ex_block:
			if (e->block.result)
				return get_type (e->block.result);
			return &type_void;
		case ex_expr:
		case ex_uexpr:
			type = e->expr.type;
			break;
		case ex_def:
			type = e->def->type;
			break;
		case ex_symbol:
			type = e->symbol->type;
			break;
		case ex_temp:
			type = e->temp.type;
			break;
		case ex_value:
			type = e->value->type;
			break;
		case ex_vector:
			return e->vector.type;
		case ex_selector:
			return &type_SEL;
		case ex_alias:
			type = e->alias.type;
			break;
		case ex_address:
			type = e->address.type;
			break;
		case ex_assign:
			return get_type (e->assign.dst);
		case ex_args:
			return &type_va_list;
		case ex_horizontal:
			return e->hop.type;
		case ex_swizzle:
			return e->swizzle.type;
		case ex_extend:
			return e->extend.type;
		case ex_multivec:
			return e->multivec.type;
		case ex_list:
			if (e->list.head) {
				auto last = (ex_listitem_t *) e->list.tail;
				return get_type (last->expr);
			}
			return 0;
		case ex_type:
			return nullptr;
		case ex_count:
			internal_error (e, "invalid expression");
	}
	return unalias_type (type);
}

etype_t
extract_type (const expr_t *e)
{
	auto type = get_type (e);

	if (type)
		return type->type;
	return ev_type_count;
}

const expr_t *
type_mismatch (const expr_t *e1, const expr_t *e2, int op)
{
	if (options.verbosity >= 2) {
		print_expr (e1);
		print_expr (e2);
	}
	return error (e1, "type mismatch: %s %s %s",
				  get_type_string (get_type (e1)), get_op_string (op),
				  get_type_string (get_type (e2)));
}

const expr_t *
param_mismatch (const expr_t *e, int param, const char *fn,
				const type_t *t1, const type_t *t2)
{
	return error (e, "type mismatch for parameter %d of %s: "
				  "expected %s, got %s", param, fn, get_type_string (t1),
				  get_type_string (t2));
}

const expr_t *
test_error (const expr_t *e, const type_t *t)
{
	dstring_t  *s = dstring_newstr ();

	print_type_str (s, t);

	auto err = error (e, "%s cannot be tested", s->str);
	dstring_delete (s);
	return err;
}

expr_t *
new_expr (void)
{
	expr_t     *e;

	ALLOC (16384, expr_t, exprs, e);

	e->loc = pr.loc;
	return e;
}

void
restore_src_loc (expr_t **e)
{
	if (*e) {
		pr.loc = (*e)->loc;

		FREE (exprs, *e);
	}
}

expr_t *
set_src_loc (const expr_t *e)
{
	if (!e) {
		return nullptr;
	}
	// save the current source location
	auto n = new_expr ();
	n->type = ex_error;

	pr.loc = e->loc;
	return n;
}

ex_listitem_t *
new_listitem (const expr_t *e)
{
	ex_listitem_t *li;
	ALLOC (16384, ex_listitem_t, listitems, li);
	li->expr = e;
	return li;
}

void
list_append (ex_list_t *list, const expr_t *expr)
{
	if (!list->tail) {
		list->tail = &list->head;
	}

	auto li = new_listitem (expr);
	*list->tail = li;
	list->tail = &li->next;
}

void
list_prepend (ex_list_t *list, const expr_t *expr)
{
	if (!list->tail) {
		list->tail = &list->head;
	}

	auto li = new_listitem (expr);
	li->next = list->head;
	list->head = li;

	if (list->tail == &list->head) {
		list->tail = &li->next;
	}
}

expr_t *
expr_append_expr (expr_t *list, const expr_t *expr)
{
	if (list->type != ex_list) {
		internal_error (list, "not a list expression");
	}
	list_append (&list->list, expr);
	return list;
}

expr_t *
expr_prepend_expr (expr_t *list, const expr_t *expr)
{
	if (list->type != ex_list) {
		internal_error (list, "not a list expression");
	}
	list_prepend (&list->list, expr);
	return list;
}

expr_t *
expr_append_list (expr_t *list, ex_list_t *append)
{
	if (list->type != ex_list) {
		internal_error (list, "not a list expression");
	}
	*list->list.tail = append->head;
	list->list.tail = append->tail;
	return list;
}

expr_t *
expr_prepend_list (expr_t *list, ex_list_t *prepend)
{
	if (list->type != ex_list) {
		internal_error (list, "not a list expression");
	}
	if (!list->list.head) {
		list->list.tail = prepend->tail;
	}
	*prepend->tail = list->list.head;
	list->list.head = prepend->head;
	return list;
}

expr_t *
new_list_expr (const expr_t *first)
{
	auto list = new_expr ();
	list->type = ex_list;
	list->list.head = 0;
	list->list.tail = &list->list.head;
	if (first) {
		expr_append_expr (list, first);
	}
	return list;
}

int
list_count (const ex_list_t *list)
{
	int         count = 0;
	for (auto li = list->head; li; li = li->next) {
		count++;
	}
	return count;
}

void
list_scatter (const ex_list_t *list, const expr_t **exprs)
{
	for (auto li = list->head; li; li = li->next) {
		*exprs++ = li->expr;
	}
}

void
list_scatter_rev (const ex_list_t *list, const expr_t **exprs)
{
	int count = list_count (list);
	for (auto li = list->head; li; li = li->next) {
		exprs[--count] = li->expr;
	}
}

void
list_gather (ex_list_t *list, const expr_t **exprs, int count)
{
	if (!list->tail) {
		list->tail = &list->head;
	}
	while (count-- > 0) {
		auto li = new_listitem (*exprs++);
		*list->tail = li;
		list->tail = &li->next;
	}
}

const char *
new_label_name (void)
{
	static int  label = 0;
	int         lnum = ++label;
	const char *fname = current_func->sym->name;
	const char *lname;

	lname = save_string (va (0, "$%s_%d", fname, lnum));
	return lname;
}

static expr_t *
new_error_expr (void)
{
	expr_t     *e = new_expr ();
	e->type = ex_error;
	return e;
}

expr_t *
new_state_expr (const expr_t *frame, const expr_t *think, const expr_t *step)
{
	expr_t     *s = new_expr ();

	s->type = ex_state;
	s->state.frame = frame;
	s->state.think = think;
	s->state.step = step;
	return s;
}

expr_t *
new_bool_expr (ex_boollist_t *true_list, ex_boollist_t *false_list, const expr_t *e)
{
	expr_t     *b = new_expr ();

	b->type = ex_bool;
	b->boolean.true_list = true_list;
	b->boolean.false_list = false_list;
	b->boolean.e = e;
	return b;
}

expr_t *
new_label_expr (void)
{

	expr_t     *l = new_expr ();

	l->type = ex_label;
	l->label.name = new_label_name ();
	return l;
}

const expr_t *
named_label_expr (symbol_t *label)
{
	symbol_t   *sym;
	expr_t     *l;

	if (!current_func) {
		// XXX this might be only an error
		internal_error (0, "label defined outside of function scope");
	}

	sym = symtab_lookup (current_func->label_scope, label->name);

	if (sym) {
		return sym->expr;
	}
	l = new_label_expr ();
	l->label.name = save_string (va (0, "%s_%s", l->label.name,
									   label->name));
	l->label.symbol = label;
	label->sy_type = sy_expr;
	label->expr = l;
	symtab_addsymbol (current_func->label_scope, label);
	return label->expr;
}

expr_t *
new_label_ref (const ex_label_t *label)
{

	expr_t     *l = new_expr ();

	l->type = ex_labelref;
	l->labelref.label = label;
	((ex_label_t *) label)->used++;
	return l;
}

expr_t *
new_block_expr (const expr_t *old)
{
	scoped_src_loc (old);
	expr_t     *b = new_expr ();

	b->type = ex_block;
	b->block.head = 0;
	b->block.tail = &b->block.head;
	if (old) {
		if (old->type != ex_block) {
			internal_error (old, "not a block expression");
		}
		if ((b->block.head = old->block.head)) {
			b->block.tail = old->block.tail;
		}
		b->block.result = old->block.result;
		b->block.is_call = old->block.is_call;
	}
	b->block.return_addr = __builtin_return_address (0);
	return b;
}

expr_t *
new_binary_expr (int op, const expr_t *e1, const expr_t *e2)
{
	if (e1->type == ex_error) {
		internal_error (e1, "error expr in new_binary_expr");
	}
	if (e2 && e2->type == ex_error) {
		internal_error (e2, "error expr in new_binary_expr");
	}

	expr_t     *e = new_expr ();
	e->type = ex_expr;
	e->nodag = e1->nodag | e2->nodag;
	e->expr.op = op;
	e->expr.e1 = e1;
	e->expr.e2 = e2;
	return e;
}

expr_t *
build_block_expr (expr_t *list, bool set_result)
{
	if (list->type != ex_list) {
		return list;
	}
	expr_t     *b = new_block_expr (0);

	b->block.head = list->list.head;
	b->block.tail = list->list.tail;
	if (set_result && b->block.tail != &b->block.head) {
		auto last = (ex_listitem_t *) b->block.tail;
		b->block.result = last->expr;
	}
	return b;
}

expr_t *
new_unary_expr (int op, const expr_t *e1)
{

	if (e1 && e1->type == ex_error) {
		internal_error (e1, "error expr in new_binary_expr");
	}

	expr_t     *e = new_expr ();
	e->type = ex_uexpr;
	e->nodag = e1->nodag;
	e->expr.op = op;
	e->expr.e1 = e1;
	return e;
}

expr_t *
new_horizontal_expr (int op, const expr_t *vec, type_t *type)
{
	vec = convert_name (vec);
	if (vec->type == ex_error) {
		return (expr_t *) vec;
	}
	auto vec_type = get_type (vec);
	if (!is_math (vec_type) || is_scalar (vec_type)) {
		internal_error (vec, "horizontal operand not a vector type");
	}
	if (!is_scalar (type)) {
		internal_error (vec, "horizontal result not a scalar type");
	}

	expr_t     *e = new_expr ();
	e->type = ex_horizontal;
	e->hop.op = op;
	e->hop.vec = vec;
	e->hop.type = type;
	return e;
}

const expr_t *
new_swizzle_expr (const expr_t *src, const char *swizzle)
{
	src = convert_name (src);
	if (src->type == ex_error) {
		return (expr_t *) src;
	}
	auto src_type = get_type (src);
	int         src_width = type_width (src_type);
	ex_swizzle_t swiz = {};

#define m(x) (1 << ((x) - 'a'))
#define v(x, mask) (((x) & 0x60) == 0x60 && (m(x) & (mask)))
#define vind(x) ((x) & 3)
#define cind(x) (-(((x) >> 3) ^ (x)) & 3)
#define tind(x) ((((~(x+1)>>2)&1) + x + 1) & 3)
	const int   color = m('r') | m('g') | m('b') | m('a');
	const int   vector = m('x') | m('y') | m('z') | m('w');
	const int   texture = m('s') | m('t') | m('p') | m('q');

	int         type_mask = 0;
	int         comp_count = 0;

	for (const char *s = swizzle; *s; s++) {
		if (comp_count >= 4) {
			return error (src, "too many components in swizzle");
		}
		if (*s == '0') {
			swiz.zero |= 1 << comp_count;
			comp_count++;
		} else if (*s == '-') {
			swiz.neg |= 1 << comp_count;
		} else {
			int         ind = 0;
			int         mask = 0;
			if (v (*s, vector)) {
				ind = vind (*s);
				mask = 1;
			} else if (v (*s, color)) {
				ind = cind (*s);
				mask = 2;
			} else if (v (*s, texture)) {
				ind = tind (*s);
				mask = 4;
			}
			if (!mask) {
				return error (src, "invalid component in swizzle");
			}
			if (type_mask & ~mask) {
				return error (src, "mixed components in swizzle");
			}
			if (ind >= src_width) {
				return error (src, "swizzle component out of bounds");
			}
			type_mask |= mask;
			swiz.source[comp_count++] = ind;
		}
	}
	swiz.zero |= (0xf << comp_count) & 0xf;
	swiz.src = src;
	swiz.type = vector_type (base_type (src_type), comp_count);

	expr_t     *expr = new_expr ();
	expr->type = ex_swizzle;
	expr->swizzle = swiz;
	return expr;
}

const expr_t *
new_extend_expr (const expr_t *src, const type_t *type, int ext, bool rev)
{
	expr_t     *expr = new_expr ();
	expr->type = ex_extend;
	expr->extend.src = src;
	expr->extend.extend = ext;
	expr->extend.reverse = rev;
	expr->extend.type = type;
	return evaluate_constexpr (expr);
}

expr_t *
new_def_expr (def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_def;
	e->def = def;
	return e;
}

expr_t *
new_symbol_expr (symbol_t *symbol)
{
	expr_t     *e = new_expr ();
	e->type = ex_symbol;
	e->symbol = symbol;
	return e;
}

expr_t *
new_temp_def_expr (const type_t *type)
{
	expr_t     *e = new_expr ();

	e->type = ex_temp;
	e->temp.type = unalias_type (type);
	return e;
}

expr_t *
new_nil_expr (void)
{
	expr_t     *e = new_expr ();
	e->type = ex_nil;
	return e;
}

expr_t *
new_args_expr (void)
{
	expr_t     *e = new_expr ();
	e->type = ex_args;
	return e;
}

const expr_t *
new_value_expr (ex_value_t *value, bool implicit)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->value = value;
	e->implicit = implicit;
	return e;
}

const expr_t *
new_zero_expr (const type_t *type)
{
	pr_type_t zero[type_size (type)] = {};
	return new_value_expr (new_type_value (type, zero), false);
}

const expr_t *
new_name_expr (const char *name)
{
	expr_t     *e = new_expr ();
	symbol_t   *sym;

	sym = symtab_lookup (current_symtab, name);
	if (!sym)
		sym = new_symbol (name);
	e->type = ex_symbol;
	e->symbol = sym;
	return e;
}

const expr_t *
new_string_expr (const char *string_val)
{
	return new_value_expr (new_string_val (string_val), false);
}

const expr_t *
new_double_expr (double double_val, bool implicit)
{
	return new_value_expr (new_double_val (double_val), implicit);
}

const expr_t *
new_float_expr (float float_val, bool implicit)
{
	return new_value_expr (new_float_val (float_val), implicit);
}

const expr_t *
new_vector_expr (const float *vector_val)
{
	return new_value_expr (new_vector_val (vector_val), false);
}

const expr_t *
new_entity_expr (int entity_val)
{
	return new_value_expr (new_entity_val (entity_val), false);
}

const expr_t *
new_field_expr (int field_val, const type_t *type, def_t *def)
{
	return new_value_expr (new_field_val (field_val, type, def), false);
}

const expr_t *
new_func_expr (int func_val, const type_t *type)
{
	return new_value_expr (new_func_val (func_val, type), false);
}

const expr_t *
new_pointer_expr (int val, const type_t *type, def_t *def)
{
	return new_value_expr (new_pointer_val (val, type, def, 0), false);
}

const expr_t *
new_quaternion_expr (const float *quaternion_val)
{
	return new_value_expr (new_quaternion_val (quaternion_val), false);
}

const expr_t *
new_int_expr (int int_val, bool implicit)
{
	return new_value_expr (new_int_val (int_val), implicit);
}

const expr_t *
new_uint_expr (unsigned uint_val)
{
	return new_value_expr (new_uint_val (uint_val), false);
}

const expr_t *
new_long_expr (pr_long_t long_val, bool implicit)
{
	return new_value_expr (new_long_val (long_val), implicit);
}

int
is_long_val (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->value->lltype == ev_long) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const) {
		auto type = e->symbol->type;
		if (is_long (type)) {
			return 1;
		}
	}
	if (e->type == ex_def && e->def->constant) {
		auto type = e->def->type;
		if (is_long (type)) {
			return 1;
		}
	}
	return 0;
}

int
is_ulong_val (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->value->lltype == ev_ulong) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const) {
		auto type = e->symbol->type;
		if (is_ulong (type)) {
			return 1;
		}
	}
	if (e->type == ex_def && e->def->constant) {
		auto type = e->def->type;
		if (is_ulong (type)) {
			return 1;
		}
	}
	return 0;
}

pr_long_t
expr_long (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_long) {
		return e->value->long_val;
	}
	internal_error (e, "not a long constant");
}

const expr_t *
new_ulong_expr (pr_ulong_t ulong_val)
{
	return new_value_expr (new_ulong_val (ulong_val), false);
}

pr_ulong_t
expr_ulong (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_ulong) {
		return e->value->ulong_val;
	}
	internal_error (e, "not a ulong constant");
}

const expr_t *
new_short_expr (short short_val)
{
	return new_value_expr (new_short_val (short_val), false);
}

bool
is_error (const expr_t *e)
{
	return e->type == ex_error;
}

int
is_constant (const expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->alias.expr;
	}
	if (e->type == ex_nil || e->type == ex_value || e->type == ex_labelref
		|| (e->type == ex_symbol && e->symbol->sy_type == sy_const)
		|| (e->type == ex_symbol && e->symbol->sy_type == sy_var
			&& e->symbol->def->constant))
		return 1;
	return 0;
}

int
is_variable (const expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->alias.expr;
	}
	if (e->type == ex_def
		|| (e->type == ex_symbol && e->symbol->sy_type == sy_var)
		|| e->type == ex_temp) {
		return 1;
	}
	return 0;
}

int
is_selector (const expr_t *e)
{
	return e->type == ex_selector;
}

const expr_t *
constant_expr (const expr_t *e)
{
	symbol_t   *sym;
	ex_value_t *value;

	if (!is_constant (e))
		return e;
	if (e->type == ex_nil || e->type == ex_value || e->type == ex_labelref)
		return e;
	if (e->type != ex_symbol)
		return e;
	sym = e->symbol;
	if (sym->sy_type == sy_const) {
		value = sym->value;
	} else if (sym->sy_type == sy_var && sym->def->constant) {
		//FIXME pointers and fields
		internal_error (e, "what to do here?");
		//memset (&value, 0, sizeof (value));
		//memcpy (&value.v, &D_INT (sym->def),
				//type_size (sym->def->type) * sizeof (pr_type_t));
	} else {
		return e;
	}
	scoped_src_loc (e);
	return new_value_expr (value, false);
}

int
is_nil (const expr_t *e)
{
	return e->type == ex_nil;
}

int
is_string_val (const expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->value->lltype == ev_string)
		return 1;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_string)
		return 1;
	return 0;
}

const char *
expr_string (const expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->value->lltype == ev_string)
		return e->value->string_val;
	internal_error (e, "not a string constant");
}

int
is_float_val (const expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->value->lltype == ev_float)
		return 1;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& is_float (e->symbol->type))
		return 1;
	return 0;
}

int
is_double_val (const expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->value->lltype == ev_double)
		return 1;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& is_double (e->symbol->type))
		return 1;
	return 0;
}

double
expr_double (const expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->value->lltype == ev_double)
		return e->value->double_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_double)
		return e->symbol->value->double_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& is_double (e->symbol->def->type))
		return D_DOUBLE (e->symbol->def);
	internal_error (e, "not a double constant");
}

float
expr_float (const expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->value->lltype == ev_float)
		return e->value->float_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_float)
		return e->symbol->value->float_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& is_float (e->symbol->def->type))
		return D_FLOAT (e->symbol->def);
	internal_error (e, "not a float constant");
}

int
is_vector_val (const expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->value->lltype == ev_vector)
		return 1;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_vector)
		return 1;
	return 0;
}

const float *
expr_vector (const expr_t *e)
{
	if (e->type == ex_nil)
		return vec3_origin;
	if (e->type == ex_value && e->value->lltype == ev_vector)
		return e->value->vector_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_vector)
		return e->symbol->value->vector_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& e->symbol->def->type->type == ev_vector)
		return D_VECTOR (e->symbol->def);
	internal_error (e, "not a vector constant");
}

int
is_quaternion_val (const expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->value->lltype == ev_quaternion)
		return 1;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_quaternion)
		return 1;
	return 0;
}

const float *
expr_quaternion (const expr_t *e)
{
	if (e->type == ex_nil)
		return quat_origin;
	if (e->type == ex_value && e->value->lltype == ev_quaternion)
		return e->value->quaternion_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_quaternion)
		return e->symbol->value->quaternion_val;
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& e->symbol->def->type->type == ev_quaternion)
		return D_QUAT (e->symbol->def);
	internal_error (e, "not a quaternion constant");
}

int
is_int_val (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->value->lltype == ev_int) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const) {
		auto type = e->symbol->type;
		if (!is_long (type) && !is_ulong (type) && is_integral (type)) {
			return 1;
		}
	}
	if (e->type == ex_def && e->def->constant) {
		auto type = e->def->type;
		if (!is_long (type) && !is_ulong (type) && is_integral (type)) {
			return 1;
		}
	}
	return 0;
}

int
expr_int (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_int) {
		return e->value->int_val;
	}
	if (e->type == ex_value && e->value->lltype == ev_short) {
		return e->value->short_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& (e->symbol->type->type == ev_int
			|| is_enum (e->symbol->type))) {
		return e->symbol->value->int_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& is_integral (e->symbol->def->type)) {
		return D_INT (e->symbol->def);
	}
	if (e->type == ex_def && e->def->constant
		&& is_integral (e->def->type)) {
		return D_INT (e->def);
	}
	internal_error (e, "not an int constant");
}

int
is_uint_val (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->value->lltype == ev_uint) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& is_integral (e->symbol->type)) {
		return 1;
	}
	if (e->type == ex_def && e->def->constant
		&& is_integral (e->def->type)) {
		return 1;
	}
	return 0;
}

unsigned
expr_uint (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_uint) {
		return e->value->uint_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_uint) {
		return e->symbol->value->uint_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_var
		&& e->symbol->def->constant
		&& is_integral (e->symbol->def->type)) {
		return D_INT (e->symbol->def);
	}
	if (e->type == ex_def && e->def->constant
		&& is_integral (e->def->type)) {
		return D_INT (e->def);
	}
	internal_error (e, "not an unsigned constant");
}

int
is_short_val (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->value->lltype == ev_short) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_short) {
		return 1;
	}
	return 0;
}

short
expr_short (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_short) {
		return e->value->short_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_short) {
		return e->symbol->value->short_val;
	}
	internal_error (e, "not a short constant");
}

unsigned short
expr_ushort (const expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->value->lltype == ev_ushort) {
		return e->value->ushort_val;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& e->symbol->type->type == ev_ushort) {
		return e->symbol->value->ushort_val;
	}
	internal_error (e, "not a ushort constant");
}

int
is_integral_val (const expr_t *e)
{
	if (is_constant (e)) {
		if (is_int_val (e)) {
			return 1;
		}
		if (is_uint_val (e)) {
			return 1;
		}
		if (is_short_val (e)) {
			return 1;
		}
		if (is_long_val (e)) {
			return 1;
		}
		if (is_ulong_val (e)) {
			return 1;
		}
	}
	return 0;
}

int
is_floating_val (const expr_t *e)
{
	if (is_constant (e)) {
		if (is_float_val (e)) {
			return 1;
		}
		if (is_double_val (e)) {
			return 1;
		}
	}
	return 0;
}

pr_long_t
expr_integral (const expr_t *e)
{
	if (is_constant (e)) {
		if (is_int_val (e)) {
			return expr_int (e);
		}
		if (is_uint_val (e)) {
			return expr_uint (e);
		}
		if (is_short_val (e)) {
			return expr_short (e);
		}
		if (is_long_val (e)) {
			return expr_long (e);
		}
		if (is_ulong_val (e)) {
			return expr_ulong (e);
		}
	}
	internal_error (e, "not an integral constant");
}

double
expr_floating (const expr_t *e)
{
	if (is_constant (e)) {
		if (is_float_val (e)) {
			return expr_float (e);
		}
		if (is_double_val (e)) {
			return expr_double (e);
		}
	}
	internal_error (e, "not an integral constant");
}

int
is_pointer_val (const expr_t *e)
{
	if (e->type == ex_value && e->value->lltype == ev_ptr) {
		return 1;
	}
	return 0;
}

int
is_math_val (const expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->alias.expr;
	}
	if (e->type == ex_value && is_math (e->value->type)) {
		return 1;
	}
	if (e->type == ex_symbol && e->symbol->sy_type == sy_const
		&& is_math (e->symbol->value->type)) {
		return 1;
	}
	return 0;
}

const expr_t *
new_alias_expr (const type_t *type, const expr_t *expr)
{
	if (is_ptr (type) && expr->type == ex_address) {
		auto new = new_address_expr (type, expr->address.lvalue,
									 expr->address.offset);
		new->address.type = type;
		return new;
	}
	if (expr->type == ex_alias) {
		if (expr->alias.offset) {
			return new_offset_alias_expr (type, expr, 0);
		}
		expr = expr->alias.expr;
	}
	// this can happen when constant folding an offset pointer results in
	// a noop due to the offset being 0 and thus casting back to the original
	// type
	if (type == get_type (expr)) {
		return (expr_t *) expr;
	}

	expr_t     *alias = new_expr ();
	alias->type = ex_alias;
	alias->alias.type = type;
	alias->alias.expr = expr;
	alias->loc = expr->loc;
	return edag_add_expr (alias);
}

const expr_t *
new_offset_alias_expr (const type_t *type, const expr_t *expr, int offset)
{
	if (expr->type == ex_alias && expr->alias.offset) {
		const expr_t *ofs_expr = expr->alias.offset;
		if (!is_constant (ofs_expr)) {
			internal_error (ofs_expr, "non-constant offset for alias expr");
		}
		offset += expr_int (ofs_expr);

		if (expr->alias.expr->type == ex_alias) {
			internal_error (expr, "alias expr of alias expr");
		}
		expr = expr->alias.expr;
	}

	expr_t     *alias = new_expr ();
	alias->type = ex_alias;
	alias->alias.type = type;
	alias->alias.expr = edag_add_expr (expr);
	alias->alias.offset = edag_add_expr (new_int_expr (offset, false));
	alias->loc = expr->loc;
	return edag_add_expr (evaluate_constexpr (alias));
}

expr_t *
new_address_expr (const type_t *lvtype, const expr_t *lvalue,
				  const expr_t *offset)
{
	expr_t     *addr = new_expr ();
	addr->type = ex_address;
	addr->address.type = pointer_type (lvtype);
	addr->address.lvalue = lvalue;
	addr->address.offset = offset;
	return addr;
}

expr_t *
new_assign_expr (const expr_t *dst, const expr_t *src)
{
	expr_t     *addr = new_expr ();
	addr->type = ex_assign;
	addr->assign.dst = dst;
	addr->assign.src = src;
	return addr;
}

expr_t *
new_return_expr (const expr_t *ret_val)
{
	expr_t     *retrn = new_expr ();
	retrn->type = ex_return;
	retrn->retrn.ret_val = ret_val;
	return retrn;
}

expr_t *
new_adjstk_expr (int mode, int offset)
{
	expr_t     *adj = new_expr ();
	adj->type = ex_adjstk;
	adj->adjstk.mode = mode;
	adj->adjstk.offset = offset;
	return adj;
}

expr_t *
new_with_expr (int mode, int reg, const expr_t *val)
{
	expr_t     *with = new_expr ();
	with->type = ex_with;
	with->with.mode = mode;
	with->with.reg = reg;
	with->with.with = val;
	return with;
}

static const expr_t *
param_expr (const char *name, const type_t *type)
{
	symbol_t   *sym;
	expr_t     *sym_expr;

	sym = make_symbol (name, &type_param, pr.symtab->space, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	sym_expr = new_symbol_expr (sym);
	return new_alias_expr (type, sym_expr);
}

const expr_t *
new_ret_expr (type_t *type)
{
	return param_expr (".return", type);
}

const expr_t *
new_param_expr (const type_t *type, int num)
{
	return param_expr (va (0, ".param_%d", num), type);
}

expr_t *
new_memset_expr (const expr_t *dst, const expr_t *val, const expr_t *count)
{
	expr_t     *e = new_expr ();
	e->type = ex_memset;
	e->memset.dst = dst;
	e->memset.val = val;
	e->memset.count = count;
	return e;
}

expr_t *
new_type_expr (const type_t *type)
{
	expr_t     *e = new_expr ();
	e->type = ex_type;
	e->typ.type = type;
	return e;
}

expr_t *
append_expr (expr_t *block, const expr_t *e)
{
	if (block->type != ex_block)
		internal_error (block, "not a block expression");

	if (!e || e->type == ex_error)
		return block;

	auto li = new_listitem (e);
	*block->block.tail = li;
	block->block.tail = &li->next;

	return block;
}

expr_t *
prepend_expr (expr_t *block, const expr_t *e)
{
	if (block->type != ex_block)
		internal_error (block, "not a block expression");

	if (!e || e->type == ex_error)
		return block;

	auto li = new_listitem (e);
	li->next = block->block.head;
	block->block.head = li;

	if (block->block.tail == &block->block.head) {
		block->block.tail = &li->next;
	}

	return block;
}

symbol_t *
get_name (const expr_t *e)
{
	if (e->type == ex_symbol) {
		return e->symbol;
	}
	return 0;
}

symbol_t *
get_struct_field (const type_t *t1, const expr_t *e1, const expr_t *e2)
{
	symtab_t   *strct = t1->symtab;
	symbol_t   *sym = get_name (e2);
	symbol_t   *field;

	if (!strct) {
		error (e1, "dereferencing pointer to incomplete type");
		return 0;
	}
	if (!sym) {
		error (e2, "field reference is not a name");
		return 0;
	}
	field = symtab_lookup (strct, sym->name);
	if (!field && !is_entity(t1) && !is_nonscalar (t1)) {
		const char *name = t1->name;
		if (!strncmp (name, "tag ", 4)) {
			name += 4;
		}
		error (e2, "'%s' has no member named '%s'", name, sym->name);
	}
	return field;
}

static const expr_t *
swizzle_expr (const expr_t *vec, const expr_t *swizzle)
{
	auto sym = get_name (swizzle);
	if (!sym) {
		// error already reported
		return vec;
	}
	return new_swizzle_expr (vec, sym->name);
}

const expr_t *
field_expr (const expr_t *e1, const expr_t *e2)
{
	const type_t *t1, *t2;
	expr_t     *e;

	e1 = convert_name (e1);
	t1 = get_type (e1);
	if (e1->type == ex_error)
		return e1;
	if (is_entity (t1)) {
		symbol_t   *field = 0;

		if (e2->type == ex_symbol)
			field = get_struct_field (&type_entity, e1, e2);
		if (field) {
			e2 = new_field_expr (0, field->type, field->def);
			e = new_binary_expr ('.', e1, e2);
			e->expr.type = field->type;
			return e;
		} else {
			e2 = convert_name (e2);
			t2 = get_type (e2);
			if (e2->type == ex_error)
				return e2;
			if (t2->type == ev_field) {
				e = new_binary_expr ('.', e1, e2);
				e->expr.type = t2->fldptr.type;
				return e;
			}
		}
	} else if (is_ptr (t1)) {
		if (is_struct (t1->fldptr.type) || is_union (t1->fldptr.type)) {
			symbol_t   *field;

			field = get_struct_field (t1->fldptr.type, e1, e2);
			if (!field)
				return e1;

			const expr_t *offset = new_short_expr (field->offset);
			e1 = offset_pointer_expr (e1, offset);
			if (e1->type == ex_error) {
				return e1;
			}
			e1 = cast_expr (pointer_type (field->type), e1);
			return unary_expr ('.', e1);
		} else if (is_class (t1->fldptr.type)) {
			class_t    *class = t1->fldptr.type->class;
			symbol_t   *sym = e2->symbol;//FIXME need to check
			symbol_t   *ivar;
			int         protected = class_access (current_class, class);

			ivar = class_find_ivar (class, protected, sym->name);
			if (!ivar)
				return new_error_expr ();
			const expr_t *offset = new_short_expr (ivar->offset);
			e1 = offset_pointer_expr (e1, offset);
			e1 = cast_expr (pointer_type (ivar->type), e1);
			if (e1->type == ex_error) {
				return e1;
			}
			return unary_expr ('.', e1);
		}
	} else if (is_algebra (t1)) {
		return algebra_field_expr (e1, e2);
	} else if (is_nonscalar (t1) || is_struct (t1) || is_union (t1)) {
		symbol_t   *field;

		field = get_struct_field (t1, e1, e2);
		if (!field) {
			if (is_nonscalar (t1)) {
				return swizzle_expr (e1, e2);
			}
			return e1;
		}

		if (e1->type == ex_expr && e1->expr.op == '.'
			&& is_entity(get_type (e1->expr.e1))) {
			// undo the . expression
			e2 = e1->expr.e2;
			e1 = e1->expr.e1;
			// offset the field expresion
			if (e2->type == ex_symbol) {
				symbol_t   *sym;
				def_t      *def;
				sym = symtab_lookup (pr.entity_fields, e2->symbol->name);
				if (!sym) {
					internal_error (e2, "failed to find entity field %s",
									e2->symbol->name);
				}
				def = sym->def;
				e2 = new_field_expr (0, field->type, def);
			} else if (e2->type != ex_value
					   || e2->value->lltype != ev_field) {
				internal_error (e2, "unexpected field exression");
			}
			auto fv = new_field_val (e2->value->pointer.val + field->offset, field->type, e2->value->pointer.def);
			scoped_src_loc (e2);
			e2 = new_value_expr (fv, false);
			// create a new . expression
			return field_expr (e1, e2);
		} else {
			if (e1->type == ex_uexpr && e1->expr.op == '.') {
				const expr_t *offset = new_short_expr (field->offset);
				e1 = offset_pointer_expr (e1->expr.e1, offset);
				e1 = cast_expr (pointer_type (field->type), e1);
				return unary_expr ('.', e1);
			} else {
				return new_offset_alias_expr (field->type, e1, field->offset);
			}
		}
	} else if (is_class (t1)) {
		//Class instance variables aren't allowed and thus declaring one
		//is treated as an error, so this is a follow-on error.
		return error (e1, "class instance access");
	}
	return type_mismatch (e1, e2, '.');
}

const expr_t *
convert_from_bool (const expr_t *e, const type_t *type)
{
	const expr_t *zero;
	const expr_t *one;

	expr_t     *enum_zero, *enum_one;

	if (is_float (type)) {
		one = new_float_expr (1, false);
		zero = new_float_expr (0, false);
	} else if (is_int (type)) {
		one = new_int_expr (1, false);
		zero = new_int_expr (0, false);
	} else if (is_enum (type) && enum_as_bool (type, &enum_zero, &enum_one)) {
		zero = enum_zero;
		one = enum_one;
	} else if (is_uint (type)) {
		one = new_uint_expr (1);
		zero = new_uint_expr (0);
	} else {
		return error (e, "can't convert from boolean value");
	}
	auto cond = new_expr ();
	*cond = *e;

	return conditional_expr (cond, one, zero);
}

const expr_t *
convert_nil (const expr_t *e, const type_t *t)
{
	scoped_src_loc (e);
	auto nil = new_expr ();
	nil->type = ex_nil;
	nil->nil = t;
	return nil;
}

int
is_compare (int op)
{
	if (op == QC_EQ || op == QC_NE || op == QC_LE || op == QC_GE
		|| op == QC_LT || op == QC_GT || op == '>' || op == '<')
		return 1;
	return 0;
}

int
is_math_op (int op)
{
	if (op == '*' || op == '/' || op == '+' || op == '-')
		return 1;
	return 0;
}

int
is_logic (int op)
{
	if (op == QC_OR || op == QC_AND)
		return 1;
	return 0;
}

int
has_function_call (const expr_t *e)
{
	switch (e->type) {
		case ex_bool:
			return has_function_call (e->boolean.e);
		case ex_block:
			if (e->block.is_call)
				return 1;
		case ex_list:
			for (auto li = e->block.head; li; li = li->next)
				if (has_function_call (li->expr))
					return 1;
			return 0;
		case ex_expr:
			return (has_function_call (e->expr.e1)
					|| has_function_call (e->expr.e2));
		case ex_uexpr:
			return has_function_call (e->expr.e1);
		case ex_alias:
			return has_function_call (e->alias.expr);
		case ex_address:
			return has_function_call (e->address.lvalue);
		case ex_assign:
			return (has_function_call (e->assign.dst)
					|| has_function_call (e->assign.src));
		case ex_branch:
			if (e->branch.type == pr_branch_call) {
				return 1;
			}
			if (e->branch.type == pr_branch_jump) {
				return 0;
			}
			return has_function_call (e->branch.test);
		case ex_inout:
			// in is just a cast of out, if it's not null
			return has_function_call (e->inout.out);
		case ex_return:
			return has_function_call (e->retrn.ret_val);
		case ex_horizontal:
			return has_function_call (e->hop.vec);
		case ex_swizzle:
			return has_function_call (e->swizzle.src);
		case ex_extend:
			return has_function_call (e->extend.src);
		case ex_error:
		case ex_state:
		case ex_label:
		case ex_labelref:
		case ex_def:
		case ex_symbol:
		case ex_temp:
		case ex_vector:
		case ex_selector:
		case ex_nil:
		case ex_value:
		case ex_compound:
		case ex_memset:
		case ex_adjstk:
		case ex_with:
		case ex_args:
		case ex_type:
			return 0;
		case ex_multivec:
			for (auto c = e->multivec.components.head; c; c = c->next) {
				if (has_function_call (c->expr)) {
					return 1;
				}
			}
			return 0;
		case ex_count:
			break;
	}
	internal_error (e, "invalid expression type");
}

int
is_function_call (const expr_t *e)
{
	if (e->type != ex_block || !e->block.is_call) {
		return 0;
	}
	e = e->block.result;
	return e->type == ex_branch && e->branch.type == pr_branch_call;
}

const expr_t *
asx_expr (int op, const expr_t *e1, const expr_t *e2)
{
	if (e1->type == ex_error)
		return e1;
	else if (e2->type == ex_error)
		return e2;
	else {
		expr_t     *e = new_expr ();
		auto paren = new_expr ();

		*e = *e1;
		*paren = *e2;
		paren->paren = 1;
		return assign_expr (e, binary_expr (op, e1, paren));
	}
}

const expr_t *
unary_expr (int op, const expr_t *e)
{
	vec3_t      v;
	quat_t      q;
	const char *s;
	const type_t *t;

	e = convert_name (e);
	if (e->type == ex_error)
		return e;
	switch (op) {
		case '-':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary -");
			if (is_algebra (get_type (e))) {
				return algebra_negate (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, "type check failed!");
					case ev_double:
						return new_double_expr (-expr_double (e), e->implicit);
					case ev_float:
						return new_float_expr (-expr_float (e), e->implicit);
					case ev_vector:
						VectorNegate (expr_vector (e), v);
						return new_vector_expr (v);
					case ev_quaternion:
						QuatNegate (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (-expr_int (e), false);
					case ev_uint:
						return new_uint_expr (-expr_uint (e));
					case ev_short:
						return new_short_expr (-expr_short (e));
					case ev_invalid:
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->expr.op == '-') {
						return e->expr.e1;
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_block:
					if (!e->block.result) {
						return error (e, "invalid type for unary -");
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_branch:
				case ex_inout:
					return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_bool:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_def:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = e->def->type;
						return edag_add_expr (n);
					}
				case ex_symbol:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = e->symbol->type;
						return edag_add_expr (n);
					}
				case ex_multivec:
					return algebra_negate (e);
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary -");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '!':
			if (is_algebra (get_type (e))) {
				return algebra_dual (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, 0);
					case ev_string:
						s = expr_string (e);
						return new_int_expr (!s || !s[0], false);
					case ev_double:
						return new_int_expr (!expr_double (e), false);
					case ev_float:
						return new_int_expr (!expr_float (e), false);
					case ev_vector:
						return new_int_expr (!VectorIsZero (expr_vector (e)),
											 false);
					case ev_quaternion:
						return new_int_expr (!QuatIsZero (expr_quaternion (e)),
											 false);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (!expr_int (e), e->implicit);
					case ev_uint:
						return new_uint_expr (!expr_uint (e));
					case ev_short:
						return new_short_expr (!expr_short (e));
					case ev_invalid:
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_bool:
					return new_bool_expr (e->boolean.false_list,
										  e->boolean.true_list, e);
				case ex_block:
					if (!e->block.result)
						return error (e, "invalid type for unary !");
				case ex_uexpr:
				case ex_expr:
				case ex_def:
				case ex_symbol:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_address:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
					if (options.code.progsversion == PROG_VERSION) {
						return binary_expr (QC_EQ, e, new_nil_expr ());
					} else {
						expr_t     *n = new_unary_expr (op, e);

						if (options.code.progsversion > PROG_ID_VERSION)
							n->expr.type = &type_int;
						else
							n->expr.type = &type_float;
						return n;
					}
				case ex_multivec:
					return algebra_dual (e);
				case ex_branch:
				case ex_inout:
				case ex_nil:
					return error (e, "invalid type for unary !");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '~':
			if (is_algebra (get_type (e))) {
				return algebra_reverse (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
					case ev_vector:
					case ev_double:
						return error (e, "invalid type for unary ~");
					case ev_float:
						return new_float_expr (~(int) expr_float (e),
											   e->implicit);
					case ev_quaternion:
						QuatConj (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (~expr_int (e), e->implicit);
					case ev_uint:
						return new_uint_expr (~expr_uint (e));
					case ev_short:
						return new_short_expr (~expr_short (e));
					case ev_invalid:
						t = get_type (e);
						if (t->meta == ty_enum) {
							return new_int_expr (~expr_int (e), false);
						}
						break;
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->expr.op == '~')
						return e->expr.e1;
					goto bitnot_expr;
				case ex_block:
					if (!e->block.result)
						return error (e, "invalid type for unary ~");
					goto bitnot_expr;
				case ex_branch:
				case ex_inout:
					return error (e, "invalid type for unary ~");
				case ex_expr:
				case ex_bool:
				case ex_def:
				case ex_symbol:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
bitnot_expr:
					if (options.code.progsversion == PROG_ID_VERSION) {
						const expr_t *n1 = new_int_expr (-1, false);
						return binary_expr ('-', n1, e);
					} else {
						expr_t     *n = new_unary_expr (op, e);
						auto t = get_type (e);

						if (!is_int(t) && !is_float(t)
							&& !is_quaternion(t))
							return error (e, "invalid type for unary ~");
						n->expr.type = t;
						return n;
					}
				case ex_multivec:
					return algebra_reverse (e);
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary ~");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '.':
			{
				if (extract_type (e) != ev_ptr)
					return error (e, "invalid type for unary .");
				scoped_src_loc (e);
				auto new = new_unary_expr ('.', e);
				new->expr.type = get_type (e)->fldptr.type;
				return new;
			}
		case '+':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary +");
			return e;
		case QC_REVERSE:
			return algebra_reverse (e);
		case QC_DUAL:
			return algebra_dual (e);
		case QC_UNDUAL:
			return algebra_undual (e);
	}
	internal_error (e, 0);
}

void
vararg_integer (const expr_t *e)
{
	if (is_int_val (e) && options.code.progsversion < PROG_VERSION
		&& options.warnings.vararg_integer) {
		warning (e, "passing int constant into ... function");
	}
}

const expr_t *
build_function_call (const expr_t *fexpr, const type_t *ftype, const expr_t *params)
{
	int         param_count = 0;
	expr_t     *call;
	const expr_t *err = 0;

	int arg_count = params ? list_count (&params->list) : 0;
	const expr_t *arguments[arg_count + 1];
	if (params) {
		list_scatter_rev (&params->list, arguments);
	}

	for (int i = 0; i < arg_count; i++) {
		auto e = arguments[i];
		if (e->type == ex_error) {
			return e;
		}
		if (e->type != ex_compound) {
			arguments[i] = algebra_optimize (e);
		}
	}

	if (options.code.progsversion < PROG_VERSION
		&& arg_count > PR_MAX_PARAMS) {
		return error (fexpr, "more than %d parameters", PR_MAX_PARAMS);
	}

	if (ftype->func.num_params < -1) {
		if (-arg_count > ftype->func.num_params + 1) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = -ftype->func.num_params - 1;
	} else if (ftype->func.num_params >= 0) {
		if (arg_count > ftype->func.num_params) {
			return error (fexpr, "too many arguments");
		} else if (arg_count < ftype->func.num_params) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = ftype->func.num_params;
	}

	const type_t *arg_types[arg_count + 1];
	// params is reversed (a, b, c) -> c, b, a
	for (int i = 0; i < arg_count; i++) {
		auto e = arguments[i];
		const type_t *t;

		if (e->type == ex_compound) {
			if (i < param_count) {
				t = ftype->func.param_types[i];
			} else {
				return error (e, "cannot pass compound initializer "
							  "through ...");
			}
		} else {
			t = get_type (e);
		}
		if (!t) {
			return e;
		}

		if (!type_size (t)) {
			err = error (e, "type of formal parameter %d is incomplete",
						 i + 1);
		}
		if (!is_array (t) && value_too_large (t)) {
			err = error (e, "formal parameter %d is too large to be passed by"
						 " value", i + 1);
		}
		if (i < param_count) {
			auto param_type = ftype->func.param_types[i];
			if (e->type == ex_nil)
				e = convert_nil (e, t = param_type);
			if (e->type == ex_bool)
				e = convert_from_bool (e, param_type);
			if (e->type == ex_error) {
				err = e;
				continue;
			}
			auto param_qual = ftype->func.param_quals[i];
			if (param_qual == pq_out || param_qual == pq_inout) {
				//FIXME should be able to use something like *foo() as
				//an out or inout arg
				if (!is_lvalue (e) || has_function_call (e)) {
					error (e, "lvalue required for %s parameter",
						   param_qual & pq_in ? "inout" : "out");
				}
				if (param_qual == pq_inout
					&& !type_assignable (param_type, t)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  param_type, t);
				}
				// check assignment FROM parameter is ok, but only if
				// there wasn't an earlier error so param mismatch doesn't
				// get double-reported for inout params.
				if (!err && !type_assignable (t, param_type)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  t, param_type);
				}
			} else {
				if (!type_assignable (param_type, t)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  param_type, t);
				}
			}
			t = param_type;
		} else {
			if (e->type == ex_nil)
				e = convert_nil (e, t = type_nil);
			if (e->type == ex_bool)
				e = convert_from_bool (e, get_type (e));
			if (is_int_val (e)
				&& options.code.progsversion == PROG_ID_VERSION)
				e = cast_expr (&type_float, e);
			if (options.code.promote_float) {
				if (is_scalar (get_type (e)) && is_float (get_type (e))) {
					t = &type_double;
				}
			} else {
				if (is_scalar (get_type (e)) && is_double (get_type (e))) {
					if (!e->implicit) {
						warning (e, "passing double into ... function");
					}
					if (is_constant (e)) {
						// don't auto-demote non-constant doubles
						t = &type_float;
					}
				}
			}
			vararg_integer (e);
		}
		arg_types[i] = t;
	}
	if (err) {
		return err;
	}

	bool        emit_args = false;
	if (ftype->func.num_params < 0) {
		emit_args = !ftype->func.no_va_list;
	}
	scoped_src_loc (fexpr);
	call = new_block_expr (0);
	call->block.is_call = 1;
	int         arg_expr_count = 0;
	const expr_t *arg_exprs[arg_count + 1][2];
	expr_t     *args = new_list_expr (0);
	// args is built in reverse order so it matches params
	for (int i = 0; i < arg_count; i++) {
		auto param_qual = i < param_count ? ftype->func.param_quals[i] : pq_in;
		if (emit_args && i == param_count) {
			expr_prepend_expr (args, new_args_expr ());
			emit_args = false;
		}

		auto e = arguments[i];
		if (e->type == ex_compound || e->type == ex_multivec) {
			scoped_src_loc (e);
			e = initialized_temp_expr (arg_types[i], e);
		}
		// FIXME this is target-specific info and should not be in the
		// expression tree
		// That, or always use a temp, since it should get optimized out
		if (has_function_call (e)) {
			scoped_src_loc (e);
			auto cast = cast_expr (arg_types[i], e);
			auto tmp = new_temp_def_expr (arg_types[i]);
			expr_prepend_expr (args, tmp);

			arg_exprs[arg_expr_count][0] = cast;
			arg_exprs[arg_expr_count][1] = tmp;
			arg_expr_count++;
		} else {
			if (param_qual != pq_out) {
				// out parameters do not need to be sent so no need to cast
			}
			if (param_qual & pq_out) {
				auto inout = new_expr ();
				inout->type = ex_inout;
				if (param_qual == pq_inout) {
					inout->inout.in = cast_expr (arg_types[i], e);
				}
				inout->inout.out = e;
				e = inout;
			} else {
				e = cast_expr (arg_types[i], e);
			}
			expr_prepend_expr (args, e);
		}
	}
	if (emit_args) {
		emit_args = false;
		expr_prepend_expr (args, new_args_expr ());
	}
	for (int i = 0; i < arg_expr_count - 1; i++) {
		scoped_src_loc (arg_exprs[i][0]);
		auto assign = assign_expr (arg_exprs[i][1], arg_exprs[i][0]);
		append_expr (call, assign);
	}
	if (arg_expr_count) {
		scoped_src_loc (arg_exprs[arg_expr_count - 1][0]);
		auto e = assign_expr (arg_exprs[arg_expr_count - 1][1],
							  arg_exprs[arg_expr_count - 1][0]);
		append_expr (call, e);
	}
	auto ret_type = ftype->func.ret_type;
	call->block.result = call_expr (fexpr, args, ret_type);
	return call;
}

const expr_t *
function_expr (const expr_t *fexpr, const expr_t *params)
{
	if (params) {
		for (auto p = params->list.head; p; p = p->next) {
			p->expr = convert_name (p->expr);
		}
	}

	if (fexpr->type == ex_symbol && fexpr->symbol->sy_type == sy_type) {
		return constructor_expr (fexpr, params);
	}

	fexpr = find_function (fexpr, params);
	fexpr = convert_name (fexpr);
	if (is_error (fexpr)) {
		return fexpr;
	}

	auto ftype = get_type (fexpr);
	if (ftype->type != ev_func) {
		if (fexpr->type == ex_symbol)
			return error (fexpr, "Called object \"%s\" is not a function",
						  fexpr->symbol->name);
		else
			return error (fexpr, "Called object is not a function");
	}

	if (fexpr->type == ex_symbol && params && is_string_val (params)) {
		// FIXME eww, I hate this, but it's needed :(
		// FIXME make a qc hook? :)
		if (strncmp (fexpr->symbol->name, "precache_sound", 14) == 0)
			PrecacheSound (expr_string (params), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_model", 14) == 0)
			PrecacheModel (expr_string (params), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_file", 13) == 0)
			PrecacheFile (expr_string (params), fexpr->symbol->name[13]);
	}

	return build_function_call (fexpr, ftype, params);
}

const expr_t *
branch_expr (int op, const expr_t *test, const expr_t *label)
{
	// need to translate op due to precedence rules dictating the layout
	// of the token ids
	static pr_branch_e branch_type [] = {
		pr_branch_eq,
		pr_branch_ne,
		pr_branch_lt,
		pr_branch_gt,
		pr_branch_le,
		pr_branch_ge,
	};
	if (op < QC_EQ || op > QC_LE) {
		internal_error (label, "invalid op: %d", op);
	}
	if (label && label->type != ex_label) {
		internal_error (label, "not a label");
	}
	if (label) {
		((expr_t *) label)->label.used++;
	}
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->branch.type = branch_type[op - QC_EQ];
	branch->branch.target = label;
	branch->branch.test = test;
	return branch;
}

const expr_t *
goto_expr (const expr_t *label)
{
	if (label && label->type != ex_label) {
		internal_error (label, "not a label");
	}
	if (label) {
		((expr_t *) label)->label.used++;//FIXME cast
	}
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->branch.type = pr_branch_jump;
	branch->branch.target = label;
	return branch;
}

const expr_t *
jump_table_expr (const expr_t *table, const expr_t *index)
{
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->branch.type = pr_branch_jump;
	branch->branch.target = table;//FIXME separate? all branch types can
	branch->branch.index = index;
	return branch;
}

const expr_t *
call_expr (const expr_t *func, const expr_t *args, const type_t *ret_type)
{
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->branch.type = pr_branch_call;
	branch->branch.target = func;
	branch->branch.args = args;
	branch->branch.ret_type = ret_type;
	return branch;
}

const expr_t *
return_expr (function_t *f, const expr_t *e)
{
	const type_t *t;
	const type_t *ret_type = unalias_type (f->type->func.ret_type);

	e = convert_name (e);
	if (!e) {
		if (!is_void(ret_type)) {
			if (options.traditional) {
				if (options.warnings.traditional)
					warning (e,
							 "return from non-void function without a value");
				// force a nil return value in case qf code is being generated
				e = new_nil_expr ();
			} else {
				e = error (e, "return from non-void function without a value");
				return e;
			}
		}
		// the traditional check above may have set e
		if (!e) {
			return new_return_expr (0);
		}
	}

	if (e->type == ex_compound || e->type == ex_multivec) {
		scoped_src_loc (e);
		e = initialized_temp_expr (ret_type, e);
	} else {
		e = algebra_optimize (e);
	}

	t = get_type (e);

	if (!t) {
		return e;
	}
	if (is_void(ret_type)) {
		if (!options.traditional)
			return error (e, "returning a value for a void function");
		if (options.warnings.traditional)
			warning (e, "returning a value for a void function");
	}
	if (e->type == ex_bool) {
		e = convert_from_bool (e, ret_type);
	}
	if (is_float(ret_type) && is_int_val (e)) {
		e = cast_expr (&type_float, e);
		t = &type_float;
	}
	if (is_void(t)) {
		if (e->type == ex_nil) {
			t = ret_type;
			e = convert_nil (e, t);
		} else {
			if (!options.traditional)
				return error (e, "void value not ignored as it ought to be");
			if (options.warnings.traditional)
				warning (e, "void value not ignored as it ought to be");
			//FIXME does anything need to be done here?
		}
	}
	if (!type_assignable (ret_type, t)) {
		if (!options.traditional)
			return error (e, "type mismatch for return value of %s: %s -> %s",
						  f->sym->name, get_type_string (t),
						  get_type_string (ret_type));
		if (options.warnings.traditional)
			warning (e, "type mismatch for return value of %s",
					 f->sym->name);
	} else {
		if (ret_type != t) {
			e = cast_expr (ret_type, e);
			t = f->sym->type->func.ret_type;
		}
	}
	if (e->type == ex_vector) {
		e = assign_expr (new_temp_def_expr (t), e);
	}
	if (e->type == ex_block) {
		((expr_t *) e->block.result)->rvalue = 1;//FIXME
	}
	return new_return_expr (e);
}

const expr_t *
at_return_expr (function_t *f, const expr_t *e)
{
	const type_t *ret_type = unalias_type (f->type->func.ret_type);

	if (!is_void(ret_type)) {
		return error (e, "use of @return in non-void function");
	}
	if (is_nil (e)) {
		// int or pointer 0 seems reasonable
		return new_return_expr (new_int_expr (0, false));
	} else if (!is_function_call (e)) {
		return error (e, "@return value not a function");
	}
	const expr_t *call_expr = e->block.result->branch.target;
	const auto call_type = get_type (call_expr);
	if (!is_func (call_type) && !call_type->func.void_return) {
		return error (e, "@return function not void_return");
	}
	expr_t     *ret_expr = new_return_expr (e);
	ret_expr->retrn.at_return = 1;
	return ret_expr;
}

const expr_t *
conditional_expr (const expr_t *cond, const expr_t *e1, const expr_t *e2)
{
	if (cond->type == ex_error)
		return cond;
	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;

	expr_t *c = (expr_t *) convert_bool (cond, 1);
	if (c->type == ex_error)
		return c;

	scoped_src_loc (cond);

	expr_t     *block = new_block_expr (0);
	auto type1 = get_type (e1);
	auto type2 = get_type (e2);
	expr_t     *tlabel = new_label_expr ();
	expr_t     *flabel = new_label_expr ();
	expr_t     *elabel = new_label_expr ();

	backpatch (c->boolean.true_list, tlabel);
	backpatch (c->boolean.false_list, flabel);

	if (!type_same (type1, type2)) {
		if (!type_assignable (type1, type2)
			&& !type_assignable (type2, type1)) {
			type1 = 0;
		}
		if (!type_assignable (type1, type2)) {
			type1 = type2;
		}
		if (type_promotes (type2, type1)) {
			type1 = type2;
		}
	}

	block->block.result = type1 ? new_temp_def_expr (type1) : 0;
	append_expr (block, c);
	append_expr ((expr_t *) c->boolean.e, flabel);//FIXME cast
	if (block->block.result)
		append_expr (block, assign_expr (block->block.result, e2));
	else
		append_expr (block, e2);
	append_expr (block, goto_expr (elabel));
	append_expr (block, tlabel);
	if (block->block.result)
		append_expr (block, assign_expr (block->block.result, e1));
	else
		append_expr (block, e1);
	append_expr (block, elabel);
	return block;
}

const expr_t *
incop_expr (int op, const expr_t *e, int postop)
{
	if (e->type == ex_error)
		return e;

	auto one = new_int_expr (1, false);	// int constants get auto-cast to float
	if (is_scalar (get_type (e))) {
		one = cast_expr (get_type (e), one);
	}
	if (postop) {
		expr_t     *t1, *t2;
		auto type = get_type (e);
		expr_t     *block = new_block_expr (0);

		if (e->type == ex_error)	// get_type failed
			return e;
		t1 = new_temp_def_expr (type);
		t2 = new_temp_def_expr (type);
		append_expr (block, assign_expr (t1, e));
		append_expr (block, assign_expr (t2, binary_expr (op, t1, one)));
		if (e->type == ex_uexpr && e->expr.op == '.')
			e = deref_pointer_expr (address_expr (e, 0));
		append_expr (block, assign_expr (e, t2));
		block->block.result = t1;
		return block;
	} else {
		return asx_expr (op, e, one);
	}
}

const expr_t *
array_expr (const expr_t *array, const expr_t *index)
{
	array = convert_name (array);
	index = convert_name (index);

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

	if (!is_ptr (array_type) && !is_array (array_type)
		&& !is_nonscalar (array_type))
		return error (array, "not an array");
	if (!is_integral (index_type))
		return error (index, "invalid array index type");
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_int_val (index))
		ind = expr_int (index);
	if (is_array (array_type)
		&& array_type->array.size
		&& is_constant (index)
		&& (ind < array_type->array.base
			|| ind - array_type->array.base >= array_type->array.size)) {
		return error (index, "array index out of bounds");
	}
	if (is_nonscalar (array_type)
		&& is_constant (index)
		&& (ind < 0 || ind >= array_type->width)) {
		return error (index, "array index out of bounds");
	}
	if (is_array (array_type)) {
		ele_type = dereference_type (array_type);
		base = new_int_expr (array_type->array.base, false);
	} else if (is_ptr (array_type)) {
		ele_type = array_type->fldptr.type;
		base = new_int_expr (0, false);
	} else {
		ele_type = ev_types[array_type->type];
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
	} else if (is_nonscalar (array_type)) {
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
deref_pointer_expr (const expr_t *pointer)
{
	auto pointer_type = get_type (pointer);

	if (pointer->type == ex_error)
		return pointer;
	if (pointer_type->type != ev_ptr)
		return error (pointer, "not a pointer");
	return unary_expr ('.', pointer);
}

const expr_t *
offset_pointer_expr (const expr_t *pointer, const expr_t *offset)
{
	auto ptr_type = get_type (pointer);
	if (!is_ptr (ptr_type)) {
		internal_error (pointer, "not a pointer");
	}
	if (!is_integral (get_type (offset))) {
		internal_error (offset, "pointer offset is not an integer type");
	}
	const expr_t *ptr;
	if (pointer->type == ex_alias && !pointer->alias.offset
		&& is_integral (get_type (pointer->alias.expr))) {
		ptr = pointer->alias.expr;
	} else if (pointer->type == ex_address && is_constant (offset)) {
		if (pointer->address.offset) {
			offset = binary_expr ('+', pointer->address.offset, offset);
		}
		ptr = new_address_expr (ptr_type->fldptr.type,
								pointer->address.lvalue, offset);
		return ptr;
	} else {
		ptr = cast_expr (&type_int, pointer);
	}
	if (ptr->type == ex_error) {
		return ptr;
	}
	ptr = binary_expr ('+', ptr, offset);
	return cast_expr (ptr_type, ptr);
}

const expr_t *
address_expr (const expr_t *e1, const type_t *t)
{
	expr_t     *e;

	e1 = convert_name (e1);
	if (e1->type == ex_error)
		return e1;

	if (!t)
		t = get_type (e1);

	switch (e1->type) {
		case ex_def:
			{
				auto def = e1->def;
				auto type = def->type;

				//FIXME this test should be in statements.c
				if (options.code.progsversion == PROG_VERSION
					&& (def->local || def->param)) {
					e = new_address_expr (t, e1, 0);
					return e;
				}
				if (is_array (type)) {
					auto ptrval =  new_pointer_val (0, t, def, 0);
					return new_value_expr (ptrval, false);
				} else {
					return new_pointer_expr (0, t, def);
				}
			}
			break;
		case ex_symbol:
			if (e1->symbol->sy_type == sy_var) {
				auto def = e1->symbol->def;
				auto type = def->type;

				//FIXME this test should be in statements.c
				if (options.code.progsversion == PROG_VERSION
					&& (def->local || def->param)) {
					return new_address_expr (t, e1, 0);
				}

				if (is_array (type)) {
					auto ptrval =  new_pointer_val (0, t, def, 0);
					return new_value_expr (ptrval, false);
				} else {
					return new_pointer_expr (0, t, def);
				}
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_expr:
			if (e1->expr.op == '.') {
				e = new_address_expr (e1->expr.type,
									  e1->expr.e1, e1->expr.e2);
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_uexpr:
			if (e1->expr.op == '.') {
				auto p = e1->expr.e1;
				if (p->type == ex_expr && p->expr.op == '.') {
					p = new_address_expr (p->expr.type, p->expr.e1, p->expr.e2);
				}
				return p;
			}
			return error (e1, "invalid type for unary &");
		case ex_label:
			return new_label_ref (&e1->label);
		case ex_temp:
			e = new_address_expr (t, e1, 0);
			break;
		case ex_alias:
			if (!t) {
				t = e1->alias.type;
			}
			return new_address_expr (t, e1, 0);
		default:
			return error (e1, "invalid type for unary &");
	}
	return e;
}

const expr_t *
build_if_statement (int not, const expr_t *test, const expr_t *s1, const expr_t *els, const expr_t *s2)
{
	expr_t     *if_expr;
	expr_t     *tl = new_label_expr ();
	expr_t     *fl = new_label_expr ();

	if (els && !s2) {
		warning (els,
				 "suggest braces around empty body in an ‘else’ statement");
	}
	if (!els && !s1) {
		warning (test,
				 "suggest braces around empty body in an ‘if’ statement");
	}

	auto saved_loc = pr.loc;
	pr.loc = test->loc;

	if_expr = new_block_expr (0);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->boolean.true_list, fl);
			backpatch (test->boolean.false_list, tl);
		} else {
			backpatch (test->boolean.true_list, tl);
			backpatch (test->boolean.false_list, fl);
		}
		append_expr ((expr_t *) test->boolean.e, tl);//FIXME cast
		append_expr (if_expr, test);
	}
	append_expr (if_expr, s1);

	if (els) {
		pr.loc = els->loc;
	}

	if (s2) {
		expr_t     *nl = new_label_expr ();
		append_expr (if_expr, goto_expr (nl));

		append_expr (if_expr, fl);
		append_expr (if_expr, s2);
		append_expr (if_expr, nl);
	} else {
		append_expr (if_expr, fl);
	}

	pr.loc = saved_loc;

	return if_expr;
}

const expr_t *
build_while_statement (int not, const expr_t *test, const expr_t *statement,
					   const expr_t *break_label, const expr_t *continue_label)
{
	const expr_t *l1 = new_label_expr ();
	const expr_t *l2 = break_label;
	expr_t     *while_expr;

	auto saved_loc = pr.loc;
	pr.loc = test->loc;

	while_expr = new_block_expr (0);

	append_expr (while_expr, goto_expr (continue_label));
	append_expr (while_expr, l1);
	append_expr (while_expr, statement);
	append_expr (while_expr, continue_label);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->boolean.true_list, l2);
			backpatch (test->boolean.false_list, l1);
		} else {
			backpatch (test->boolean.true_list, l1);
			backpatch (test->boolean.false_list, l2);
		}
		append_expr ((expr_t *) test->boolean.e, l2);//FIXME cast
		append_expr (while_expr, test);
	}

	pr.loc = saved_loc;

	return while_expr;
}

const expr_t *
build_do_while_statement (const expr_t *statement, int not, const expr_t *test,
						  const expr_t *break_label,
						  const expr_t *continue_label)
{
	expr_t *l1 = new_label_expr ();
	auto        saved_loc = pr.loc;
	expr_t     *do_while_expr;

	if (!statement) {
		warning (break_label,
				 "suggest braces around empty body in a ‘do’ statement");
	}

	pr.loc = test->loc;

	do_while_expr = new_block_expr (0);

	append_expr (do_while_expr, l1);
	append_expr (do_while_expr, statement);
	append_expr (do_while_expr, continue_label);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->boolean.true_list, break_label);
			backpatch (test->boolean.false_list, l1);
		} else {
			backpatch (test->boolean.true_list, l1);
			backpatch (test->boolean.false_list, break_label);
		}
		append_expr ((expr_t *) test->boolean.e, break_label);//FIXME
		append_expr (do_while_expr, test);
	}

	pr.loc = saved_loc;

	return do_while_expr;
}

const expr_t *
build_for_statement (const expr_t *init, const expr_t *test, const expr_t *next,
					 const expr_t *statement, const expr_t *break_label,
					 const expr_t *continue_label)
{
	expr_t     *tl = new_label_expr ();
	const expr_t *fl = break_label;
	expr_t     *l1 = 0;
	const expr_t *t;
	auto        saved_loc = pr.loc;
	expr_t     *for_expr;

	if (next)
		t = next;
	else if (test)
		t = test;
	else if (init)
		t = init;
	else
		t = continue_label;
	pr.loc = t->loc;

	for_expr = new_block_expr (0);

	append_expr (for_expr, init);
	if (test) {
		l1 = new_label_expr ();
		append_expr (for_expr, goto_expr (l1));
	}
	append_expr (for_expr, tl);
	append_expr (for_expr, statement);
	append_expr (for_expr, continue_label);
	append_expr (for_expr, next);
	if (test) {
		append_expr (for_expr, l1);
		test = convert_bool (test, 1);
		if (test->type != ex_error) {
			backpatch (test->boolean.true_list, tl);
			backpatch (test->boolean.false_list, fl);
			append_expr ((expr_t *) test->boolean.e, fl);//FIXME cast
			append_expr (for_expr, test);
		}
	} else {
		append_expr (for_expr, goto_expr (tl));
		append_expr (for_expr, fl);
	}

	pr.loc = saved_loc;

	return for_expr;
}

const expr_t *
build_state_expr (const expr_t *e)
{
	int         count = e ? list_count (&e->list) : 0;
	if (count < 2) {
		return error (e, "not enough state arguments");
	}
	if (count > 3) {
		return error (e, "too many state arguments");
	}
	const expr_t *state_args[3] = {};
	list_scatter (&e->list, state_args);
	const expr_t *frame = state_args[0];
	const expr_t *think = state_args[1];
	const expr_t *step = state_args[2];

	if (think->type == ex_symbol)
		think = think_expr (think->symbol);
	if (is_int_val (frame))
		frame = cast_expr (&type_float, frame);
	if (!type_assignable (&type_float, get_type (frame)))
		return error (frame, "invalid type for frame number");
	if (extract_type (think) != ev_func)
		return error (think, "invalid type for think");
	if (step) {
		if (is_int_val (step))
			step = cast_expr (&type_float, step);
		if (!type_assignable (&type_float, get_type (step)))
			return error (step, "invalid type for step");
	}
	return new_state_expr (frame, think, step);
}

const expr_t *
think_expr (symbol_t *think_sym)
{
	symbol_t   *sym;

	if (think_sym->table)
		return new_symbol_expr (think_sym);

	sym = symtab_lookup (current_symtab, "think");
	if (sym && sym->sy_type == sy_var && sym->type
		&& sym->type->type == ev_field
		&& sym->type->fldptr.type->type == ev_func) {
		think_sym->type = sym->type->fldptr.type;
	} else {
		think_sym->type = &type_func;
	}
	think_sym = function_symbol ((specifier_t) { .sym = think_sym });
	make_function (think_sym, 0, current_symtab->space, current_storage);
	return new_symbol_expr (think_sym);
}

const expr_t *
encode_expr (const type_t *type)
{
	dstring_t  *encoding = dstring_newstr ();

	encode_type (encoding, type);
	auto e = new_string_expr (encoding->str);
	free (encoding);
	return e;
}

const expr_t *
sizeof_expr (const expr_t *expr, const type_t *type)
{
	if (!((!expr) ^ (!type)))
		internal_error (0, 0);
	if (!type)
		type = get_type (expr);
	if (type) {
		expr = new_int_expr (type_aligned_size (type), false);
	}
	return expr;
}
