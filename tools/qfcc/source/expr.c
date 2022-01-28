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

static expr_t *exprs_freelist;

void
convert_name (expr_t *e)
{
	symbol_t   *sym;
	expr_t     *new;

	if (e->type != ex_symbol)
		return;

	sym = e->e.symbol;

	if (!strcmp (sym->name, "__PRETTY_FUNCTION__")
		&& current_func) {
		new = new_string_expr (current_func->name);
		goto convert;
	}
	if (!strcmp (sym->name, "__FUNCTION__")
		&& current_func) {
		new = new_string_expr (current_func->def->name);
		goto convert;
	}
	if (!strcmp (sym->name, "__LINE__")
		&& current_func) {
		new = new_int_expr (e->line);
		goto convert;
	}
	if (!strcmp (sym->name, "__INFINITY__")
		&& current_func) {
		new = new_float_expr (INFINITY);
		goto convert;
	}
	if (!strcmp (sym->name, "__FILE__")
		&& current_func) {
		new = new_string_expr (GETSTR (e->file));
		goto convert;
	}
	if (!sym->table) {
		error (e, "%s undefined", sym->name);
		sym->type = type_default;
		//FIXME need a def
		return;
	}
	if (sym->sy_type == sy_convert) {
		new = sym->s.convert.conv (sym, sym->s.convert.data);
		goto convert;
	}
	if (sym->sy_type == sy_expr) {
		new = copy_expr (sym->s.expr);
		goto convert;
	}
	if (sym->sy_type == sy_type)
		internal_error (e, "unexpected typedef");
	// var, const and func shouldn't need extra handling
	return;
convert:
	e->type = new->type;
	e->e = new->e;
}

expr_t *
convert_vector (expr_t *e)
{
	float       val[4];

	if (e->type != ex_vector)
		return e;
	if (is_vector(e->e.vector.type)) {
		// guaranteed to have three elements
		expr_t     *x = e->e.vector.list;
		expr_t     *y = x->next;
		expr_t     *z = y->next;
		x = fold_constants (cast_expr (&type_float, x));
		y = fold_constants (cast_expr (&type_float, y));
		z = fold_constants (cast_expr (&type_float, z));
		if (is_constant (x) && is_constant (y) && is_constant (z)) {
			val[0] = expr_float(x);
			val[1] = expr_float(y);
			val[2] = expr_float(z);
			return new_vector_expr (val);
		}
		// at least one of x, y, z is not constant, so rebuild the
		// list incase any of them are new expressions
		z->next = 0;
		y->next = z;
		x->next = y;
		e->e.vector.list = x;
		return e;
	}
	if (is_quaternion(e->e.vector.type)) {
		// guaranteed to have two or four elements
		if (e->e.vector.list->next->next) {
			// four vals: x, y, z, w
			expr_t     *x = e->e.vector.list;
			expr_t     *y = x->next;
			expr_t     *z = y->next;
			expr_t     *w = z->next;
			x = fold_constants (cast_expr (&type_float, x));
			y = fold_constants (cast_expr (&type_float, y));
			z = fold_constants (cast_expr (&type_float, z));
			w = fold_constants (cast_expr (&type_float, w));
			if (is_constant (x) && is_constant (y) && is_constant (z)
				&& is_constant (w)) {
				val[0] = expr_float(x);
				val[1] = expr_float(y);
				val[2] = expr_float(z);
				val[3] = expr_float(w);
				return new_quaternion_expr (val);
			}
			// at least one of x, y, z, w is not constant, so rebuild the
			// list incase any of them are new expressions
			w->next = 0;
			z->next = w;
			y->next = z;
			x->next = y;
			e->e.vector.list = x;
			return e;
		} else {
			// v, s
			expr_t     *v = e->e.vector.list;
			expr_t     *s = v->next;

			v = convert_vector (v);
			s = fold_constants (cast_expr (&type_float, s));
			if (is_constant (v) && is_constant (s)) {
				memcpy (val, expr_vector (v), 3 * sizeof (float));
				val[3] = expr_float (s);
				return new_quaternion_expr (val);
			}
			// Either v or s is not constant, so can't convert to a quaternion
			// constant.
			// Rebuild the list in case v or s is a new expression
			// the list will always be v, s
			s->next = 0;
			v->next = s;
			e->e.vector.list = v;
			return e;
		}
	}
	internal_error (e, "bogus vector expression");
}

type_t *
get_type (expr_t *e)
{
	const type_t *type = 0;
	convert_name (e);
	switch (e->type) {
		case ex_branch:
			type = e->e.branch.ret_type;
			break;
		case ex_labelref:
		case ex_adjstk:
		case ex_with:
			return &type_void;
		case ex_memset:
			return e->e.memset.type;
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
			if (e->e.nil) {
				return e->e.nil;
			}
			// fall through
		case ex_state:
			return &type_void;
		case ex_block:
			if (e->e.block.result)
				return get_type (e->e.block.result);
			return &type_void;
		case ex_expr:
		case ex_uexpr:
			type = e->e.expr.type;
			break;
		case ex_def:
			type = e->e.def->type;
			break;
		case ex_symbol:
			type = e->e.symbol->type;
			break;
		case ex_temp:
			type = e->e.temp.type;
			break;
		case ex_value:
			type = e->e.value->type;
			break;
		case ex_vector:
			return e->e.vector.type;
		case ex_selector:
			return &type_SEL;
		case ex_alias:
			type = e->e.alias.type;
			break;
		case ex_address:
			type = e->e.address.type;
			break;
		case ex_assign:
			return get_type (e->e.assign.dst);
		case ex_args:
			return &type_va_list;
		case ex_count:
			internal_error (e, "invalid expression");
	}
	return (type_t *) unalias_type (type);//FIXME cast
}

etype_t
extract_type (expr_t *e)
{
	type_t     *type = get_type (e);

	if (type)
		return type->type;
	return ev_type_count;
}

expr_t *
type_mismatch (expr_t *e1, expr_t *e2, int op)
{
	dstring_t  *t1 = dstring_newstr ();
	dstring_t  *t2 = dstring_newstr ();

	print_type_str (t1, get_type (e1));
	print_type_str (t2, get_type (e2));

	e1 = error (e1, "type mismatch: %s %s %s",
				t1->str, get_op_string (op), t2->str);
	dstring_delete (t1);
	dstring_delete (t2);
	return e1;
}

expr_t *
param_mismatch (expr_t *e, int param, const char *fn, type_t *t1, type_t *t2)
{
	dstring_t  *s1 = dstring_newstr ();
	dstring_t  *s2 = dstring_newstr ();

	print_type_str (s1, t1);
	print_type_str (s2, t2);

	e = error (e, "type mismatch for parameter %d of %s: expected %s, got %s",
			   param, fn, s1->str, s2->str);
	dstring_delete (s1);
	dstring_delete (s2);
	return e;
}

expr_t *
cast_error (expr_t *e, type_t *t1, type_t *t2)
{
	dstring_t  *s1 = dstring_newstr ();
	dstring_t  *s2 = dstring_newstr ();

	print_type_str (s1, t1);
	print_type_str (s2, t2);

	e = error (e, "cannot cast from %s to %s", s1->str, s2->str);
	dstring_delete (s1);
	dstring_delete (s2);
	return e;
}

expr_t *
test_error (expr_t *e, type_t *t)
{
	dstring_t  *s = dstring_newstr ();

	print_type_str (s, t);

	e =  error (e, "%s cannot be tested", s->str);
	dstring_delete (s);
	return e;
}

expr_t *
new_expr (void)
{
	expr_t     *e;

	ALLOC (16384, expr_t, exprs, e);

	e->line = pr.source_line;
	e->file = pr.source_file;
	return e;
}

