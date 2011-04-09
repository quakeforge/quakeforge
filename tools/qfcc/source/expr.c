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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/dstring.h>
#include <QF/mathlib.h>
#include <QF/sys.h>
#include <QF/va.h>

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
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "qc-parse.h"

static expr_t *free_exprs;

type_t     *ev_types[ev_type_count] = {
	&type_void,
	&type_string,
	&type_float,
	&type_vector,
	&type_entity,
	&type_field,
	&type_function,
	&type_pointer,
	&type_quaternion,
	&type_integer,
	&type_uinteger,
	&type_short,
	&type_invalid,
};

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
		new = new_integer_expr (e->line);
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

type_t *
get_type (expr_t *e)
{
	convert_name (e);
	switch (e->type) {
		case ex_label:
		case ex_error:
			return 0;					// something went very wrong
		case ex_bool:
			if (options.code.progsversion == PROG_ID_VERSION)
				return &type_float;
			return &type_integer;
		case ex_nil:
		case ex_state:
			return &type_void;
		case ex_block:
			if (e->e.block.result)
				return get_type (e->e.block.result);
			return &type_void;
		case ex_expr:
		case ex_uexpr:
			return e->e.expr.type;
		case ex_symbol:
			return e->e.symbol->type;
		case ex_temp:
			return e->e.temp.type;
		case ex_value:
			if (e->e.value.type == ev_pointer)
				return pointer_type (e->e.value.v.pointer.type);
			if (e->e.value.type == ev_field)
				return field_type (e->e.value.v.pointer.type);
			if (e->e.value.type == ev_integer
				&& options.code.progsversion == PROG_ID_VERSION) {
				e->e.value.type = ev_float;
				e->e.value.v.float_val = e->e.value.v.integer_val;
			}
			return ev_types[e->e.value.type];
	}
	return 0;
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

	e =  error (e, "can not cast from %s to %s", s1->str, s2->str);
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
						copy_expr ( e->e.bool.true_list->e[count]);
			}
			if (e->e.bool.false_list) {
				int         count = e->e.bool.false_list->size;
				size_t      size = (size_t)&((ex_list_t *) 0)->e[count];
				n->e.bool.false_list = malloc (size);
				while (count--)
					n->e.bool.false_list->e[count] =
						copy_expr ( e->e.bool.false_list->e[count]);
			}
			n->e.bool.e = copy_expr (e->e.bool.e);
			return n;
		case ex_label:
			/// Create a fresh label
			return new_label_expr ();
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
			if (e->e.block.result && !n->e.block.result) {
				error (e, "internal: bogus block result?");
				abort ();
			}
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
			n->e.temp.expr = copy_expr (e->e.temp.expr);
			return n;
	}
	error (e, "internal: invalid expression");
	abort ();
}

const char *
new_label_name (void)
{
	static int  label = 0;
	int         lnum = ++label;
	const char *fname = current_func->sym->name;
	char       *lname;

	lname = nva ("$%s_%d", fname, lnum);
	SYS_CHECKMEM (lname);
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
new_block_expr (void)
{
	expr_t     *b = new_expr ();

	b->type = ex_block;
	b->e.block.head = 0;
	b->e.block.tail = &b->e.block.head;
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
new_symbol_expr (symbol_t *symbol)
{
	expr_t     *e = new_expr ();
	e->type = ex_symbol;
	e->e.symbol = symbol;
	return e;
}

expr_t *
new_temp_def_expr (type_t *type)
{
	expr_t     *e = new_expr ();

	e->type = ex_temp;
	e->e.temp.type = type;
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
	e->e.value.type = ev_string;
	e->e.value.v.string_val = string_val;
	return e;
}

expr_t *
new_float_expr (float float_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_float;
	e->e.value.v.float_val = float_val;
	return e;
}

expr_t *
new_vector_expr (const float *vector_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_vector;
	VectorCopy (vector_val, e->e.value.v.vector_val);
	return e;
}

expr_t *
new_entity_expr (int entity_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_entity;
	e->e.value.v.entity_val = entity_val;
	return e;
}

expr_t *
new_field_expr (int field_val, type_t *type, def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_field;
	e->e.value.v.pointer.val = field_val;
	e->e.value.v.pointer.type = type;
	e->e.value.v.pointer.def = def;
	return e;
}

expr_t *
new_func_expr (int func_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_func;
	e->e.value.v.func_val = func_val;
	return e;
}

expr_t *
new_pointer_expr (int val, type_t *type, def_t *def)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_pointer;
	e->e.value.v.pointer.val = val;
	e->e.value.v.pointer.type = type;
	e->e.value.v.pointer.def = def;
	return e;
}

expr_t *
new_quaternion_expr (const float *quaternion_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_quat;
	QuatCopy (quaternion_val, e->e.value.v.quaternion_val);
	return e;
}

expr_t *
new_integer_expr (int integer_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_integer;
	e->e.value.v.integer_val = integer_val;
	return e;
}

expr_t *
new_uinteger_expr (unsigned uinteger_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_uinteger;
	e->e.value.v.uinteger_val = uinteger_val;
	return e;
}

expr_t *
new_short_expr (short short_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_value;
	e->e.value.type = ev_short;
	e->e.value.v.short_val = short_val;
	return e;
}

int
is_constant (expr_t *e)
{
	if (e->type == ex_nil || e->type == ex_value
		|| (e->type == ex_symbol && e->e.symbol->sy_type == sy_const)
		|| (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
			&& e->e.symbol->s.def->constant))
		return 1;
	return 0;
}

expr_t *
constant_expr (expr_t *e)
{
	expr_t     *new;
	if (!is_constant (e))
		return e;
	if (e->type == ex_nil || e->type == ex_value)
		return e;
	if (e->type != ex_symbol || e->e.symbol->sy_type != sy_const)
		return e;
	new = new_expr ();
	new->type = ex_value;
	new->line = e->line;
	new->file = e->file;
	new->e.value = e->e.symbol->s.value;
	return new;
}

int
is_string_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value.type == ev_string)
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
	if (e->type == ex_value && e->e.value.type == ev_string)
		return e->e.value.v.string_val;
	//if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
	//	&& e->e.symbol->type->type == ev_string)
	//	return e->e.symbol->s.value;
	internal_error (e, "not a string constant");
}

int
is_float_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value.type == ev_float)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_float)
		return 1;
	return 0;
}