expr_t *
copy_expr (expr_t *e)
{
	expr_t     *n;
	expr_t     *t;

	if (!e)
		return 0;
	switch (e->type) {
		case ex_error:
		case ex_def:
		case ex_symbol:
		case ex_nil:
		case ex_value:
			// nothing to do here
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			return n;
		case ex_state:
			return new_state_expr (copy_expr (e->e.state.frame),
								   copy_expr (e->e.state.think),
								   copy_expr (e->e.state.step));
		case ex_bool:
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			if (e->e.bool.true_list) {
				int         count = e->e.bool.true_list->size;
				size_t      size = (size_t)&((ex_list_t *) 0)->e[count];
				n->e.bool.true_list = malloc (size);
				while (count--)
					n->e.bool.true_list->e[count] =
						copy_expr (e->e.bool.true_list->e[count]);
			}
			if (e->e.bool.false_list) {
				int         count = e->e.bool.false_list->size;
				size_t      size = (size_t)&((ex_list_t *) 0)->e[count];
				n->e.bool.false_list = malloc (size);
				while (count--)
					n->e.bool.false_list->e[count] =
						copy_expr (e->e.bool.false_list->e[count]);
			}
			n->e.bool.e = copy_expr (e->e.bool.e);
			return n;
		case ex_label:
			/// Create a fresh label
			return new_label_expr ();
		case ex_labelref:
			return new_label_ref (e->e.labelref.label);
		case ex_block:
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			n->e.block.head = 0;
			n->e.block.tail = &n->e.block.head;
			n->e.block.result = 0;
			for (t = e->e.block.head; t; t = t->next) {
				if (t == e->e.block.result) {
					n->e.block.result = copy_expr (t);
					append_expr (n, n->e.block.result);
				} else {
					append_expr (n, copy_expr (t));
				}
			}
			if (e->e.block.result && !n->e.block.result)
				internal_error (e, "bogus block result?");
			break;
		case ex_expr:
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			n->e.expr.e1 = copy_expr (e->e.expr.e1);
			n->e.expr.e2 = copy_expr (e->e.expr.e2);
			return n;
		case ex_uexpr:
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			n->e.expr.e1 = copy_expr (e->e.expr.e1);
			return n;
		case ex_temp:
			n = new_expr ();
			*n = *e;
			n->line = pr.source_line;
			n->file = pr.source_file;
			return n;
		case ex_vector:
			n = new_expr ();
			*n = *e;
			n->e.vector.type = e->e.vector.type;
			n->e.vector.list = copy_expr (e->e.vector.list);
			t = e->e.vector.list;
			e = n->e.vector.list;
			while (t->next) {
				e->next = copy_expr (t->next);
				e = e->next;
				t = t->next;
			}
			return n;
		case ex_selector:
			n = new_expr ();
			*n = *e;
			n->e.selector.sel_ref = copy_expr (e->e.selector.sel_ref);
			return n;
		case ex_compound:
			n = new_expr ();
			*n = *e;
			for (element_t *i = e->e.compound.head; i; i = i->next) {
				append_element (n, new_element (i->expr, i->symbol));
			}
			return n;
		case ex_memset:
			n = new_expr ();
			*n = *e;
			n->e.memset.dst = copy_expr (e->e.memset.dst);
			n->e.memset.val = copy_expr (e->e.memset.val);
			n->e.memset.count = copy_expr (e->e.memset.count);
			return n;
		case ex_alias:
			n = new_expr ();
			*n = *e;
			n->e.alias.expr = copy_expr (e->e.alias.expr);
			n->e.alias.offset = copy_expr (e->e.alias.offset);
			return n;
		case ex_address:
			n = new_expr ();
			*n = *e;
			n->e.address.lvalue = copy_expr (e->e.address.lvalue);
			n->e.address.offset = copy_expr (e->e.address.offset);
			return n;
		case ex_assign:
			n = new_expr ();
			*n = *e;
			n->e.assign.dst = copy_expr (e->e.assign.dst);
			n->e.assign.src = copy_expr (e->e.assign.src);
			return n;
		case ex_branch:
			n = new_expr ();
			*n = *e;
			n->e.branch.target = copy_expr (e->e.branch.target);
			n->e.branch.index = copy_expr (e->e.branch.index);
			n->e.branch.test = copy_expr (e->e.branch.test);
			n->e.branch.args = copy_expr (e->e.branch.args);
			return n;
		case ex_return:
			n = new_expr ();
			*n = *e;
			n->e.retrn.ret_val = copy_expr (e->e.retrn.ret_val);
			return n;
		case ex_adjstk:
			n = new_expr ();
			*n = *e;
			return n;
		case ex_with:
			n = new_expr ();
			*n = *e;
			n->e.with.with = copy_expr (e->e.with.with);
			return n;
		case ex_args:
			n = new_expr ();
			*n = *e;
			return n;
		case ex_count:
			break;
	}
	internal_error (e, "invalid expression");
}

expr_t *
expr_file_line (expr_t *dst, const expr_t *src)
{
	dst->file = src->file;
	dst->line = src->line;
	return dst;
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
new_state_expr (expr_t *frame, expr_t *think, expr_t *step)
{
	expr_t     *s = new_expr ();

	s->type = ex_state;
	s->e.state.frame = frame;
	s->e.state.think = think;
	s->e.state.step = step;
	return s;
}

expr_t *
new_bool_expr (ex_list_t *true_list, ex_list_t *false_list, expr_t *e)
{
	expr_t     *b = new_expr ();

	b->type = ex_bool;
	b->e.bool.true_list = true_list;
	b->e.bool.false_list = false_list;
	b->e.bool.e = e;
	return b;
}

expr_t *
new_label_expr (void)
{

	expr_t     *l = new_expr ();

	l->type = ex_label;
	l->e.label.name = new_label_name ();
	return l;
}

expr_t *
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
		return sym->s.expr;
	}
	l = new_label_expr ();
	l->e.label.name = save_string (va (0, "%s_%s", l->e.label.name,
									   label->name));
	l->e.label.symbol = label;
	label->sy_type = sy_expr;
	label->s.expr = l;
	symtab_addsymbol (current_func->label_scope, label);
	return label->s.expr;
}

expr_t *
new_label_ref (ex_label_t *label)
{

	expr_t     *l = new_expr ();

	l->type = ex_labelref;
	l->e.labelref.label = label;
	label->used++;
	return l;
}

expr_t *
new_block_expr (void)
{
	expr_t     *b = new_expr ();

	b->type = ex_block;
	b->e.block.head = 0;
	b->e.block.tail = &b->e.block.head;
	b->e.block.return_addr = __builtin_return_address (0);
	return b;
}

expr_t *
new_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	expr_t     *e = new_expr ();

	if (e1->type == ex_error)
		return e1;
	if (e2 && e2->type == ex_error)
		return e2;

	e->type = ex_expr;
	e->e.expr.op = op;
	e->e.expr.e1 = e1;
	e->e.expr.e2 = e2;
	return e;
}

expr_t *
build_block_expr (expr_t *expr_list)
{
	expr_t     *b = new_block_expr ();

	while (expr_list) {
		expr_t     *e = expr_list;
		expr_list = e->next;
		e->next = 0;
		append_expr (b, e);
	}
	return b;
}

expr_t *
new_unary_expr (int op, expr_t *e1)
{
	expr_t     *e = new_expr ();

	if (e1 && e1->type == ex_error)
		return e1;

	e->type = ex_uexpr;
	e->e.expr.op = op;
	e->e.expr.e1 = e1;
	return e;
}

expr_t *
new_def_expr (def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_def;
	e->e.def = def;
	return e;
}

expr_t *
new_symbol_expr (symbol_t *symbol)
{
	expr_t     *e = new_expr ();
	e->type = ex_symbol;
	e->e.symbol = symbol;
	return e;
}

expr_t *
new_temp_def_expr (const type_t *type)
{
	expr_t     *e = new_expr ();

	e->type = ex_temp;
	e->e.temp.type = (type_t *) unalias_type (type);	// FIXME cast
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

expr_t *
new_value_expr (ex_value_t *value)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = value;
	return e;
}

expr_t *
new_name_expr (const char *name)
{
	expr_t     *e = new_expr ();
	symbol_t   *sym;

	sym = symtab_lookup (current_symtab, name);
	if (!sym)
		sym = new_symbol (name);
	e->type = ex_symbol;
	e->e.symbol = sym;
	return e;
}

expr_t *
new_string_expr (const char *string_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_string_val (string_val);
	return e;
}

expr_t *
new_double_expr (double double_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_double_val (double_val);
	return e;
}

expr_t *
new_float_expr (float float_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_float_val (float_val);
	return e;
}

expr_t *
new_vector_expr (const float *vector_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_vector_val (vector_val);
	return e;
}

expr_t *
new_vector_list (expr_t *e)
{
	expr_t     *t;
	int         count;
	type_t     *type = &type_vector;
	expr_t     *vec;

	e = reverse_expr_list (e);		// put the elements in the right order
	for (t = e, count = 0; t; t = t->next)
		count++;
	switch (count) {
		case 4:
			type = &type_quaternion;
		case 3:
			// quaternion or vector. all expressions must be compatible with
			// a float (ie, a scalar)
			for (t = e; t; t = t->next) {
				if (t->type == ex_error) {
					return t;
				}
				if (!is_scalar (get_type (t))) {
					return error (t, "invalid type for vector element");
				}
			}
			vec = new_expr ();
			vec->type = ex_vector;
			vec->e.vector.type = type;
			vec->e.vector.list = e;
			break;
		case 2:
			if (e->type == ex_error || e->next->type == ex_error) {
				return e;
			}
			if (is_scalar (get_type (e)) && is_scalar (get_type (e->next))) {
				// scalar, scalar
				// expand [x, y] to [x, y, 0]
				e->next->next = new_float_expr (0);
				vec = new_expr ();
				vec->type = ex_vector;
				vec->e.vector.type = type;
				vec->e.vector.list = e;
				break;
			}
			// quaternion. either scalar, vector or vector, scalar
			if (is_scalar (get_type (e))
				&& is_vector (get_type (e->next))) {
				// scalar, vector
				// swap expressions
				t = e;
				e = e->next;
				e->next = t;
				t->next = 0;
			} else if (is_vector (get_type (e))
					   && is_scalar (get_type (e->next))) {
				// vector, scalar
				// do nothing
			} else {
				return error (t, "invalid types for vector elements");
			}
			// v, s
			vec = new_expr ();
			vec->type = ex_vector;
			vec->e.vector.type = &type_quaternion;
			vec->e.vector.list = e;
			break;
		default:
			return error (e, "invalid number of elements in vector exprssion");
	}
	return vec;
}

expr_t *
new_entity_expr (int entity_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_entity_val (entity_val);
	return e;
}

expr_t *
new_field_expr (int field_val, type_t *type, def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_field_val (field_val, type, def);
	return e;
}

expr_t *
new_func_expr (int func_val, type_t *type)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_func_val (func_val, type);
	return e;
}

expr_t *
new_pointer_expr (int val, type_t *type, def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_pointer_val (val, type, def, 0);
	return e;
}

expr_t *
new_quaternion_expr (const float *quaternion_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_quaternion_val (quaternion_val);
	return e;
}

expr_t *
new_int_expr (int int_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_int_val (int_val);
	return e;
}

expr_t *
new_uint_expr (unsigned uint_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_uint_val (uint_val);
	return e;
}

expr_t *
new_short_expr (short short_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value = new_short_val (short_val);
	return e;
}

int
is_constant (expr_t *e)
{
	while (e->type == ex_alias) {
		e = e->e.alias.expr;
	}
	if (e->type == ex_nil || e->type == ex_value || e->type == ex_labelref
		|| (e->type == ex_symbol && e->e.symbol->sy_type == sy_const)
		|| (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
			&& e->e.symbol->s.def->constant))
		return 1;
	return 0;
}

int
is_selector (expr_t *e)
{
	return e->type == ex_selector;
}

expr_t *
constant_expr (expr_t *e)
{
	expr_t     *new;
	symbol_t   *sym;
	ex_value_t *value;

	if (!is_constant (e))
		return e;
	if (e->type == ex_nil || e->type == ex_value || e->type == ex_labelref)
		return e;
	if (e->type != ex_symbol)
		return e;
	sym = e->e.symbol;
	if (sym->sy_type == sy_const) {
		value = sym->s.value;
	} else if (sym->sy_type == sy_var && sym->s.def->constant) {
		//FIXME pointers and fields
		internal_error (e, "what to do here?");
		//memset (&value, 0, sizeof (value));
		//memcpy (&value.v, &D_INT (sym->s.def),
				//type_size (sym->s.def->type) * sizeof (pr_type_t));
	} else {
		return e;
	}
	new = new_expr ();
	new->type = ex_value;
	new->line = e->line;
	new->file = e->file;
	new->e.value = value;
	return new;
}

int
is_nil (expr_t *e)
{
	return e->type == ex_nil;
}

int
is_string_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value->lltype == ev_string)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_string)
		return 1;
	return 0;
}

const char *
expr_string (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value->lltype == ev_string)
		return e->e.value->v.string_val;
	internal_error (e, "not a string constant");
}

int
is_float_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value->lltype == ev_float)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_float)
		return 1;
	return 0;
}

double
expr_double (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value->lltype == ev_double)
		return e->e.value->v.double_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_double)
		return e->e.symbol->s.value->v.double_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_double (e->e.symbol->s.def->type))
		return D_DOUBLE (e->e.symbol->s.def);
	internal_error (e, "not a double constant");
}

float
expr_float (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value->lltype == ev_float)
		return e->e.value->v.float_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_float)
		return e->e.symbol->s.value->v.float_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_float (e->e.symbol->s.def->type))
		return D_FLOAT (e->e.symbol->s.def);
	internal_error (e, "not a float constant");
}

int
is_vector_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value->lltype == ev_vector)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_vector)
		return 1;
	return 0;
}

const float *
expr_vector (expr_t *e)
{
	if (e->type == ex_nil)
		return vec3_origin;
	if (e->type == ex_value && e->e.value->lltype == ev_vector)
		return e->e.value->v.vector_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_vector)
		return e->e.symbol->s.value->v.vector_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& e->e.symbol->s.def->type->type == ev_vector)
		return D_VECTOR (e->e.symbol->s.def);
	internal_error (e, "not a vector constant");
}

int
is_quaternion_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value->lltype == ev_quaternion)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_quaternion)
		return 1;
	return 0;
}

const float *
expr_quaternion (expr_t *e)
{
	if (e->type == ex_nil)
		return quat_origin;
	if (e->type == ex_value && e->e.value->lltype == ev_quaternion)
		return e->e.value->v.quaternion_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_quaternion)
		return e->e.symbol->s.value->v.quaternion_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& e->e.symbol->s.def->type->type == ev_quaternion)
		return D_QUAT (e->e.symbol->s.def);
	internal_error (e, "not a quaternion constant");
}

int
is_int_val (expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_int) {
		return 1;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& is_integral (e->e.symbol->type)) {
		return 1;
	}
	if (e->type == ex_def && e->e.def->constant
		&& is_integral (e->e.def->type)) {
		return 1;
	}
	return 0;
}

int
expr_int (expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_int) {
		return e->e.value->v.int_val;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_short) {
		return e->e.value->v.short_val;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& (e->e.symbol->type->type == ev_int
			|| is_enum (e->e.symbol->type))) {
		return e->e.symbol->s.value->v.int_val;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_integral (e->e.symbol->s.def->type)) {
		return D_INT (e->e.symbol->s.def);
	}
	if (e->type == ex_def && e->e.def->constant
		&& is_integral (e->e.def->type)) {
		return D_INT (e->e.def);
	}
	internal_error (e, "not an int constant");
}

int
is_uint_val (expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_uint) {
		return 1;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& is_integral (e->e.symbol->type)) {
		return 1;
	}
	if (e->type == ex_def && e->e.def->constant
		&& is_integral (e->e.def->type)) {
		return 1;
	}
	return 0;
}

unsigned
expr_uint (expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_uint) {
		return e->e.value->v.uint_val;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_uint) {
		return e->e.symbol->s.value->v.uint_val;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_integral (e->e.symbol->s.def->type)) {
		return D_INT (e->e.symbol->s.def);
	}
	if (e->type == ex_def && e->e.def->constant
		&& is_integral (e->e.def->type)) {
		return D_INT (e->e.def);
	}
	internal_error (e, "not an unsigned constant");
}