float
expr_float (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value.type == ev_float)
		return e->e.value.v.float_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_float)
		return e->e.symbol->s.value.v.float_val;
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
	if (e->type == ex_value && e->e.value.type == ev_vector)
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
	if (e->type == ex_value && e->e.value.type == ev_vector)
		return e->e.value.v.vector_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_vector)
		return e->e.symbol->s.value.v.vector_val;
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
	if (e->type == ex_value && e->e.value.type == ev_quat)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_quat)
		return 1;
	return 0;
}

const float *
expr_quaternion (expr_t *e)
{
	if (e->type == ex_nil)
		return quat_origin;
	if (e->type == ex_value && e->e.value.type == ev_quat)
		return e->e.value.v.quaternion_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_quat)
		return e->e.symbol->s.value.v.quaternion_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& e->e.symbol->s.def->type->type == ev_quat)
		return D_QUAT (e->e.symbol->s.def);
	internal_error (e, "not a quaternion constant");
}

int
is_integer_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value.type == ev_integer)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& (e->e.symbol->type->type == ev_integer
			|| is_enum (e->e.symbol->type)))
		return 1;
	return 0;
}

int
expr_integer (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value.type == ev_integer)
		return e->e.value.v.integer_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& (e->e.symbol->type->type == ev_integer
			|| is_enum (e->e.symbol->type)))
		return e->e.symbol->s.value.v.integer_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_integral (e->e.symbol->s.def->type))
		return D_INT (e->e.symbol->s.def);
	internal_error (e, "not an integer constant");
}

unsigned
expr_uinteger (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value.type == ev_uinteger)
		return e->e.value.v.uinteger_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_uinteger)
		return e->e.symbol->s.value.v.uinteger_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_var
		&& e->e.symbol->s.def->constant
		&& is_integral (e->e.symbol->s.def->type))
		return D_INT (e->e.symbol->s.def);
	internal_error (e, "not an unsigned constant");
}

int
is_short_val (expr_t *e)
{
	if (e->type == ex_nil)
		return 1;
	if (e->type == ex_value && e->e.value.type == ev_short)
		return 1;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_short)
		return 1;
	return 0;
}

short
expr_short (expr_t *e)
{
	if (e->type == ex_nil)
		return 0;
	if (e->type == ex_value && e->e.value.type == ev_short)
		return e->e.value.v.short_val;
	if (e->type == ex_symbol && e->e.symbol->sy_type == sy_const
		&& e->e.symbol->type->type == ev_short)
		return e->e.symbol->s.value.v.short_val;
	internal_error (e, "not a short constant");
}