int
is_short_val (expr_t *e)
{
	if (e->type == ex_nil) {
		return 1;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_short) {
		return 1;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_short) {
		return 1;
	}
	return 0;
}

short
expr_short (expr_t *e)
{
	if (e->type == ex_nil) {
		return 0;
	}
	if (e->type == ex_value && e->e.value->lltype == ev_short) {
		return e->e.value->v.short_val;
	}
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_short) {
		return e->e.symbol->s.value->v.short_val;
	}
	internal_error (e, "not a short constant");
}

int
is_integral_val (expr_t *e)
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
	}
	return 0;
}

int
expr_integral (expr_t *e)
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
	}
	internal_error (e, "not an integral constant");
}

int
is_pointer_val (expr_t *e)
{
	if (e->type == ex_value && e->e.value->lltype == ev_ptr) {
		return 1;
	}
	return 0;
}

expr_t *
new_alias_expr (type_t *type, expr_t *expr)
{
	if (expr->type == ex_alias) {
		if (expr->e.alias.offset) {
			return new_offset_alias_expr (type, expr, 0);
		}
		expr = expr->e.alias.expr;
	}
	// this can happen when constant folding an offset pointer results in
	// a noop due to the offset being 0 and thus casting back to the original
	// type
	if (type == get_type (expr)) {
		return expr;
	}

	expr_t     *alias = new_expr ();
	alias->type = ex_alias;
	alias->e.alias.type = type;
	alias->e.alias.expr = expr;
	alias->file = expr->file;
	alias->line = expr->line;
	return alias;
}

expr_t *
new_offset_alias_expr (type_t *type, expr_t *expr, int offset)
{
	if (expr->type == ex_alias && expr->e.alias.offset) {
		expr_t     *ofs_expr = expr->e.alias.offset;
		if (!is_constant (ofs_expr)) {
			internal_error (ofs_expr, "non-constant offset for alias expr");
		}
		offset += expr_int (ofs_expr);

		if (expr->e.alias.expr->type == ex_alias) {
			internal_error (expr, "alias expr of alias expr");
		}
		expr = expr->e.alias.expr;
	}

	expr_t     *alias = new_expr ();
	alias->type = ex_alias;
	alias->e.alias.type = type;
	alias->e.alias.expr = expr;
	alias->e.alias.offset = new_int_expr (offset);
	alias->file = expr->file;
	alias->line = expr->line;
	return alias;
}

expr_t *
new_address_expr (type_t *lvtype, expr_t *lvalue, expr_t *offset)
{
	expr_t     *addr = new_expr ();
	addr->type = ex_address;
	addr->e.address.type = pointer_type (lvtype);
	addr->e.address.lvalue = lvalue;
	addr->e.address.offset = offset;
	return addr;
}

expr_t *
new_assign_expr (expr_t *dst, expr_t *src)
{
	expr_t     *addr = new_expr ();
	addr->type = ex_assign;
	addr->e.assign.dst = dst;
	addr->e.assign.src = src;
	return addr;
}

expr_t *
new_return_expr (expr_t *ret_val)
{
	expr_t     *retrn = new_expr ();
	retrn->type = ex_return;
	retrn->e.retrn.ret_val = ret_val;
	return retrn;
}

expr_t *
new_adjstk_expr (int mode, int offset)
{
	expr_t     *adj = new_expr ();
	adj->type = ex_adjstk;
	adj->e.adjstk.mode = mode;
	adj->e.adjstk.offset = offset;
	return adj;
}

expr_t *
new_with_expr (int mode, int reg, expr_t *val)
{
	expr_t     *with = new_expr ();
	with->type = ex_with;
	with->e.with.mode = mode;
	with->e.with.reg = reg;
	with->e.with.with = val;
	return with;
}

static expr_t *
param_expr (const char *name, type_t *type)
{
	symbol_t   *sym;
	expr_t     *sym_expr;

	sym = make_symbol (name, &type_param, pr.symtab->space, sc_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	sym_expr = new_symbol_expr (sym);
	return new_alias_expr (type, sym_expr);
}

expr_t *
new_ret_expr (type_t *type)
{
	return param_expr (".return", type);
}

expr_t *
new_param_expr (type_t *type, int num)
{
	return param_expr (va (0, ".param_%d", num), type);
}

expr_t *
append_expr (expr_t *block, expr_t *e)
{
	if (block->type != ex_block)
		internal_error (block, "not a block expression");

	if (!e || e->type == ex_error)
		return block;

	if (e->next)
		internal_error (e, "append_expr: expr loop detected");

	*block->e.block.tail = e;
	block->e.block.tail = &e->next;

	return block;
}

expr_t *
prepend_expr (expr_t *block, expr_t *e)
{
	if (block->type != ex_block)
		internal_error (block, "not a block expression");

	if (!e || e->type == ex_error)
		return block;

	if (e->next)
		internal_error (e, "append_expr: expr loop detected");

	e->next = block->e.block.head;
	block->e.block.head = e;

	if (block->e.block.tail == &block->e.block.head) {
		block->e.block.tail = &e->next;
	}

	return block;
}

static symbol_t *
get_struct_field (const type_t *t1, expr_t *e1, expr_t *e2)
{
	symtab_t   *strct = t1->t.symtab;
	symbol_t   *sym = e2->e.symbol;//FIXME need to check
	symbol_t   *field;

	if (!strct) {
		error (e1, "dereferencing pointer to incomplete type");
		return 0;
	}
	field = symtab_lookup (strct, sym->name);
	if (!field && !is_entity(t1)) {
		error (e2, "'%s' has no member named '%s'", t1->name + 4, sym->name);
		e1->type = ex_error;
	}
	return field;
}

expr_t *
field_expr (expr_t *e1, expr_t *e2)
{
	const type_t *t1, *t2;
	expr_t     *e;

	t1 = get_type (e1);
	if (e1->type == ex_error)
		return e1;
	if (is_entity (t1)) {
		symbol_t   *field = 0;

		if (e2->type == ex_symbol)
			field = get_struct_field (&type_entity, e1, e2);
		if (field) {
			e2 = new_field_expr (0, field->type, field->s.def);
			e = new_binary_expr ('.', e1, e2);
			e->e.expr.type = field->type;
			return e;
		} else {
			t2 = get_type (e2);
			if (e2->type == ex_error)
				return e2;
			if (t2->type == ev_field) {
				e = new_binary_expr ('.', e1, e2);
				e->e.expr.type = t2->t.fldptr.type;
				return e;
			}
		}
	} else if (is_ptr (t1)) {
		if (is_struct (t1->t.fldptr.type)) {
			symbol_t   *field;

			field = get_struct_field (t1->t.fldptr.type, e1, e2);
			if (!field)
				return e1;

			expr_t     *offset = new_short_expr (field->s.offset);
			e1 = offset_pointer_expr (e1, offset);
			e1 = cast_expr (pointer_type (field->type), e1);
			return unary_expr ('.', e1);
		} else if (is_class (t1->t.fldptr.type)) {
			class_t    *class = t1->t.fldptr.type->t.class;
			symbol_t   *sym = e2->e.symbol;//FIXME need to check
			symbol_t   *ivar;
			int         protected = class_access (current_class, class);

			ivar = class_find_ivar (class, protected, sym->name);
			if (!ivar)
				return new_error_expr ();
			expr_t     *offset = new_short_expr (ivar->s.offset);
			e1 = offset_pointer_expr (e1, offset);
			e1 = cast_expr (pointer_type (ivar->type), e1);
			return unary_expr ('.', e1);
		}
	} else if (is_vector (t1) || is_quaternion (t1) || is_struct (t1)) {
		symbol_t   *field;

		field = get_struct_field (t1, e1, e2);
		if (!field)
			return e1;

		if (e1->type == ex_expr && e1->e.expr.op == '.'
			&& is_entity(get_type (e1->e.expr.e1))) {
			// undo the . expression
			e2 = e1->e.expr.e2;
			e1 = e1->e.expr.e1;
			// offset the field expresion
			if (e2->type == ex_symbol) {
				symbol_t   *sym;
				def_t      *def;
				sym = symtab_lookup (pr.entity_fields, e2->e.symbol->name);
				if (!sym) {
					internal_error (e2, "failed to find entity field %s",
									e2->e.symbol->name);
				}
				def = sym->s.def;
				e2 = new_field_expr (0, field->type, def);
			} else if (e2->type != ex_value
					   || e2->e.value->lltype != ev_field) {
				internal_error (e2, "unexpected field exression");
			}
			e2->e.value = new_field_val (e2->e.value->v.pointer.val + field->s.offset, field->type, e2->e.value->v.pointer.def);
			// create a new . expression
			return field_expr (e1, e2);
		} else {
			if (e1->type == ex_uexpr && e1->e.expr.op == '.') {
				expr_t     *offset = new_short_expr (field->s.offset);
				e1 = offset_pointer_expr (e1->e.expr.e1, offset);
				e1 = cast_expr (pointer_type (field->type), e1);
				return unary_expr ('.', e1);
			} else {
				return new_offset_alias_expr (field->type, e1, field->s.offset);
			}
		}
	} else if (is_class (t1)) {
		//Class instance variables aren't allowed and thus declaring one
		//is treated as an error, so this is a follow-on error.
		return error (e1, "class instance access");
	}
	return type_mismatch (e1, e2, '.');
}

expr_t *
convert_from_bool (expr_t *e, type_t *type)
{
	expr_t     *zero;
	expr_t     *one;
	expr_t     *cond;

	if (is_float (type)) {
		one = new_float_expr (1);
		zero = new_float_expr (0);
	} else if (is_int (type)) {
		one = new_int_expr (1);
		zero = new_int_expr (0);
	} else if (is_enum (type) && enum_as_bool (type, &zero, &one)) {
		// don't need to do anything
	} else if (is_uint (type)) {
		one = new_uint_expr (1);
		zero = new_uint_expr (0);
	} else {
		return error (e, "can't convert from bool value");
	}
	cond = new_expr ();
	*cond = *e;
	cond->next = 0;

	cond = conditional_expr (cond, one, zero);
	e->type = cond->type;
	e->e = cond->e;
	return e;
}

void
convert_int (expr_t *e)
{
	float       float_val = expr_int (e);
	e->type = ex_value;
	e->e.value = new_float_val (float_val);
}

void
convert_short (expr_t *e)
{
	float       float_val = expr_short (e);
	e->type = ex_value;
	e->e.value = new_float_val (float_val);
}

void
convert_short_int (expr_t *e)
{
	float       int_val = expr_short (e);
	e->type = ex_value;
	e->e.value = new_int_val (int_val);
}

void
convert_double (expr_t *e)
{
	float       float_val = expr_double (e);
	e->type = ex_value;
	e->e.value = new_float_val (float_val);
}

expr_t *
convert_nil (expr_t *e, type_t *t)
{
	e->e.nil = t;
	return e;
}

int
is_compare (int op)
{
	if (op == EQ || op == NE || op == LE || op == GE || op == LT || op == GT
		|| op == '>' || op == '<')
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
	if (op == OR || op == AND)
		return 1;
	return 0;
}

int
has_function_call (expr_t *e)
{
	switch (e->type) {
		case ex_bool:
			return has_function_call (e->e.bool.e);
		case ex_block:
			if (e->e.block.is_call)
				return 1;
			for (e = e->e.block.head; e; e = e->next)
				if (has_function_call (e))
					return 1;
			return 0;
		case ex_expr:
			return (has_function_call (e->e.expr.e1)
					|| has_function_call (e->e.expr.e2));
		case ex_uexpr:
			return has_function_call (e->e.expr.e1);
		case ex_alias:
			return has_function_call (e->e.alias.expr);
		case ex_address:
			return has_function_call (e->e.address.lvalue);
		case ex_assign:
			return (has_function_call (e->e.assign.dst)
					|| has_function_call (e->e.assign.src));
		case ex_branch:
			if (e->e.branch.type == pr_branch_call) {
				return 1;
			}
			if (e->e.branch.type == pr_branch_jump) {
				return 0;
			}
			return has_function_call (e->e.branch.test);
		case ex_return:
			return has_function_call (e->e.retrn.ret_val);
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
			return 0;
		case ex_count:
			break;
	}
	internal_error (e, "invalid expression type");
}

int
is_function_call (expr_t *e)
{
	return e->type == ex_branch && e->e.branch.type == pr_branch_call;
}

expr_t *
asx_expr (int op, expr_t *e1, expr_t *e2)
{
	if (e1->type == ex_error)
		return e1;
	else if (e2->type == ex_error)
		return e2;
	else {
		expr_t     *e = new_expr ();

		*e = *e1;
		e2->paren = 1;
		return assign_expr (e, binary_expr (op, e1, e2));
	}
}

expr_t *
unary_expr (int op, expr_t *e)
{
	vec3_t      v;
	quat_t      q;
	const char *s;
	expr_t     *new;
	type_t     *t;

	convert_name (e);
	if (e->type == ex_error)
		return e;
	switch (op) {
		case '-':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary -");
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, "type check failed!");
					case ev_double:
						new = new_double_expr (-expr_double (e));
						new->implicit = e->implicit;
						return new;
					case ev_float:
						return new_float_expr (-expr_float (e));
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
						return new_int_expr (-expr_int (e));
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
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->e.expr.op == '-') {
						return e->e.expr.e1;
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = get_type (e);
						return n;
					}
				case ex_block:
					if (!e->e.block.result) {
						return error (e, "invalid type for unary -");
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = get_type (e);
						return n;
					}
				case ex_branch:
					return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_bool:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = get_type (e);
						return n;
					}
				case ex_def:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = e->e.def->type;
						return n;
					}
				case ex_symbol:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = e->e.symbol->type;
						return n;
					}
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary -");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '!':
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, 0);
					case ev_string:
						s = expr_string (e);
						return new_int_expr (!s || !s[0]);
					case ev_double:
						return new_int_expr (!expr_double (e));
					case ev_float:
						return new_int_expr (!expr_float (e));
					case ev_vector:
						return new_int_expr (!VectorIsZero (expr_vector (e)));
					case ev_quaternion:
						return new_int_expr (!QuatIsZero (expr_quaternion (e)));
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (!expr_int (e));
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
					internal_error (e, "unexpected expression type");
				case ex_bool:
					return new_bool_expr (e->e.bool.false_list,
										  e->e.bool.true_list, e);
				case ex_block:
					if (!e->e.block.result)
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
					{
						expr_t     *n = new_unary_expr (op, e);

						if (options.code.progsversion > PROG_ID_VERSION)
							n->e.expr.type = &type_int;
						else
							n->e.expr.type = &type_float;
						return n;
					}
				case ex_branch:
				case ex_nil:
					return error (e, "invalid type for unary !");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '~':
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
						return new_float_expr (~(int) expr_float (e));
					case ev_quaternion:
						QuatConj (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (~expr_int (e));
					case ev_uint:
						return new_uint_expr (~expr_uint (e));
					case ev_short:
						return new_short_expr (~expr_short (e));
					case ev_invalid:
						t = get_type (e);
						if (t->meta == ty_enum) {
							return new_int_expr (~expr_int (e));
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
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->e.expr.op == '~')
						return e->e.expr.e1;
					goto bitnot_expr;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary ~");
					goto bitnot_expr;
				case ex_branch:
					return error (e, "invalid type for unary ~");
				case ex_expr:
				case ex_bool:
				case ex_def:
				case ex_symbol:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
bitnot_expr:
					if (options.code.progsversion == PROG_ID_VERSION) {
						expr_t     *n1 = new_int_expr (-1);
						return binary_expr ('-', n1, e);
					} else {
						expr_t     *n = new_unary_expr (op, e);
						type_t     *t = get_type (e);

						if (!is_int(t) && !is_float(t)
							&& !is_quaternion(t))
							return error (e, "invalid type for unary ~");
						n->e.expr.type = t;
						return n;
					}
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary ~");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '.':
			if (extract_type (e) != ev_ptr)
				return error (e, "invalid type for unary .");
			e = new_unary_expr ('.', e);
			e->e.expr.type = get_type (e->e.expr.e1)->t.fldptr.type;
			return e;
		case '+':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary +");
			return e;
	}
	internal_error (e, 0);
}