expr_t *
new_self_expr (void)
{
	symbol_t   *sym;
	
	sym = make_symbol (".self", &type_entity, pr.near_data, st_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return new_symbol_expr (sym);
}

expr_t *
new_this_expr (void)
{
	symbol_t   *sym;
	
	sym = make_symbol (".this", field_type (&type_id), pr.near_data, st_extern);
	if (!sym->table)
		symtab_addsymbol (pr.symtab, sym);
	return new_symbol_expr (sym);
}

expr_t *
new_alias_expr (type_t *type, expr_t *expr)
{
	expr_t     *alias;

	alias = new_unary_expr ('A', expr);
	alias->e.expr.type = type;
	return alias;
}

static expr_t *
param_expr (const char *name, type_t *type)
{
	symbol_t   *sym;
	expr_t     *sym_expr;

	sym = make_symbol (name, &type_param, pr.symtab->space, st_extern);
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
	return param_expr (va (".param_%d", num), type);
}

expr_t *
new_move_expr (expr_t *e1, expr_t *e2, type_t *type, int indirect)
{
	expr_t     *e = new_binary_expr (indirect ? 'M' : 'm', e1, e2);
	e->e.expr.type = type;
	return e;
}

expr_t *
append_expr (expr_t *block, expr_t *e)
{
	if (block->type != ex_block)
		abort ();

	if (!e || e->type == ex_error)
		return block;

	if (e->next) {
		error (e, "append_expr: expr loop detected");
		abort ();
	}

	*block->e.block.tail = e;
	block->e.block.tail = &e->next;

	return block;
}

static symbol_t *
get_struct_field (type_t *t1, expr_t *e1, expr_t *e2)
{
	symtab_t   *strct = t1->t.symtab;
	symbol_t   *sym = e2->e.symbol;//FIXME need to check
	symbol_t   *field;
	
	if (!strct) {
		error (e1, "dereferencing pointer to incomplete type");
		return 0;
	}
	field = symtab_lookup (strct, sym->name);
	if (!field && t1 != &type_entity) {
		error (e2, "'%s' has no member named '%s'", t1->name + 4, sym->name);
		e1->type = ex_error;
	}
	return field;
}

static expr_t *
field_expr (expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	expr_t     *e;
	
	if (e1->type == ex_error)
		return e1;
	t1 = get_type (e1);
	if (t1->type == ev_entity) {
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
	} else if (t1->type == ev_pointer) {
		if (is_struct (t1->t.fldptr.type)) {
			symbol_t   *field;

			field = get_struct_field (t1->t.fldptr.type, e1, e2);
			if (!field)
				return e1;

			e2->type = ex_value;
			e2->e.value.type = ev_short;
			e2->e.value.v.short_val = field->s.offset;
			e = new_binary_expr ('&', e1, e2);
			e->e.expr.type = pointer_type (field->type);
			return unary_expr ('.', e);
		} else if (is_class (t1->t.fldptr.type)) {
			class_t    *class = t1->t.fldptr.type->t.class;
			symbol_t   *sym = e2->e.symbol;//FIXME need to check
			symbol_t   *ivar;
			
			ivar = class_find_ivar (class, vis_protected, sym->name);
			if (!ivar)
				return new_error_expr ();
			e2->type = ex_value;
			e2->e.value.type = ev_short;
			e2->e.value.v.short_val = ivar->s.offset;
			e = new_binary_expr ('&', e1, e2);
			e->e.expr.type = pointer_type (ivar->type);
			return unary_expr ('.', e);
		}
	} else if (t1->type == ev_vector || t1->type == ev_quat
			   || is_struct (t1)) {
		symbol_t   *field;
		
		field = get_struct_field (t1, e1, e2);
		if (!field)
			return e1;

		if (e1->type == ex_expr && e1->e.expr.op == '.'
			&& get_type (e1->e.expr.e1) == &type_entity) {
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
			} else if (e2->type != ex_value || e2->e.value.type != ev_field) {
				internal_error (e2, "unexpected field exression");
			}
			e2->e.value.v.pointer.val += field->s.offset;
			e2->e.value.v.pointer.type = field->type;
			// create a new . expression
			return field_expr (e1, e2);
		} else {
			e2->type = ex_value;
			e2->e.value.type = ev_short;
			e2->e.value.v.short_val = field->s.offset;
			e = address_expr (e1, e2, field->type);
			return unary_expr ('.', e);
		}
	} else if (is_class (t1)) {
		internal_error (e1, "access to class instances not implemented");
	}
	return type_mismatch (e1, e2, '.');
}

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
			new = new_string_expr (0);
			break;
		case ev_uinteger:
		case ev_integer:
		case ev_short:
			if (type_default != &type_integer)
				return new_alias_expr (type_default, e);
			return e;
		case ev_float:
			if (options.code.fast_float
				|| options.code.progsversion == PROG_ID_VERSION) {
				if (type_default != &type_float)
					return new_alias_expr (type_default, e);
				return e;
			}
			new = new_float_expr (0);
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

	if (!l1 && !l2) {
		internal_error (0, 0);
	}
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
convert_bool (expr_t *e, int block)
{
	expr_t     *b;

	if (e->type == ex_expr && (e->e.expr.op == '=' || e->e.expr.op == PAS)
		&& !e->paren) {
		if (options.warnings.precedence)
			warning (e, "suggest parentheses around assignment "
					 "used as truth value");
	}

	if (e->type == ex_uexpr && e->e.expr.op == '!') {
		e = convert_bool (e->e.expr.e1, 0);
		if (e->type == ex_error)
			return e;
		e = unary_expr ('!', e);
	}
	if (e->type != ex_bool) {
		e = test_expr (e);
		if (e->type == ex_error)
			return e;
		if (is_integer_val (e)) {
			b = goto_expr (0);
			if (expr_integer (e))
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

static expr_t *
convert_from_bool (expr_t *e, type_t *type)
{
	expr_t     *zero;
	expr_t     *one;
	expr_t     *cond;

	if (type == &type_float) {
		one = new_float_expr (1);
		zero = new_float_expr (0);
	} else if (type == &type_integer) {
		one = new_integer_expr (1);
		zero = new_integer_expr (0);
	} else if (is_enum (type) && enum_as_bool (type, &zero, &one)) {
		// don't need to do anything
	} else if (type == &type_uinteger) {
		one = new_uinteger_expr (1);
		zero = new_uinteger_expr (0);
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

void
convert_int (expr_t *e)
{
	e->e.value.v.float_val = expr_integer (e);
	e->e.value.type = ev_float;
	e->type = ex_value;
}

void
convert_short (expr_t *e)
{
	e->e.value.v.float_val = expr_short (e);
	e->e.value.type = ev_float;
	e->type = ex_value;
}

void
convert_short_int (expr_t *e)
{
	e->e.value.v.integer_val = expr_short (e);
	e->e.value.type = ev_integer;
	e->type = ex_value;
}

void
convert_nil (expr_t *e, type_t *t)
{
	memset (&e->e.value, 0, sizeof (e->e.value));
	e->e.value.type = low_level_type (t);
	if (t->type == ev_pointer || t->type == ev_field)
		e->e.value.v.pointer.type = t->t.fldptr.type;
	e->type = ex_value;
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

static expr_t *
check_precedence (int op, expr_t *e1, expr_t *e2)
{
	if (e1->type == ex_uexpr && e1->e.expr.op == '!' && !e1->paren) {
		if (options.traditional) {
			if (op != AND && op != OR && op != '=') {
				notice (e1, "precedence of `!' and `%s' inverted for "
							"traditional code", get_op_string (op));
				e1->e.expr.e1->paren = 1;
				return unary_expr ('!', binary_expr (op, e1->e.expr.e1, e2));
			}
		} else if (op == '&' || op == '|') {
			if (options.warnings.precedence)
				warning (e1, "ambiguous logic. Suggest explicit parentheses "
						 "with expressions involving ! and %s",
						 get_op_string (op));
		}
	}
	if (options.traditional) {
		if (e2->type == ex_expr && !e2->paren) {
			if (((op == '&' || op == '|')
				 && (is_math_op (e2->e.expr.op) || is_compare (e2->e.expr.op)))
				|| (op == '='
					&& (e2->e.expr.op == OR || e2->e.expr.op == AND))) {
				notice (e1, "precedence of `%s' and `%s' inverted for "
							"traditional code", get_op_string (op),
							get_op_string (e2->e.expr.op));
				e1 = binary_expr (op, e1, e2->e.expr.e1);
				e1->paren = 1;
				return binary_expr (e2->e.expr.op, e1, e2->e.expr.e2);
			}
			if (((op == EQ || op == NE) && is_compare (e2->e.expr.op))
				|| (op == OR && e2->e.expr.op == AND)
				|| (op == '|' && e2->e.expr.op == '&')) {
				notice (e1, "precedence of `%s' raised to `%s' for "
							"traditional code", get_op_string (op),
							get_op_string (e2->e.expr.op));
				e1 = binary_expr (op, e1, e2->e.expr.e1);
				e1->paren = 1;
				return binary_expr (e2->e.expr.op, e1, e2->e.expr.e2);
			}
		} else if (e1->type == ex_expr && !e1->paren) {
			if (((op == '&' || op == '|')
				 && (is_math_op (e1->e.expr.op) || is_compare (e1->e.expr.op)))
				|| (op == '='
					&& (e1->e.expr.op == OR || e1->e.expr.op == AND))) {
				notice (e1, "precedence of `%s' and `%s' inverted for "
							"traditional code", get_op_string (op),
							get_op_string (e1->e.expr.op));
				e2 = binary_expr (op, e1->e.expr.e2, e2);
				e2->paren = 1;
				return binary_expr (e1->e.expr.op, e1->e.expr.e1, e2);
			}
		}
	} else {
		if (e2->type == ex_expr && !e2->paren) {
			if ((op == '&' || op == '|' || op == '^')
				&& (is_math_op (e2->e.expr.op)
					|| is_compare (e2->e.expr.op))) {
				if (options.warnings.precedence)
					warning (e2, "suggest parentheses around %s in "
							 "operand of %c",
							 is_compare (e2->e.expr.op)
							 		? "comparison"
							 		: get_op_string (e2->e.expr.op),
							 op);
			}
		}
		if (e1->type == ex_expr && !e1->paren) {
			if ((op == '&' || op == '|' || op == '^')
				&& (is_math_op (e1->e.expr.op)
					|| is_compare (e1->e.expr.op))) {
				if (options.warnings.precedence)
					warning (e1, "suggest parentheses around %s in "
							 "operand of %c",
							 is_compare (e1->e.expr.op)
							 		? "comparison"
							 		: get_op_string (e1->e.expr.op),
							 op);
			}
		}
	}
	return 0;
}

static int
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
			if (e->e.expr.op == 'c')
				return 1;
			return (has_function_call (e->e.expr.e1)
					|| has_function_call (e->e.expr.e2));
		case ex_uexpr:
			if (e->e.expr.op != 'g')
				return has_function_call (e->e.expr.e1);
		default:
			return 0;
	}
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	type_t     *type = 0;
	expr_t     *e;

	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;

	convert_name (e1);
	if (e1->type == ex_block && e1->e.block.is_call
		&& has_function_call (e2) && e1->e.block.result) {
		e = new_temp_def_expr (get_type (e1->e.block.result));
		e1 = assign_expr (e, e1);
	}

	if (op == '.')
		return field_expr (e1, e2);

	convert_name (e2);
	if (op == OR || op == AND) {
		e1 = test_expr (e1);
		e2 = test_expr (e2);
	}

	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;
	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2) {
		internal_error (e1, 0);
	}
	if (op == EQ || op == NE) {
		if (e1->type == ex_nil) {
			t1 = t2;
			convert_nil (e1, t1);
		} else if (e2->type == ex_nil) {
			t2 = t1;
			convert_nil (e2, t2);
		}
	}

	if (e1->type == ex_bool)
		e1 = convert_from_bool (e1, t2);

	if (e2->type == ex_bool)
		e2 = convert_from_bool (e2, t1);

	if ((e = check_precedence (op, e1, e2)))
		return e;

	type = t1;

	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			type = &type_integer;
		else
			type = &type_float;
	} else if (op == '*' && t1 == &type_vector && t2 == &type_vector) {
		type = &type_float;
	}
	if (!type)
		internal_error (e1, 0);

	if (options.code.progsversion == PROG_ID_VERSION) {
		switch (op) {
			case '%':
				{
					expr_t     *tmp1, *tmp2, *tmp3, *t1, *t2;
					e = new_block_expr ();
					t1 = new_temp_def_expr (&type_float);
					t2 = new_temp_def_expr (&type_float);
					tmp1 = new_temp_def_expr (&type_float);
					tmp2 = new_temp_def_expr (&type_float);
					tmp3 = new_temp_def_expr (&type_float);

					append_expr (e, assign_expr (t1, e1));
					e1 = binary_expr ('&', t1, t1);
					append_expr (e, assign_expr (tmp1, e1));

					append_expr (e, assign_expr (t2, e2));
					e2 = binary_expr ('&', t2, t2);
					append_expr (e, assign_expr (tmp2, e2));

					e1 = binary_expr ('/', tmp1, tmp2);
					append_expr (e, assign_expr (tmp3, e1));

					e2 = binary_expr ('&', tmp3, tmp3);
					append_expr (e, assign_expr (tmp3, e2));

					e1 = binary_expr ('*', tmp2, tmp3);
					e2 = binary_expr ('-', tmp1, e1);
					e->e.block.result = e2;
					return e;
				}
				break;
		}
	}
	e = new_binary_expr (op, e1, e2);
	e->e.expr.type = type;
	return e;
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

	convert_name (e);
	if (e->type == ex_error)
		return e;
	switch (op) {
		case '-':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary +");
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_pointer:
						internal_error (e, "type check failed!");
					case ev_float:
						return new_float_expr (-expr_float (e));
					case ev_vector:
						VectorNegate (expr_vector (e), v);
						return new_vector_expr (v);
					case ev_quat:
						QuatNegate (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_integer:
						return new_integer_expr (-expr_integer (e));
					case ev_uinteger:
						return new_uinteger_expr (-expr_uinteger (e));
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
				case ex_state:
					internal_error (e, 0);
				case ex_uexpr:
					if (e->e.expr.op == '-')
						return e->e.expr.e1;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_bool:
				case ex_temp:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = e->e.expr.type;
						return n;
					}
				case ex_symbol:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = e->e.symbol->type;
						return n;
					}
				case ex_nil:
					return error (e, "invalid type for unary -");
			}
			break;
		case '!':
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_pointer:
						internal_error (e, 0);
					case ev_string:
						s = expr_string (e);
						return new_integer_expr (!s || !s[0]);
					case ev_float:
						return new_integer_expr (!expr_float (e));
					case ev_vector:
						return new_integer_expr (!VectorIsZero (expr_vector (e)));
					case ev_quat:
						return new_integer_expr (!QuatIsZero (expr_quaternion (e)));
					case ev_integer:
						return new_integer_expr (!expr_integer (e));
					case ev_uinteger:
						return new_uinteger_expr (!expr_uinteger (e));
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
				case ex_state:
					internal_error (e, 0);
				case ex_bool:
					return new_bool_expr (e->e.bool.false_list,
										  e->e.bool.true_list, e);
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary !");
				case ex_uexpr:
				case ex_expr:
				case ex_symbol:
				case ex_temp:
					{
						expr_t     *n = new_unary_expr (op, e);

						if (options.code.progsversion > PROG_ID_VERSION)
							n->e.expr.type = &type_integer;
						else
							n->e.expr.type = &type_float;
						return n;
					}
				case ex_nil:
					return error (e, "invalid type for unary !");
			}
			break;
		case '~':
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_pointer:
					case ev_vector:
						return error (e, "invalid type for unary -");
					case ev_float:
						return new_float_expr (~(int) expr_float (e));
					case ev_quat:
						QuatConj (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_integer:
						return new_integer_expr (~expr_integer (e));
					case ev_uinteger:
						return new_uinteger_expr (~expr_uinteger (e));
					case ev_short:
						return new_short_expr (~expr_short (e));
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
				case ex_state:
					internal_error (e, 0);
				case ex_uexpr:
					if (e->e.expr.op == '~')
						return e->e.expr.e1;
					goto bitnot_expr;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary ~");
					goto bitnot_expr;
				case ex_expr:
				case ex_bool:
				case ex_symbol:
				case ex_temp:
bitnot_expr:
					if (options.code.progsversion == PROG_ID_VERSION) {
						expr_t     *n1 = new_integer_expr (-1);
						return binary_expr ('-', n1, e);
					} else {
						expr_t     *n = new_unary_expr (op, e);
						type_t     *t = get_type (e);

						if (t != &type_integer && t != &type_float
							&& t != &type_quaternion)
							return error (e, "invalid type for unary ~");
						n->e.expr.type = t;
						return n;
					}
				case ex_nil:
					return error (e, "invalid type for unary ~");
			}
			break;
		case '.':
			if (extract_type (e) != ev_pointer)
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
build_function_call (expr_t *fexpr, type_t *ftype, expr_t *params)
{
	expr_t     *e;
	int         arg_count = 0, parm_count = 0;
	int         i;
	expr_t     *args = 0, **a = &args;
	type_t     *arg_types[MAX_PARMS];
	expr_t     *arg_exprs[MAX_PARMS][2];
	int         arg_expr_count = 0;
	expr_t     *call;
	expr_t     *err = 0;

	for (e = params; e; e = e->next) {
		if (e->type == ex_error)
			return e;
		arg_count++;
	}

	if (arg_count > MAX_PARMS) {
		return error (fexpr, "more than %d parameters", MAX_PARMS);
	}
	if (ftype->t.func.num_params < -1) {
		if (-arg_count > ftype->t.func.num_params + 1) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		parm_count = -ftype->t.func.num_params - 1;
	} else if (ftype->t.func.num_params >= 0) {
		if (arg_count > ftype->t.func.num_params) {
			return error (fexpr, "too many arguments");
		} else if (arg_count < ftype->t.func.num_params) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		parm_count = ftype->t.func.num_params;
	}
	for (i = arg_count - 1, e = params; i >= 0; i--, e = e->next) {
		type_t     *t = get_type (e);
 
		if (!type_size (t))
			err = error (e, "type of formal parameter %d is incomplete",
						 i + 1);
		if (type_size (t) > type_size (&type_param))
			err = error (e, "formal parameter %d is too large to be passed by"
						 " value", i + 1);
		if (ftype->t.func.param_types[i] == &type_float
			&& is_integer_val (e)) {
			convert_int (e);
			t = &type_float;
		}
		if (i < parm_count) {
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
			if (is_integer_val (e)
				&& options.code.progsversion == PROG_ID_VERSION)
				convert_int (e);
			if (is_integer_val (e) && options.warnings.vararg_integer)
				warning (e, "passing integer constant into ... function");
		}
		arg_types[arg_count - 1 - i] = t;
	}
	if (err)
		return err;

	call = new_block_expr ();
	call->e.block.is_call = 1;
	for (e = params, i = 0; e; e = e->next, i++) {
		if (has_function_call (e)) {
			*a = new_temp_def_expr (arg_types[i]);
			arg_exprs[arg_expr_count][0] = cast_expr (arg_types[i], e);
			arg_exprs[arg_expr_count][1] = *a;
			arg_expr_count++;
		} else {
			*a = cast_expr (arg_types[i], e);
		}
		a = &(*a)->next;
	}
	for (i = 0; i < arg_expr_count - 1; i++) {
		append_expr (call, assign_expr (arg_exprs[i][1], arg_exprs[i][0]));
	}
	if (arg_expr_count) {
		e = assign_expr (arg_exprs[arg_expr_count - 1][1],
						 arg_exprs[arg_expr_count - 1][0]);
		append_expr (call, e);
	}
	e = new_binary_expr ('c', fexpr, args);
	e->e.expr.type = ftype->t.func.type;
	append_expr (call, e);
	if (ftype->t.func.type != &type_void) {
		call->e.block.result = new_ret_expr (ftype->t.func.type);
	} else if (options.traditional) {
		call->e.block.result = new_ret_expr (&type_float);
	}
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
	if (label && label->type != ex_label)
		internal_error (label, "not a label");
	if (label)
		label->e.label.used++;
	return new_binary_expr (op, test, label);
}

expr_t *
goto_expr (expr_t *label)
{
	if (label && label->type != ex_label)
		internal_error (label, "not a label");
	if (label)
		label->e.label.used++;
	return new_unary_expr ('g', label);
}

expr_t *
return_expr (function_t *f, expr_t *e)
{
	type_t     *t;

	if (!e) {
		if (f->sym->type->t.func.type != &type_void) {
			if (options.traditional) {
				if (options.warnings.traditional)
					warning (e,
							 "return from non-void function without a value");
				e = new_nil_expr ();
			} else {
				e = error (e, "return from non-void function without a value");
				return e;
			}
		}
		return new_unary_expr ('r', 0);
	}

	t = get_type (e);

	if (e->type == ex_error)
		return e;
	if (f->sym->type->t.func.type == &type_void) {
		if (!options.traditional)
			return error (e, "returning a value for a void function");
		if (options.warnings.traditional)
			warning (e, "returning a value for a void function");
	}
	if (e->type == ex_bool)
		e = convert_from_bool (e, f->sym->type->t.func.type);
	if (f->sym->type->t.func.type == &type_float && is_integer_val (e)) {
		convert_int (e);
		t = &type_float;
	}
	if (t == &type_void) {
		if (e->type == ex_nil) {
			t = f->sym->type->t.func.type;
			convert_nil (e, t);
			if (e->type == ex_nil)
				return error (e, "invalid return type for NIL");
		} else {
			if (!options.traditional)
				return error (e, "void value not ignored as it ought to be");
			if (options.warnings.traditional)
				warning (e, "void value not ignored as it ought to be");
			//FIXME does anything need to be done here?
		}
	}
	if (!type_assignable (f->sym->type->t.func.type, t)) {
		if (!options.traditional)
			return error (e, "type mismatch for return value of %s",
						  f->sym->name);
		if (options.warnings.traditional)
			warning (e, "type mismatch for return value of %s",
					 f->sym->name);
	} else {
		if (f->sym->type->t.func.type != t)
			e = cast_expr (f->sym->type->t.func.type, e);
	}
	return new_unary_expr ('r', e);
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

	one = new_integer_expr (1);		// integer constants get auto-cast to float
	if (postop) {
		expr_t     *t1, *t2;
		type_t     *type = get_type (e);
		expr_t     *block = new_block_expr ();
		expr_t     *res = new_expr ();

		t1 = new_temp_def_expr (type);
		t2 = new_temp_def_expr (type);
		append_expr (block, assign_expr (t1, e));
		append_expr (block, assign_expr (t2, binary_expr (op, t1, one)));
		res = copy_expr (e);
		if (res->type == ex_uexpr && res->e.expr.op == '.')
			res = pointer_expr (address_expr (res, 0, 0));
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
	expr_t     *e;
	int         ind = 0;

	if (array->type == ex_error)
		return array;
	if (index->type == ex_error)
		return index;

	if (array_type->type != ev_pointer && !is_array (array_type))
		return error (array, "not an array");
	if (index_type != &type_integer)
		return error (index, "invalid array index type");
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_integer_val (index))
		ind = expr_integer (index);
	if (array_type->t.func.num_params
		&& is_constant (index)
		&& (ind < array_type->t.array.base
			|| ind - array_type->t.array.base >= array_type->t.array.size))
			return error (index, "array index out of bounds");
	scale = new_integer_expr (type_size (array_type->t.array.type));
	index = binary_expr ('*', index, scale);
	index = binary_expr ('-', index,
				 binary_expr ('*',
							  new_integer_expr (array_type->t.array.base),
							  scale));
	index = fold_constants (index);
	if (is_short_val (index))
		ind = expr_short (index);
	if (is_integer_val (index))
		ind = expr_integer (index);
	if ((is_constant (index) && ind < 32768 && ind >= -32768))
		index = new_short_expr (ind);
	if (is_array (array_type)) {
		e = address_expr (array, index, array_type->t.array.type);
	} else {
		if (!is_short_val (index) || expr_short (index)) {
			e = new_binary_expr ('&', array, index);
			//e->e.expr.type = array_type->aux_type;
			e->e.expr.type = array_type;
		} else {
			e = array;
		}
	}
	e = unary_expr ('.', e);
	return e;
}

expr_t *
pointer_expr (expr_t *pointer)
{
	type_t     *pointer_type = get_type (pointer);

	if (pointer->type == ex_error)
		return pointer;
	if (pointer_type->type != ev_pointer)
		return error (pointer, "not a pointer");
	return array_expr (pointer, new_integer_expr (0));
}

expr_t *
address_expr (expr_t *e1, expr_t *e2, type_t *t)
{
	expr_t     *e;

	if (e1->type == ex_error)
		return e1;

	if (!t)
		t = get_type (e1);

	switch (e1->type) {
		case ex_symbol:
			if (e1->e.symbol->sy_type == sy_var) {
				def_t      *def = e1->e.symbol->s.def;
				type_t     *type = def->type;

				if (is_array (type)) {
					e = e1;
					e->type = ex_value;
					e->e.value.type = ev_pointer;
					e->e.value.v.pointer.val = 0;
					e->e.value.v.pointer.type = t;
					e->e.value.v.pointer.def = def;
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
				e = e1;
				e->e.expr.op = '&';
				e->e.expr.type = pointer_type (e->e.expr.type);
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_uexpr:
			if (e1->e.expr.op == '.') {
				e = e1->e.expr.e1;
				if (e->type == ex_expr && e->e.expr.op == '.') {
					e->e.expr.type = pointer_type (e->e.expr.type);
					e->e.expr.op = '&';
				}
				break;
			}
			if (e1->e.expr.op == 'A') {
				if (!t)
					t = e1->e.expr.type;
				return address_expr (e1->e.expr.e1, e2, t);
			}
			return error (e1, "invalid type for unary &");
		case ex_block:
			if (!e1->e.block.result)
				return error (e1, "invalid type for unary &");
			e1->e.block.result = address_expr (e1->e.block.result, e2, t);
			return e1;
		default:
			return error (e1, "invalid type for unary &");
	}
	if (e2) {
		if (e2->type == ex_error)
			return e2;
		if (e->type == ex_value && e->e.value.type == ev_pointer
			&& is_short_val (e2)) {
			e->e.value.v.pointer.val += expr_short (e2);
			e->e.value.v.pointer.type = t;
		} else {
			if (!is_short_val (e2) || expr_short (e2)) {
				if (e->type == ex_expr && e->e.expr.op == '&') {
					e = new_binary_expr ('&', e->e.expr.e1,
										 binary_expr ('+', e->e.expr.e2, e2));
				} else {
					e = new_binary_expr ('&', e, e2);
				}
			}
			if (e->type == ex_expr || e->type == ex_uexpr)
				e->e.expr.type = pointer_type (t);
		}
	}
	return e;
}

expr_t *
build_if_statement (expr_t *test, expr_t *s1, expr_t *s2)
{
	int         line = pr.source_line;
	string_t    file = pr.source_file;
	expr_t     *if_expr;
	expr_t     *tl = new_label_expr ();
	expr_t     *fl = new_label_expr ();

	pr.source_line = test->line;
	pr.source_file = test->file;

	if_expr = new_block_expr ();

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		backpatch (test->e.bool.true_list, tl);
		backpatch (test->e.bool.false_list, fl);
		append_expr (test->e.bool.e, tl);
		append_expr (if_expr, test);
	}
	append_expr (if_expr, s1);

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
build_while_statement (expr_t *test, expr_t *statement,
					   expr_t *break_label, expr_t *continue_label)
{
	int         line = pr.source_line;
	string_t    file = pr.source_file;
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
		backpatch (test->e.bool.true_list, l1);
		backpatch (test->e.bool.false_list, l2);
		append_expr (test->e.bool.e, l2);
		append_expr (while_expr, test);
	}

	pr.source_line = line;
	pr.source_file = file;

	return while_expr;
}

expr_t *
build_do_while_statement (expr_t *statement, expr_t *test,
						  expr_t *break_label, expr_t *continue_label)
{
	expr_t *l1 = new_label_expr ();
	int         line = pr.source_line;
	string_t    file = pr.source_file;
	expr_t     *do_while_expr;

	pr.source_line = test->line;
	pr.source_file = test->file;

	do_while_expr = new_block_expr ();

	append_expr (do_while_expr, l1);
	append_expr (do_while_expr, statement);
	append_expr (do_while_expr, continue_label);

	test = convert_bool (test, 1);
	if (test->type != ex_error) {
		backpatch (test->e.bool.true_list, l1);
		backpatch (test->e.bool.false_list, break_label);
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
	string_t    file = pr.source_file;
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
build_state_expr (expr_t *frame, expr_t *think, expr_t *step)
{
	if (is_integer_val (frame))
		convert_int (frame);
	if (!type_assignable (&type_float, get_type (frame)))
		return error (frame, "invalid type for frame number");
	if (extract_type (think) != ev_func)
		return error (think, "invalid type for think");
	if (step) {
		if (is_integer_val (step))
			convert_int (step);
		if (!type_assignable (&type_float, get_type (step)))
			return error (step, "invalid type for frame number");
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
		think_sym->type = &type_function;
	}
	think_sym = function_symbol (think_sym, 0, 1);
	make_function (think_sym, 0, current_symtab->space, current_storage);
	return new_symbol_expr (think_sym);
}

static int
is_indirect (expr_t *e)
{
	if (e->type == ex_block && e->e.block.result)
		return is_indirect (e->e.block.result);
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (!(e->type == ex_uexpr && e->e.expr.op == '.'))
		return 0;
	e = e->e.expr.e1;
	if (e->type != ex_value || e->e.value.type != ev_pointer
		|| !(POINTER_VAL (e->e.value.v.pointer) >= 0
			 && POINTER_VAL (e->e.value.v.pointer) < 65536)) {
		return 1;
	}
	return 0;
}

static inline int
is_lvalue (expr_t *e)
{
	if (e->type == ex_symbol) {
		switch (e->e.symbol->sy_type) {
			case sy_var:
				return 1;
			case sy_const:
				return 0;
			case sy_type:
				return 0;
			case sy_expr:
				return 0;
			case sy_func:
				return 0;
			case sy_class:
				return 0;
		}
	}
	if (e->type == ex_temp)
		return 1;
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (e->type == ex_uexpr && e->e.expr.op == '.')
		return 1;
	if (e->type == ex_uexpr && e->e.expr.op == 'A')
		return is_lvalue (e->e.expr.e1);
	return 0;
}

expr_t *
assign_expr (expr_t *e1, expr_t *e2)
{
	int         op = '=';
	type_t     *t1, *t2, *type;
	expr_t     *e;

	convert_name (e1);
	convert_name (e2);

	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;

	e1 = fold_constants (e1);
	e2 = fold_constants (e2);

	if (options.traditional) {
		if (e2->type == ex_expr && !e2->paren
			&& (e2->e.expr.op == AND || e2->e.expr.op == OR)) {
			notice (e2, "precedence of `%s' and `%s' inverted for "
						"traditional code", get_op_string (op),
						get_op_string (e2->e.expr.op));
			e1 = assign_expr (e1, e2->e.expr.e1);
			e1->paren = 1;
			return binary_expr (e2->e.expr.op, e1, e2->e.expr.e2);
		}
	}

	if (!is_lvalue (e1)) {
		if (options.traditional)
			warning (e1, "invalid lvalue in assignment");
		else
			return error (e1, "invalid lvalue in assignment");
	}
	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2) {
		internal_error (e1, 0);
	}
	//XXX func = func ???
	if (t1->type == ev_pointer && is_array (t2)) {
		e2 = address_expr (e2, 0, t2->t.fldptr.type);
		t2 = get_type (e2);
	}
	if (e2->type == ex_bool)
		e2 = convert_from_bool (e2, t1);

	if (t1->type != ev_void && e2->type == ex_nil) {
		t2 = t1;
		convert_nil (e2, t2);
	}

	e2->rvalue = 1;

	if (!type_assignable (t1, t2)) {
		if (options.traditional) {
			if (t1->type == ev_func && t2->type == ev_func) {
				warning (e1, "assignment between disparate function types");
			} else if (t1->type == ev_float && t2->type == ev_vector) {
				warning (e1, "assignment of vector to float");
				e2 = binary_expr ('.', e2, new_name_expr ("x"));
			} else if (t1->type == ev_vector && t2->type == ev_float) {
				warning (e1, "assignment of float to vector");
				e1 = binary_expr ('.', e1, new_name_expr ("x"));
			} else {
				return type_mismatch (e1, e2, op);
			}
		} else {
			return type_mismatch (e1, e2, op);
		}
	}
	type = t1;
	if (is_indirect (e1) && is_indirect (e2)) {
		if (is_struct (get_type (e2))) {
			e1 = address_expr (e1, 0, 0);
			e2 = address_expr (e2, 0, 0);
			e = new_move_expr (e1, e2, t2, 1);
		} else {
			expr_t     *temp = new_temp_def_expr (t1);

			e = new_block_expr ();
			append_expr (e, assign_expr (temp, e2));
			append_expr (e, assign_expr (e1, temp));
			e->e.block.result = temp;
		}
		return e;
	} else if (is_indirect (e1)) {
		if (is_struct (get_type (e1))) {
			e1 = address_expr (e1, 0, 0);
			e2 = address_expr (e2, 0, 0);
			return new_move_expr (e1, e2, t1, 1);
		}
		if (e1->type == ex_expr) {
			if (get_type (e1->e.expr.e1) == &type_entity) {
				type = e1->e.expr.type;
				e1->e.expr.type = pointer_type (type);
				e1->e.expr.op = '&';
			}
			op = PAS;
		} else {
			e = e1->e.expr.e1;
			if ((e->type != ex_value || e->e.value.type != ev_pointer)
				|| !(POINTER_VAL (e->e.value.v.pointer) > 0
					 && POINTER_VAL (e->e.value.v.pointer) < 65536)) {
				e1 = e;
				op = PAS;
			}
		}
	} else if (is_indirect (e2)) {
		if (is_struct (get_type (e1))) {
			e1 = address_expr (e1, 0, 0);
			e2 = address_expr (e2, 0, 0);
			e2->rvalue = 1;
			return new_move_expr (e1, e2, t2, 1);
		}
		if (e2->type == ex_uexpr) {
			e = e2->e.expr.e1;
			if ((e->type != ex_value || e->e.value.type != ev_pointer)
				|| !(POINTER_VAL (e->e.value.v.pointer) > 0
					 && POINTER_VAL (e->e.value.v.pointer) < 65536)) {
				if (e->type == ex_expr && e->e.expr.op == '&'
					&& e->e.expr.type->type == ev_pointer
					&& !is_constant (e)) {
					e2 = e;
					e2->e.expr.op = '.';
					e2->e.expr.type = t2;
					e2->rvalue = 1;
				}
			}
		}
	}
	if (is_struct (get_type (e1))) {
		return new_move_expr (e1, e2, get_type (e2), 0);
	}
	if (!type)
		internal_error (e1, 0);

	e = new_binary_expr (op, e1, e2);
	e->e.expr.type = type;
	return e;
}

expr_t *
cast_expr (type_t *type, expr_t *e)
{
	expr_t    *c;
	type_t    *e_type;

	convert_name (e);

	if (e->type == ex_error)
		return e;

	e_type = get_type (e);

	if (type == e_type)
		return e;

	if ((type == type_default && is_enum (e_type))
		|| (is_enum (type) && e_type == type_default))
		return e;
	if ((type == &type_integer && e_type == &type_uinteger)
		|| (type == &type_uinteger && e_type == &type_integer))
		return new_alias_expr (type, e);
	if (!(type->type == ev_pointer
		  && (e_type->type == ev_pointer
			  || e_type == &type_integer //|| e_type == &type_uinteger
			  || is_array (e_type)))
		&& !(type->type == ev_func && e_type->type == ev_func)
		&& !(((type == &type_integer)
			  && (e_type == &type_float || e_type == &type_integer
				  || e_type->type == ev_pointer))
			 || (type == &type_float
				 && (e_type == &type_integer || is_enum (e_type))))) {
		return cast_error (e, get_type (e), type);
	}
	if (is_array (e_type)) {
		return address_expr (e, 0, 0);
	}
	if ((is_float (type) && is_integral (e_type))
		|| (is_integral (type) && is_float (e_type))) {
		c = new_unary_expr ('C', e);
		c->e.expr.type = type;
	} else if (e->type == ex_uexpr && e->e.expr.op == '.') {
		e->e.expr.type = type;
		c = e;
	} else {
		c = new_unary_expr ('C', e);
		c->e.expr.type = type;
	}
	return c;
}

expr_t *
selector_expr (keywordarg_t *selector)
{
	dstring_t  *sel_id = dstring_newstr ();
	expr_t     *sel;
	symbol_t   *sel_sym;
	symbol_t   *sel_table;
	int         index;

	selector = copy_keywordargs (selector);
	selector = (keywordarg_t *) reverse_params ((param_t *) selector);
	selector_name (sel_id, selector);
	index = selector_index (sel_id->str);
	index *= type_size (type_SEL.t.fldptr.type);
	sel_sym = make_symbol ("_OBJ_SELECTOR_TABLE_PTR", &type_SEL,
						   pr.near_data, st_static);
	if (!sel_sym->table) {
		symtab_addsymbol (pr.symtab, sel_sym);
		sel_table = make_symbol ("_OBJ_SELECTOR_TABLE",
								 array_type (type_SEL.t.fldptr.type, 0),
								 pr.far_data, st_extern);
		if (!sel_table->table)
			symtab_addsymbol (pr.symtab, sel_table);
		reloc_def_def (sel_table->s.def, sel_sym->s.def);
	}
	sel = new_symbol_expr (sel_sym);
	dstring_delete (sel_id);
	sel = new_binary_expr ('&', sel, new_short_expr (index));
	sel->e.expr.type = &type_SEL;
	return sel;
}

expr_t *
protocol_expr (const char *protocol)
{
	return error (0, "not implemented");
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
super_expr (class_type_t *class_type)
{
	symbol_t   *sym;
	expr_t     *super;
	expr_t     *e;
	expr_t     *super_block;
	class_t    *class;

	if (!class_type)
		return error (0, "`super' used outside of class implementation");

	class = extract_class (class_type);

	if (!class->super_class)
		return error (0, "%s has no super class", class->name);

	sym = symtab_lookup (current_symtab, ".super");
	if (!sym || sym->table != current_symtab) {
		sym = new_symbol (".super");
		initialize_def (sym, &type_Super, 0, current_symtab->space, st_local);
	}
	super = new_symbol_expr (sym);

	super_block = new_block_expr ();

	e = assign_expr (binary_expr ('.', super, new_name_expr ("self")),
								  new_name_expr ("self"));
	append_expr (super_block, e);

	e = new_symbol_expr (class_pointer_symbol (class));
	e = assign_expr (binary_expr ('.', super, new_name_expr ("class")),
					 binary_expr ('.', e, new_name_expr ("super_class")));
	append_expr (super_block, e);

	e = address_expr (super, 0, 0);
	super_block->e.block.result = e;
	return super_block;
}

expr_t *
message_expr (expr_t *receiver, keywordarg_t *message)
{
	expr_t     *args = 0, **a = &args;
	expr_t     *selector = selector_expr (message);
	expr_t     *call;
	keywordarg_t *m;
	int         self = 0, super = 0, class_msg = 0;
	type_t     *rec_type;
	type_t     *return_type;
	type_t     *method_type = &type_IMP;
	class_t    *class = 0;
	method_t   *method;
	expr_t     *send_msg;

	if (receiver->type == ex_symbol
		&& strcmp (receiver->e.symbol->name, "super") == 0) {
		super = 1;

		receiver = super_expr (current_class);

		if (receiver->type == ex_error)
			return receiver;
		receiver = cast_expr (&type_id, receiver);	//FIXME better way?
		class = extract_class (current_class);
		rec_type = class->type;
	} else {
		if (receiver->type == ex_symbol) {
			if (strcmp (receiver->e.symbol->name, "self") == 0)
				self = 1;
			if (receiver->e.symbol->sy_type == sy_class) {
				class = receiver->e.symbol->type->t.class;
				class_msg = 1;
				receiver = new_symbol_expr (class_pointer_symbol (class));
			}
		}
		rec_type = get_type (receiver);

		if (receiver->type == ex_error)
			return receiver;

		if (rec_type->type == ev_pointer)
			rec_type = rec_type->t.fldptr.type;
		if (!is_class (rec_type))
			return error (receiver, "not a class/object");

		if (self) {
			if (!class)
				class = extract_class (current_class);
			if (rec_type == class_Class.type)
				class_msg = 1;
		} else {
			if (!class)
				class = rec_type->t.class;
		}
	}

	return_type = &type_id;
	method = class_message_response (class, class_msg, selector);
	if (method)
		return_type = method->type->t.func.type;

	for (m = message; m; m = m->next) {
		*a = m->expr;
		while ((*a))
			a = &(*a)->next;
	}
	*a = selector;
	a = &(*a)->next;
	*a = receiver;

	send_msg = send_message (super);
	if (method) {
		expr_t      *err;
		if ((err = method_check_params (method, args)))
			return err;
		method_type = method->type;
	}
	call = build_function_call (send_msg, method_type, args);

	if (call->type == ex_error)
		return receiver;

	call->e.block.result = new_ret_expr (return_type);
	return call;
}

expr_t *
sizeof_expr (expr_t *expr, struct type_s *type)
{
	if (!((!expr) ^ (!type))) {
		internal_error (0, 0);
	}
	if (!type)
		type = get_type (expr);
	expr = new_integer_expr (type_size (type));
	return expr;
}