expr_t *
build_function_call (expr_t *fexpr, const type_t *ftype, expr_t *params)
{
	expr_t     *e;
	expr_t     *p;
	int         arg_count = 0, param_count = 0;
	int         i;
	expr_t     *args = 0, **a = &args;
	type_t     *arg_types[PR_MAX_PARAMS];
	expr_t     *arg_exprs[PR_MAX_PARAMS][2];
	int         arg_expr_count = 0;
	int         emit_args = 0;
	expr_t     *assign;
	expr_t     *call;
	expr_t     *err = 0;

	for (e = params; e; e = e->next) {
		if (e->type == ex_error)
			return e;
		arg_count++;
	}

	if (arg_count > PR_MAX_PARAMS) {
		return error (fexpr, "more than %d parameters", PR_MAX_PARAMS);
	}
	if (ftype->t.func.num_params < -1) {
		if (-arg_count > ftype->t.func.num_params + 1) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = -ftype->t.func.num_params - 1;
		emit_args = 1;
	} else if (ftype->t.func.num_params >= 0) {
		if (arg_count > ftype->t.func.num_params) {
			return error (fexpr, "too many arguments");
		} else if (arg_count < ftype->t.func.num_params) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = ftype->t.func.num_params;
	}
	// params is reversed (a, b, c) -> c, b, a
	for (i = arg_count - 1, e = params; i >= 0; i--, e = e->next) {
		type_t     *t;

		if (e->type == ex_compound) {
			if (i < param_count) {
				t = ftype->t.func.param_types[i];
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

		if (!type_size (t))
			err = error (e, "type of formal parameter %d is incomplete",
						 i + 1);
		if (type_size (t) > type_size (&type_param))
			err = error (e, "formal parameter %d is too large to be passed by"
						 " value", i + 1);
		if (i < param_count) {
			if (e->type == ex_nil)
				convert_nil (e, t = ftype->t.func.param_types[i]);
			if (e->type == ex_bool)
				convert_from_bool (e, ftype->t.func.param_types[i]);
			if (e->type == ex_error)
				return e;
			if (!type_assignable (ftype->t.func.param_types[i], t)) {
				err = param_mismatch (e, i + 1, fexpr->e.symbol->name,
									  ftype->t.func.param_types[i], t);
			}
			t = ftype->t.func.param_types[i];
		} else {
			if (e->type == ex_nil)
				convert_nil (e, t = type_nil);
			if (e->type == ex_bool)
				convert_from_bool (e, get_type (e));
			if (is_int_val (e)
				&& options.code.progsversion == PROG_ID_VERSION)
				convert_int (e);
			if (options.code.promote_float) {
				if (is_float (get_type (e))) {
					t = &type_double;
				}
			} else {
				if (is_double (get_type (e))) {
					if (!e->implicit) {
						warning (e, "passing double into ... function");
					}
					if (is_constant (e)) {
						// don't auto-demote non-constant doubles
						t = &type_float;
					}
				}
			}
			if (is_int_val (e) && options.warnings.vararg_integer)
				warning (e, "passing int constant into ... function");
		}
		arg_types[arg_count - 1 - i] = t;
	}
	if (err)
		return err;

	call = expr_file_line (new_block_expr (), fexpr);
	call->e.block.is_call = 1;
	// args is built in reverse order so it matches params
	for (p = params, i = 0; p; p = p->next, i++) {
		if (emit_args && arg_count - i == param_count) {
			emit_args = 0;
			*a = new_args_expr ();
			a = &(*a)->next;
		}
		expr_t     *e = p;
		if (e->type == ex_compound) {
			e = expr_file_line (initialized_temp_expr (arg_types[i], e), e);
		}
		// FIXME this is target-specific info and should not be in the
		// expression tree
		// That, or always use a temp, since it should get optimized out
		if (has_function_call (e)) {
			expr_t     *cast = cast_expr (arg_types[i], convert_vector (e));
			expr_t     *tmp = new_temp_def_expr (arg_types[i]);
			*a = expr_file_line (tmp, e);
			arg_exprs[arg_expr_count][0] = expr_file_line (cast, e);
			arg_exprs[arg_expr_count][1] = *a;
			arg_expr_count++;
		} else {
			*a = expr_file_line (cast_expr (arg_types[i], convert_vector (e)),
								 e);
		}
		a = &(*a)->next;
	}
	if (emit_args) {
		emit_args = 0;
		*a = new_args_expr ();
		a = &(*a)->next;
	}
	for (i = 0; i < arg_expr_count - 1; i++) {
		assign = assign_expr (arg_exprs[i][1], arg_exprs[i][0]);
		append_expr (call, expr_file_line (assign, arg_exprs[i][0]));
	}
	if (arg_expr_count) {
		e = assign_expr (arg_exprs[arg_expr_count - 1][1],
						 arg_exprs[arg_expr_count - 1][0]);
		e = expr_file_line (e, arg_exprs[arg_expr_count - 1][0]);
		append_expr (call, e);
	}
	e = expr_file_line (call_expr (fexpr, args, ftype->t.func.type), fexpr);
	call->e.block.result = e;
	return call;
}

expr_t *
function_expr (expr_t *fexpr, expr_t *params)
{
	type_t     *ftype;

	find_function (fexpr, params);
	ftype = get_type (fexpr);

	if (fexpr->type == ex_error)
		return fexpr;
	if (ftype->type != ev_func) {
		if (fexpr->type == ex_symbol)
			return error (fexpr, "Called object \"%s\" is not a function",
						  fexpr->e.symbol->name);
		else
			return error (fexpr, "Called object is not a function");
	}

	if (fexpr->type == ex_symbol && params && is_string_val (params)) {
		// FIXME eww, I hate this, but it's needed :(
		// FIXME make a qc hook? :)
		if (strncmp (fexpr->e.symbol->name, "precache_sound", 14) == 0)
			PrecacheSound (expr_string (params), fexpr->e.symbol->name[14]);
		else if (strncmp (fexpr->e.symbol->name, "precache_model", 14) == 0)
			PrecacheModel (expr_string (params), fexpr->e.symbol->name[14]);
		else if (strncmp (fexpr->e.symbol->name, "precache_file", 13) == 0)
			PrecacheFile (expr_string (params), fexpr->e.symbol->name[13]);
	}

	return build_function_call (fexpr, ftype, params);
}

expr_t *
branch_expr (int op, expr_t *test, expr_t *label)
{
	// need to translated op due to precedence rules dictating the layout
	// of the token ids
	static pr_branch_e branch_type [] = {
		pr_branch_eq,
		pr_branch_ne,
		pr_branch_lt,
		pr_branch_gt,
		pr_branch_le,
		pr_branch_ge,
	};
	if (op < EQ || op > LE) {
		internal_error (label, "invalid op: %d", op);
	}
	if (label && label->type != ex_label) {
		internal_error (label, "not a label");
	}
	if (label) {
		label->e.label.used++;
	}
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->e.branch.type = branch_type[op - EQ];
	branch->e.branch.target = label;
	branch->e.branch.test = test;
	return branch;
}

expr_t *
goto_expr (expr_t *label)
{
	if (label && label->type != ex_label) {
		internal_error (label, "not a label");
	}
	if (label) {
		label->e.label.used++;
	}
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->e.branch.type = pr_branch_jump;
	branch->e.branch.target = label;
	return branch;
}

expr_t *
jump_table_expr (expr_t *table, expr_t *index)
{
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->e.branch.type = pr_branch_jump;
	branch->e.branch.target = table;//FIXME separate? all branch types can
	branch->e.branch.index = index;
	return branch;
}

expr_t *
call_expr (expr_t *func, expr_t *args, type_t *ret_type)
{
	expr_t     *branch = new_expr ();
	branch->type = ex_branch;
	branch->e.branch.type = pr_branch_call;
	branch->e.branch.target = func;
	branch->e.branch.args = args;
	branch->e.branch.ret_type = ret_type;
	return branch;
}

expr_t *
return_expr (function_t *f, expr_t *e)
{
	const type_t *t;
	const type_t *ret_type = unalias_type (f->type->t.func.type);

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

	if (e->type == ex_compound) {
		e = expr_file_line (initialized_temp_expr (ret_type, e), e);
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
		e = convert_from_bool (e, (type_t *) ret_type); //FIXME cast
	}
	if (is_float(ret_type) && is_int_val (e)) {
		convert_int (e);
		t = &type_float;
	}
	if (is_void(t)) {
		if (e->type == ex_nil) {
			t = ret_type;
			convert_nil (e, (type_t *) t);//FIXME cast
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
			return error (e, "type mismatch for return value of %s",
						  f->sym->name);
		if (options.warnings.traditional)
			warning (e, "type mismatch for return value of %s",
					 f->sym->name);
	} else {
		if (ret_type != t) {
			e = cast_expr ((type_t *) ret_type, e);//FIXME cast
			t = f->sym->type->t.func.type;
		}
	}
	if (e->type == ex_vector) {
		e = assign_expr (new_temp_def_expr (t), e);
	}
	if (e->type == ex_block) {
		e->e.block.result->rvalue = 1;
	}
	return new_return_expr (e);
}

expr_t *
conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2)
{
	expr_t     *block = new_block_expr ();
	type_t     *type1 = get_type (e1);
	type_t     *type2 = get_type (e2);
	expr_t     *tlabel = new_label_expr ();
	expr_t     *flabel = new_label_expr ();
	expr_t     *elabel = new_label_expr ();

	if (cond->type == ex_error)
		return cond;
	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;

	cond = convert_bool (cond, 1);
	if (cond->type == ex_error)
		return cond;

	backpatch (cond->e.bool.true_list, tlabel);
	backpatch (cond->e.bool.false_list, flabel);

	block->e.block.result = (type1 == type2) ? new_temp_def_expr (type1) : 0;
	append_expr (block, cond);
	append_expr (cond->e.bool.e, flabel);
	if (block->e.block.result)
		append_expr (block, assign_expr (block->e.block.result, e2));
	else
		append_expr (block, e2);
	append_expr (block, goto_expr (elabel));
	append_expr (block, tlabel);
	if (block->e.block.result)
		append_expr (block, assign_expr (block->e.block.result, e1));
	else
		append_expr (block, e1);
	append_expr (block, elabel);
	return block;
}

expr_t *
incop_expr (int op, expr_t *e, int postop)
{
	expr_t     *one;

	if (e->type == ex_error)
		return e;

	one = new_int_expr (1);			// int constants get auto-cast to float
	if (postop) {
		expr_t     *t1, *t2;
		type_t     *type = get_type (e);
		expr_t     *block = new_block_expr ();
		expr_t     *res = new_expr ();

		if (e->type == ex_error)	// get_type failed
			return e;
		t1 = new_temp_def_expr (type);
		t2 = new_temp_def_expr (type);
		append_expr (block, assign_expr (t1, e));
		append_expr (block, assign_expr (t2, binary_expr (op, t1, one)));
		res = copy_expr (e);
		if (res->type == ex_uexpr && res->e.expr.op == '.')
			res = deref_pointer_expr (address_expr (res, 0));
		append_expr (block, assign_expr (res, t2));
		block->e.block.result = t1;
		return block;
	} else {
		return asx_expr (op, e, one);
	}
}

expr_t *
array_expr (expr_t *array, expr_t *index)
{
	type_t     *array_type = get_type (array);
	type_t     *index_type = get_type (index);
	expr_t     *scale;
	expr_t     *offset;
	expr_t     *base;
	expr_t     *ptr;
	expr_t     *e;
	int         ind = 0;

	if (array->type == ex_error)
		return array;
	if (index->type == ex_error)
		return index;

	if (!is_ptr (array_type) && !is_array (array_type))
		return error (array, "not an array");
	if (!is_integral (index_type))
		return error (index, "invalid array index type");
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_int_val (index))
		ind = expr_int (index);
	if (is_array (array_type)
		&& array_type->t.array.size
		&& is_constant (index)
		&& (ind < array_type->t.array.base
			|| ind - array_type->t.array.base >= array_type->t.array.size))
			return error (index, "array index out of bounds");
	scale = new_int_expr (type_size (array_type->t.array.type));
	index = binary_expr ('*', index, scale);
	base = new_int_expr (array_type->t.array.base);
	offset = binary_expr ('*', base, scale);
	index = binary_expr ('-', index, offset);
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_int_val (index))
		ind = expr_int (index);
	if (is_array (array_type)) {
		type_t     *element_type = array_type->t.array.type;
		if (array->type == ex_uexpr && array->e.expr.op == '.') {
			ptr = array->e.expr.e1;
		} else {
			expr_t     *alias = new_offset_alias_expr (element_type, array, 0);
			ptr = new_address_expr (element_type, alias, 0);
		}
	} else {
		ptr = array;
	}
	ptr = offset_pointer_expr (ptr, index);
	ptr = cast_expr (pointer_type (array_type->t.array.type), ptr);

	e = unary_expr ('.', ptr);
	return e;
}

expr_t *
deref_pointer_expr (expr_t *pointer)
{
	type_t     *pointer_type = get_type (pointer);

	if (pointer->type == ex_error)
		return pointer;
	if (pointer_type->type != ev_ptr)
		return error (pointer, "not a pointer");
	return unary_expr ('.', pointer);
}

expr_t *
offset_pointer_expr (expr_t *pointer, expr_t *offset)
{
	type_t     *ptr_type = get_type (pointer);
	if (!is_ptr (ptr_type)) {
		internal_error (pointer, "not a pointer");
	}
	if (!is_integral (get_type (offset))) {
		internal_error (offset, "pointer offset is not an integer type");
	}
	expr_t     *ptr;
	if (pointer->type == ex_alias && !pointer->e.alias.offset
		&& is_integral (get_type (pointer->e.alias.expr))) {
		ptr = pointer->e.alias.expr;
	} else {
		ptr = cast_expr (&type_int, pointer);
	}
	ptr = binary_expr ('+', ptr, offset);
	return cast_expr (ptr_type, ptr);
}

expr_t *
address_expr (expr_t *e1, type_t *t)
{
	expr_t     *e;

	if (e1->type == ex_error)
		return e1;

	if (!t)
		t = get_type (e1);

	switch (e1->type) {
		case ex_def:
			{
				def_t      *def = e1->e.def;
				type_t     *type = def->type;

				//FIXME this test should be in statements.c
				if (options.code.progsversion == PROG_VERSION
					&& (def->local || def->param)) {
					e = new_address_expr (t, e1, 0);
					return e;
				}
				if (is_array (type)) {
					e = e1;
					e->type = ex_value;
					e->e.value = new_pointer_val (0, t, def, 0);
				} else {
					e = new_pointer_expr (0, t, def);
					e->line = e1->line;
					e->file = e1->file;
				}
			}
			break;
		case ex_symbol:
			if (e1->e.symbol->sy_type == sy_var) {
				def_t      *def = e1->e.symbol->s.def;
				type_t     *type = def->type;

				//FIXME this test should be in statements.c
				if (options.code.progsversion == PROG_VERSION
					&& (def->local || def->param)) {
					e = new_address_expr (t, e1, 0);
					return e;
				}

				if (is_array (type)) {
					e = e1;
					e->type = ex_value;
					e->e.value = new_pointer_val (0, t, def, 0);
				} else {
					e = new_pointer_expr (0, t, def);
					e->line = e1->line;
					e->file = e1->file;
				}
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_expr:
			if (e1->e.expr.op == '.') {
				e = new_address_expr (e1->e.expr.type,
									  e1->e.expr.e1, e1->e.expr.e2);
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_uexpr:
			if (e1->e.expr.op == '.') {
				e = e1->e.expr.e1;
				if (e->type == ex_expr && e->e.expr.op == '.') {
					e = new_address_expr (e->e.expr.type, e->e.expr.e1, e->e.expr.e2);
				}
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_label:
			return new_label_ref (&e1->e.label);
		case ex_temp:
			e = new_address_expr (t, e1, 0);
			break;
		case ex_alias:
			if (!t) {
				t = e1->e.alias.type;
			}
			return new_address_expr (t, e1, 0);
		default:
			return error (e1, "invalid type for unary &");
	}
	return e;
}

expr_t *
build_if_statement (int not, expr_t *test, expr_t *s1, expr_t *els, expr_t *s2)
{
	int         line = pr.source_line;
	pr_string_t file = pr.source_file;
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
	pr.source_line = test->line;
	pr.source_file = test->file;

	if_expr = new_block_expr ();

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->e.bool.true_list, fl);
			backpatch (test->e.bool.false_list, tl);
		} else {
			backpatch (test->e.bool.true_list, tl);
			backpatch (test->e.bool.false_list, fl);
		}
		append_expr (test->e.bool.e, tl);
		append_expr (if_expr, test);
	}
	append_expr (if_expr, s1);

	if (els) {
		pr.source_line = els->line;
		pr.source_file = els->file;
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

	pr.source_line = line;
	pr.source_file = file;

	return if_expr;
}

expr_t *
build_while_statement (int not, expr_t *test, expr_t *statement,
					   expr_t *break_label, expr_t *continue_label)
{
	int         line = pr.source_line;
	pr_string_t file = pr.source_file;
	expr_t     *l1 = new_label_expr ();
	expr_t     *l2 = break_label;
	expr_t     *while_expr;

	pr.source_line = test->line;
	pr.source_file = test->file;

	while_expr = new_block_expr ();

	append_expr (while_expr, goto_expr (continue_label));
	append_expr (while_expr, l1);
	append_expr (while_expr, statement);
	append_expr (while_expr, continue_label);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->e.bool.true_list, l2);
			backpatch (test->e.bool.false_list, l1);
		} else {
			backpatch (test->e.bool.true_list, l1);
			backpatch (test->e.bool.false_list, l2);
		}
		append_expr (test->e.bool.e, l2);
		append_expr (while_expr, test);
	}

	pr.source_line = line;
	pr.source_file = file;

	return while_expr;
}

expr_t *
build_do_while_statement (expr_t *statement, int not, expr_t *test,
						  expr_t *break_label, expr_t *continue_label)
{
	expr_t *l1 = new_label_expr ();
	int         line = pr.source_line;
	pr_string_t file = pr.source_file;
	expr_t     *do_while_expr;

	if (!statement) {
		warning (break_label,
				 "suggest braces around empty body in a ‘do’ statement");
	}

	pr.source_line = test->line;
	pr.source_file = test->file;

	do_while_expr = new_block_expr ();

	append_expr (do_while_expr, l1);
	append_expr (do_while_expr, statement);
	append_expr (do_while_expr, continue_label);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		if (not) {
			backpatch (test->e.bool.true_list, break_label);
			backpatch (test->e.bool.false_list, l1);
		} else {
			backpatch (test->e.bool.true_list, l1);
			backpatch (test->e.bool.false_list, break_label);
		}
		append_expr (test->e.bool.e, break_label);
		append_expr (do_while_expr, test);
	}

	pr.source_line = line;
	pr.source_file = file;

	return do_while_expr;
}

expr_t *
build_for_statement (expr_t *init, expr_t *test, expr_t *next,
					 expr_t *statement,
					 expr_t *break_label, expr_t *continue_label)
{
	expr_t     *tl = new_label_expr ();
	expr_t     *fl = break_label;
	expr_t     *l1 = 0;
	expr_t     *t;
	int         line = pr.source_line;
	pr_string_t file = pr.source_file;
	expr_t     *for_expr;

	if (next)
		t = next;
	else if (test)
		t = test;
	else if (init)
		t = init;
	else
		t = continue_label;
	pr.source_line = t->line;
	pr.source_file = t->file;

	for_expr = new_block_expr ();

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
			backpatch (test->e.bool.true_list, tl);
			backpatch (test->e.bool.false_list, fl);
			append_expr (test->e.bool.e, fl);
			append_expr (for_expr, test);
		}
	} else {
		append_expr (for_expr, goto_expr (tl));
		append_expr (for_expr, fl);
	}

	pr.source_line = line;
	pr.source_file = file;

	return for_expr;
}

expr_t *
build_state_expr (expr_t *e)
{
	expr_t     *frame = 0;
	expr_t     *think = 0;
	expr_t     *step = 0;

	e = reverse_expr_list (e);
	frame = e;
	think = frame->next;
	step = think->next;
	if (think->type == ex_symbol)
		think = think_expr (think->e.symbol);
	if (is_int_val (frame))
		convert_int (frame);
	if (!type_assignable (&type_float, get_type (frame)))
		return error (frame, "invalid type for frame number");
	if (extract_type (think) != ev_func)
		return error (think, "invalid type for think");
	if (step) {
		if (step->next)
			return error (step->next, "too many state arguments");
		if (is_int_val (step))
			convert_int (step);
		if (!type_assignable (&type_float, get_type (step)))
			return error (step, "invalid type for step");
	}
	return new_state_expr (frame, think, step);
}

expr_t *
think_expr (symbol_t *think_sym)
{
	symbol_t   *sym;

	if (think_sym->table)
		return new_symbol_expr (think_sym);

	sym = symtab_lookup (current_symtab, "think");
	if (sym && sym->sy_type == sy_var && sym->type
		&& sym->type->type == ev_field
		&& sym->type->t.fldptr.type->type == ev_func) {
		think_sym->type = sym->type->t.fldptr.type;
	} else {
		think_sym->type = &type_func;
	}
	think_sym = function_symbol (think_sym, 0, 1);
	make_function (think_sym, 0, current_symtab->space, current_storage);
	return new_symbol_expr (think_sym);
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
		&& !(is_scalar (dstType) && is_scalar (srcType))) {
		return cast_error (e, srcType, dstType);
	}
	if (is_array (srcType)) {
		return address_expr (e, dstType->t.fldptr.type);
	}
	if (is_constant (e) && is_scalar (dstType) && is_scalar (srcType)) {
		ex_value_t *val = 0;
		if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const) {
			val = e->e.symbol->s.value;
		} else if (e->type == ex_symbol
				   && e->e.symbol->sy_type == sy_var) {
			// initialized global def treated as a constant
			// from the tests above, the def is known to be constant
			// and of one of the three storable scalar types
			def_t      *def = e->e.symbol->s.def;
			if (is_float (def->type)) {
				val = new_float_val (D_FLOAT (def));
			} else if (is_double (def->type)) {
				val = new_double_val (D_DOUBLE (def));
			} else if (is_integral (def->type)) {
				val = new_int_val (D_INT (def));
			}
		} else if (e->type == ex_value) {
			val = e->e.value;
		} else if (e->type == ex_nil) {
			convert_nil (e, dstType);
			return e;
		}
		if (!val)
			internal_error (e, "unexpected constant expression type");
		e->e.value = convert_value (val, dstType);
		e->type = ex_value;
		c = e;
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

expr_t *
encode_expr (type_t *type)
{
	dstring_t  *encoding = dstring_newstr ();
	expr_t     *e;

	encode_type (encoding, type);
	e = new_string_expr (encoding->str);
	free (encoding);
	return e;
}

expr_t *
sizeof_expr (expr_t *expr, struct type_s *type)
{
	if (!((!expr) ^ (!type)))
		internal_error (0, 0);
	if (!type)
		type = get_type (expr);
	if (type) {
		expr = new_int_expr (type_size (type));
	}
	return expr;
}

expr_t *
reverse_expr_list (expr_t *e)
{
	expr_t     *r = 0;

	while (e) {
		expr_t     *t = e->next;
		e->next = r;
		r = e;
		e = t;
	}
	return r;
}
