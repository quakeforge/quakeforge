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

static __attribute__ ((unused)) const char rcsid[] =
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
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "idstuff.h"
#include "immediate.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "strpool.h"
#include "struct.h"
#include "type.h"
#include "qc-parse.h"

static expr_t *free_exprs;
int         lineno_base;

etype_t     qc_types[] = {
	ev_void,							// ex_error
	ev_void,							// ex_state
	ev_void,							// ex_bool
	ev_void,							// ex_label
	ev_void,							// ex_block
	ev_void,							// ex_expr
	ev_void,							// ex_uexpr
	ev_void,							// ex_def
	ev_void,							// ex_temp
	ev_void,							// ex_nil
	ev_void,							// ex_name

	ev_string,							// ex_string
	ev_float,							// ex_float
	ev_vector,							// ex_vector
	ev_entity,							// ex_entity
	ev_field,							// ex_field
	ev_func,							// ex_func
	ev_pointer,							// ex_pointer
	ev_quaternion,						// ex_quaternion
	ev_integer,							// ex_integer
	ev_uinteger,						// ex_uinteger
	ev_short,							// ex_short
};

type_t     *ev_types[] = {
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
	&type_void,							// FIXME what type?
	&type_void,							// FIXME what type?
	&type_void,							// FIXME what type?
	&type_SEL,
	&type_void,							// FIXME what type?
};

expr_type   expr_types[] = {
	ex_nil,								// ev_void
	ex_string,							// ev_string
	ex_float,							// ev_float
	ex_vector,							// ev_vector
	ex_entity,							// ev_entity
	ex_field,							// ev_field
	ex_func,							// ev_func
	ex_pointer,							// ev_pointer
	ex_quaternion,						// ev_quaternion
	ex_integer,							// ev_integer
	ex_uinteger,						// ev_uinteger
	ex_short,							// ev_short
	ex_nil,								// ev_struct
	ex_nil,								// ev_object
	ex_nil,								// ev_class
	ex_nil,								// ev_sel
	ex_nil,								// ev_array
};

void
convert_name (expr_t *e)
{
	if (e->type == ex_name) {
		const char *name = e->e.string_val;
		def_t      *d;
		expr_t     *new;
		class_t    *class;

		class = get_class (name, 0);
		if (class) {
			e->type = ex_def;
			e->e.def = class_pointer_def (class);
			return;
		}
		d = get_def (NULL, name, current_scope, st_none);
		if (d) {
			if (d->global) {
				new = class_ivar_expr (current_class, name);
				if (new)
					goto convert;
			}
			e->type = ex_def;
			e->e.def = d;
			return;
		}
		new = class_ivar_expr (current_class, name);
		if (new)
			goto convert;
		new = get_enum (name);
		if (new)
			goto convert;
		error (e, "Undeclared variable \"%s\".", name);
		return;
	  convert:
		e->type = new->type;
		e->e = new->e;
	}
}

type_t *
get_type (expr_t *e)
{
	convert_name (e);
	switch (e->type) {
		case ex_label:
		case ex_name:
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
		case ex_def:
			return e->e.def->type;
		case ex_temp:
			return e->e.temp.type;
		case ex_pointer:
			return pointer_type (e->e.pointer.type);
		case ex_integer:
			if (options.code.progsversion == PROG_ID_VERSION) {
				e->type = ex_float;
				e->e.float_val = e->e.integer_val;
			}
			// fall through
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_quaternion:
		case ex_uinteger:
		case ex_short:
			return ev_types[qc_types[e->type]];
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

const char *
get_op_string (int op)
{
	switch (op) {
		case PAS:	return ".=";
		case OR:	return "||";
		case AND:	return "&&";
		case EQ:	return "==";
		case NE:	return "!=";
		case LE:	return "<=";
		case GE:	return ">=";
		case LT:	return "<";
		case GT:	return ">";
		case '=':	return "=";
		case '+':	return "+";
		case '-':	return "-";
		case '*':	return "*";
		case '/':	return "/";
		case '%':	return "%";
		case '&':	return "&";
		case '|':	return "|";
		case '^':	return "^";
		case '~':	return "~";
		case '!':	return "!";
		case SHL:	return "<<";
		case SHR:	return ">>";
		case '.':	return ".";
		case 'i':	return "<if>";
		case 'n':	return "<ifnot>";
		case IFBE:	return "<ifbe>";
		case IFB:	return "<ifb>";
		case IFAE:	return "<ifae>";
		case IFA:	return "<ifa>";
		case 'g':	return "<goto>";
		case 'r':	return "<return>";
		case 'b':	return "<bind>";
		case 's':	return "<state>";
		case 'c':	return "<call>";
		case 'C':	return "<cast>";
		case 'M':	return "<move>";
		default:
			return "unknown";
	}
}

expr_t *
type_mismatch (expr_t *e1, expr_t *e2, int op)
{
	etype_t     t1, t2;

	t1 = extract_type (e1);
	t2 = extract_type (e2);

	return error (e1, "type mismatch: %s %s %s",
				  pr_type_name[t1], get_op_string (op), pr_type_name[t2]);
}

static void
check_initialized (expr_t *e)
{
	if (options.warnings.uninited_variable) {
		if (e->type == ex_def
			&& !(e->e.def->type->type == ev_func
				 && e->e.def->global)
			&& !(e->e.def->type->type == ev_struct)
			&& !e->e.def->external
			&& !e->e.def->initialized) {
			warning (e, "%s may be used uninitialized", e->e.def->name);
			e->e.def->initialized = 1;	// only warn once
		}
	}
}

void
inc_users (expr_t *e)
{
	if (e && e->type == ex_temp)
		e->e.temp.users++;
	else if (e && e->type == ex_block)
		inc_users (e->e.block.result);
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

const char *
new_label_name (void)
{
	static int  label = 0;
	int         lnum = ++label;
	const char *fname = current_func->def->name;
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
	l->e.label.next = pr.labels;
	pr.labels = &l->e.label;
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

	inc_users (e1);
	inc_users (e2);

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

	inc_users (e1);

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
	e->type = ex_name;
	e->e.string_val = name;
	return e;
}

expr_t *
new_string_expr (const char *string_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_string;
	e->e.string_val = string_val;
	return e;
}

expr_t *
new_float_expr (float float_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_float;
	e->e.float_val = float_val;
	return e;
}

expr_t *
new_vector_expr (float *vector_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_vector;
	memcpy (e->e.vector_val, vector_val, sizeof (e->e.vector_val));
	return e;
}

expr_t *
new_entity_expr (int entity_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_entity;
	e->e.entity_val = entity_val;
	return e;
}

expr_t *
new_field_expr (int field_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_field;
	e->e.field_val = field_val;
	return e;
}

expr_t *
new_func_expr (int func_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_func;
	e->e.func_val = func_val;
	return e;
}

//expr_t *new_pointer_expr ();
expr_t *
new_quaternion_expr (float *quaternion_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_quaternion;
	memcpy (e->e.quaternion_val, quaternion_val, sizeof (e->e.quaternion_val));
	return e;
}

expr_t *
new_integer_expr (int integer_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_integer;
	e->e.integer_val = integer_val;
	return e;
}

expr_t *
new_uinteger_expr (unsigned int uinteger_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_uinteger;
	e->e.uinteger_val = uinteger_val;
	return e;
}

expr_t *
new_short_expr (short short_val)
{
	expr_t     *e = new_expr ();
	e->type = ex_short;
	e->e.short_val = short_val;
	return e;
}

int
is_constant (expr_t *e)
{
	if (e->type >= ex_string
		|| (e->type == ex_def && e->e.def->constant))
		return 1;
	return 0;
}

expr_t *
constant_expr (expr_t *var)
{
	if (var->type != ex_def || !var->e.def->constant)
		return var;
	switch (var->e.def->type->type) {
		case ev_string:
			return new_string_expr (G_GETSTR (var->e.def->ofs));
		case ev_float:
			return new_float_expr (G_FLOAT (var->e.def->ofs));
		case ev_vector:
			return new_vector_expr (G_VECTOR (var->e.def->ofs));
		case ev_field:
			return new_field_expr (G_var (integer, var->e.def->ofs));
		case ev_integer:
			return new_integer_expr (G_INT (var->e.def->ofs));
		case ev_uinteger:
			return new_uinteger_expr (G_INT (var->e.def->ofs));
		default:
			return var;
	}
}

expr_t *
new_bind_expr (expr_t *e1, expr_t *e2)
{
	expr_t     *e;

	if (!e2 || e2->type != ex_temp) {
		error (e1, "internal error");
		abort ();
	}
	e = new_expr ();
	e->type = ex_expr;
	e->e.expr.op = 'b';
	e->e.expr.e1 = e1;
	e->e.expr.e2 = e2;
	e->e.expr.type = get_type (e2);
	return e;
}

expr_t *
new_self_expr (void)
{
	def_t      *def = get_def (&type_entity, ".self", pr.scope, st_extern);

	def_initialized (def);
	return new_def_expr (def);
}

expr_t *
new_this_expr (void)
{
	type_t     *type = field_type (&type_id);
	def_t      *def = get_def (type, ".this", pr.scope, st_extern);

	def_initialized (def);
	def->nosave = 1;
	return new_def_expr (def);
}

static expr_t *
param_expr (const char *name, type_t *type)
{
	def_t      *def = get_def (&type_param, name, pr.scope, st_extern);
	expr_t     *def_expr;

	def_initialized (def);
	def->nosave = 1;
	def_expr = new_def_expr (def);
	return unary_expr ('.', address_expr (def_expr, 0, type));
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
new_move_expr (expr_t *e1, expr_t *e2, type_t *type)
{
	expr_t     *e = new_binary_expr ('M', e1, e2);
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

void
print_expr (expr_t *e)
{
	printf (" ");
	if (!e) {
		printf ("(nil)");
		return;
	}
	switch (e->type) {
		case ex_error:
			printf ("(error)");
			break;
		case ex_state:
			printf ("[");
			print_expr (e->e.state.frame);
			printf (",");
			print_expr (e->e.state.think);
			printf (",");
			print_expr (e->e.state.step);
			printf ("]");
			break;
		case ex_bool:
			printf ("bool");	//FIXME
			break;
		case ex_label:
			printf ("%s", e->e.label.name);
			break;
		case ex_block:
			if (e->e.block.result) {
				print_expr (e->e.block.result);
				printf ("=");
			}
			printf ("{\n");
			for (e = e->e.block.head; e; e = e->next) {
				print_expr (e);
				puts ("");
			}
			printf ("}");
			break;
		case ex_expr:
			print_expr (e->e.expr.e1);
			if (e->e.expr.op == 'c') {
				expr_t     *p = e->e.expr.e2;

				printf ("(");
				while (p) {
					print_expr (p);
					if (p->next)
						printf (",");
					p = p->next;
				}
				printf (")");
			} else if (e->e.expr.op == 'b') {
				printf (" <-->");
				print_expr (e->e.expr.e2);
			} else {
				print_expr (e->e.expr.e2);
				printf (" %s", get_op_string (e->e.expr.op));
			}
			break;
		case ex_uexpr:
			print_expr (e->e.expr.e1);
			printf (" u%s", get_op_string (e->e.expr.op));
			break;
		case ex_def:
			if (e->e.def->name)
				printf ("%s", e->e.def->name);
			if (!e->e.def->global) {
				printf ("<%d>", e->e.def->ofs);
			} else {
				printf ("[%d]", e->e.def->ofs);
			}
			break;
		case ex_temp:
			printf ("(");
			print_expr (e->e.temp.expr);
			printf (":");
			if (e->e.temp.def) {
				if (e->e.temp.def->name) {
					printf ("%s", e->e.temp.def->name);
				} else {
					printf ("<%d>", e->e.temp.def->ofs);
				}
			} else {
				printf ("<>");
			}
			printf (":%s:%d)@", pr_type_name[e->e.temp.type->type],
					e->e.temp.users);
			break;
		case ex_nil:
			printf ("NULL");
			break;
		case ex_string:
		case ex_name:
			printf ("\"%s\"", e->e.string_val);
			break;
		case ex_float:
			printf ("%g", e->e.float_val);
			break;
		case ex_vector:
			printf ("'%g", e->e.vector_val[0]);
			printf (" %g", e->e.vector_val[1]);
			printf (" %g'", e->e.vector_val[2]);
			break;
		case ex_quaternion:
			printf ("'%g", e->e.quaternion_val[0]);
			printf (" %g", e->e.quaternion_val[1]);
			printf (" %g", e->e.quaternion_val[2]);
			printf (" %g'", e->e.quaternion_val[3]);
			break;
		case ex_pointer:
			printf ("(%s)[%d]", pr_type_name[e->e.pointer.type->type],
					e->e.pointer.val);
			break;
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_integer:
			printf ("%d", e->e.integer_val);
			break;
		case ex_uinteger:
			printf ("%d", e->e.uinteger_val);
			break;
		case ex_short:
			printf ("%d", e->e.short_val);
			break;
	}
}

static expr_t *
field_expr (expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	expr_t     *e;
	def_t      *d;
	int         i;
	struct_field_t *field;
	class_t    *class;
	struct_t   *strct;

	if (e1->type == ex_error)
		return e1;
	t1 = get_type (e1);
	switch (t1->type) {
		case ev_struct:
		case ev_class:
			check_initialized (e1);
			if (e2->type != ex_name)
				return error (e2, "structure field name expected");
			if (t1->type == ev_struct)
				strct = t1->s.strct;
			else
				strct = t1->s.class->ivars;
			field = struct_find_field (strct, e2->e.string_val);
			if (!field)
				return error (e2, "structure has no field %s",
							  e2->e.string_val);
			e2->type = ex_short;
			e2->e.short_val = field->offset;
			e = unary_expr ('.', address_expr (e1, e2, field->type));
			return e;
		case ev_pointer:
			check_initialized (e1);
			switch (t1->aux_type->type) {
				case ev_struct:
					if (e2->type == ex_name) {
						field = struct_find_field (t1->aux_type->s.strct,
												   e2->e.string_val);
						if (!field)
							return error (e2, "structure has no field %s",
										  e2->e.string_val);
						e2->type = ex_short;
						e2->e.short_val = field->offset;
						t1 = pointer_type (field->type);
					}
					break;
				case ev_object:
				case ev_class:
					if (e2->type == ex_name) {
						int         protected;

						class = t1->aux_type->s.class;
						protected = class_access (current_class, class);
						field = class_find_ivar (class, protected,
												 e2->e.string_val);
						if (!field)
							return new_error_expr ();
						e2->type = ex_short;
						e2->e.short_val = field->offset;
						t1 = pointer_type (field->type);
					}
					break;
				default:
					break;
			}
			if (e1->type == ex_pointer) {
				if (e2->type == ex_short) {
					e1->e.pointer.val += e2->e.short_val;
				} else if (e2->type == ex_integer) {
					e1->e.pointer.val += e2->e.integer_val;
				} else if (e2->type == ex_uinteger) {
					e1->e.pointer.val += e2->e.uinteger_val;
				} else {
					break;
				}
				e1->e.pointer.type = t1->aux_type;
				return unary_expr ('.', e1);
			} else {
				e = new_binary_expr ('&', e1, e2);
				e->e.expr.type = t1;
				return unary_expr ('.', e);
			}
			break;
		case ev_entity:
			check_initialized (e1);
			if (e2->type == ex_name) {
				def_t      *d = field_def (e2->e.string_val);

				if (!d) {
					t2 = get_type (e2);
					if (e2->type == ex_error)
						return e2;
					break;
				}
				e2 = new_def_expr (d);
				t2 = get_type (e2);
				e = new_binary_expr ('.', e1, e2);
				e->e.expr.type = t2->aux_type;
				return e;
			} else {
				t2 = get_type (e2);
				if (e2->type == ex_error)
					return e2;
				if (t2->type == ev_field) {
					e = new_binary_expr ('.', e1, e2);
					e->e.expr.type = t2->aux_type;
					return e;
				}
			}
			break;
		case ev_vector:
			if (!options.traditional && e2->type == ex_name) {
				field = struct_find_field (vector_struct, e2->e.string_val);
				if (!field)
					return error (e2, "vector has no field %s",
								  e2->e.string_val);
				switch (e1->type) {
					case ex_expr:
						if (e1->e.expr.op == '.'
							&& extract_type (e1->e.expr.e1) == ev_entity
							&& e1->e.expr.e2->type == ex_def) {
							d = e1->e.expr.e2->e.def->def_next;
							for (i = field->offset; i; i--)
								d = d->def_next;
							e = new_def_expr (d);
							e = new_binary_expr ('.', e1->e.expr.e1, e);
							e->e.expr.type = field->type;
							return e;
						}
						break;
					case ex_uexpr:
						if (e1->e.expr.op == '.') {
							if (e1->e.expr.e1->type == ex_pointer) {
								e = new_expr ();
								e->type = ex_pointer;
								e1 = e1->e.expr.e1;
								i = e1->e.pointer.val;
								e->e.pointer.val = i + field->offset;
								e->e.pointer.type = field->type;
								return unary_expr ('.', e);
							} else if (extract_type (e1->e.expr.e1)
									   == ev_pointer) {
								e = new_integer_expr (field->offset);
								e = address_expr (e1, e, field->type);
								return unary_expr ('.', e);
							}
						}
						break;
					case ex_block:
						print_expr (e1); puts ("");
#if 0
						e1 = new_bind_expr (e1, new_temp_def_expr (t1));
						e2 = new_short_expr (field->offset);
						e = address_expr (e1, e2, field->type);
						e = unary_expr ('.', e);
						return e;
#endif
						break;
					case ex_def:
						d = e1->e.def->def_next;
						for (i = field->offset; i; i--)
							d = d->def_next;
						e = new_def_expr (d);
						return e;
					case ex_vector:
						e = new_float_expr (e1->e.vector_val[field->offset]);
						return e;
					default:
						break;
				}
			}
			break;
		default:
			break;
	}
	return type_mismatch (e1, e2, '.');
}

expr_t *
test_expr (expr_t *e, int test)
{
	static float zero[4] = {0, 0, 0, 0};
	expr_t     *new = 0;
	etype_t     type;

	if (e->type == ex_error)
		return e;

	check_initialized (e);

	if (!test)
		return unary_expr ('!', e);

	type = extract_type (e);
	if (e->type == ex_error)
		return e;
	switch (type) {
		case ev_type_count:
			error (e, "internal error");
			abort ();
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
			return e;
		case ev_float:
			if (options.code.progsversion == PROG_ID_VERSION)
				return e;
			new = new_float_expr (0);
			break;
		case ev_vector:
			new = new_vector_expr (zero);
			break;
		case ev_entity:
			new = new_entity_expr (0);
			break;
		case ev_field:
			new = new_field_expr (0);
			break;
		case ev_func:
			new = new_func_expr (0);
			break;
		case ev_pointer:
			new = new_nil_expr ();
			break;
		case ev_quaternion:
			new = new_quaternion_expr (zero);
			break;
		case ev_struct:
		case ev_object:
		case ev_class:
		case ev_sel:
		case ev_array:
			return error (e, "%s cannot be tested", pr_type_name[type]);
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

	for (i = 0; i < list->size; i++) {
		e = list->e[i];
		if (e->type == ex_uexpr && e->e.expr.op == 'g')
			e->e.expr.e1 = label;
		else if (e->type == ex_expr && (e->e.expr.op == 'i'
										|| e->e.expr.op == 'n'))
			e->e.expr.e2 = label;
		else {
			error (e, "internal compiler error");
			abort ();
		}
	}
}

static ex_list_t *
merge (ex_list_t *l1, ex_list_t *l2)
{
	ex_list_t  *m;

	if (!l1 && !l2) {
		error (0, "internal error");
		abort ();
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
		e = test_expr (e, 1);
		if (e->type == ex_error)
			return e;
		if (e->type == ex_integer) {
			b = new_unary_expr ('g', 0);
			if (e->e.integer_val)
				e = new_bool_expr (make_list (b), 0, b);
			else
				e = new_bool_expr (0, make_list (b), b);
		} else {
			b = new_block_expr ();
			append_expr (b, new_binary_expr ('i', e, 0));
			append_expr (b, new_unary_expr ('g', 0));
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
	error (e1, "internal error");
	abort ();
}

void
convert_int (expr_t *e)
{
	e->type = ex_float;
	e->e.float_val = e->e.integer_val;
}

void
convert_uint (expr_t *e)
{
	e->type = ex_float;
	e->e.float_val = e->e.uinteger_val;
}

void
convert_short (expr_t *e)
{
	e->type = ex_float;
	e->e.float_val = e->e.short_val;
}

void
convert_uint_int (expr_t *e)
{
	e->type = ex_integer;
	e->e.integer_val = e->e.uinteger_val;
}

void
convert_int_uint (expr_t *e)
{
	e->type = ex_uinteger;
	e->e.uinteger_val = e->e.integer_val;
}

void
convert_short_int (expr_t *e)
{
	e->type = ex_integer;
	e->e.integer_val = e->e.short_val;
}

void
convert_short_uint (expr_t *e)
{
	e->type = ex_uinteger;
	e->e.uinteger_val = e->e.short_val;
}

static void
convert_nil (expr_t *e, type_t *t)
{
	e->type = expr_types[t->type];
	if (e->type == ex_pointer)
		e->e.pointer.type = &type_void;
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
				 && (e2->e.expr.op == '*' || e2->e.expr.op == '/'
					 || e2->e.expr.op == '+' || e2->e.expr.op == '-'
					 || is_compare (e2->e.expr.op)))
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
		}
	} else {
		if (e2->type == ex_expr && !e2->paren) {
			if ((op == '&' || op == '|' || op == '^')
				&& is_compare (e2->e.expr.op)) {
				if (options.warnings.precedence)
					warning (e2, "suggest parentheses around comparison in "
							 "operand of %c", op);
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
		inc_users (e);					// for the block itself
		e1 = assign_expr (e, e1);
	}

	if (op == '.')
		return field_expr (e1, e2);
	check_initialized (e1);

	convert_name (e2);
	check_initialized (e2);

	if (op == OR || op == AND) {
		e1 = test_expr (e1, true);
		e2 = test_expr (e2, true);
	}

	if (e1->type == ex_error)
		return e1;
	if (e2->type == ex_error)
		return e2;
	e1 = constant_expr (e1);
	e2 = constant_expr (e2);
	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2) {
		error (e1, "internal error");
		abort ();
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

	if (e1->type == ex_short) {
		if (t2 == &type_integer) {
			convert_short_int (e1);
			t1 = &type_integer;
		} else if (t2 == &type_uinteger) {
			convert_short_uint (e1);
			t1 = &type_uinteger;
		}
	}

	if (e2->type == ex_short) {
		if (t1 == &type_integer) {
			convert_short_int (e2);
			t2 = &type_integer;
		} else if (t1 == &type_uinteger) {
			convert_short_uint (e2);
			t2 = &type_uinteger;
		}
	}

	if (e1->type == ex_integer) {
		if (t2 == &type_float
			|| t2 == &type_vector
			|| t2 == &type_quaternion) {
			convert_int (e1);
			t1 = &type_float;
		} else if (t2 == &type_uinteger) {
			convert_int_uint (e1);
			t1 = &type_uinteger;
		}
	} else if (e1->type == ex_uinteger) {
		if (t2 == &type_float
			|| t2 == &type_vector
			|| t2 == &type_quaternion) {
			convert_uint (e1);
			t1 = &type_float;
		} else if (t2 == &type_integer) {
			convert_uint_int (e1);
			t1 = &type_integer;
		}
	} else if (e2->type == ex_integer) {
		if (t1 == &type_float
			|| t1 == &type_vector
			|| t1 == &type_quaternion) {
			convert_int (e2);
			t2 = &type_float;
		} else if (t1 == &type_uinteger) {
			convert_int_uint (e2);
			t2 = &type_uinteger;
		}
	} else if (e2->type == ex_uinteger) {
		if (t1 == &type_float
			|| t1 == &type_vector
			|| t1 == &type_quaternion) {
			convert_uint (e2);
			t2 = &type_float;
		} else if (t1 == &type_integer) {
			convert_uint_int (e2);
			t2 = &type_integer;
		}
	}

	if ((e = check_precedence (op, e1, e2)))
		return e;

	if (t1 != t2) {
		switch (t1->type) {
			case ev_float:
				if (t2 == &type_vector || t2 == &type_quaternion) {
					type = &type_vector;
				} else {
					type = &type_float;
				}
				break;
			case ev_vector:
				if (t2 == &type_quaternion) {
					type = &type_quaternion;
				} else {
					type = &type_vector;
				}
				break;
			case ev_field:
				if (t1->aux_type == t2) {
					type = t1->aux_type;
				} else {
					goto type_mismatch;
				}
				break;
			case ev_func:
				if (e1->type == ex_func && !e1->e.func_val) {
					type = t2;
				} else if (e2->type == ex_func && !e2->e.func_val) {
					type = t1;
				} else {
					goto type_mismatch;
				}
				break;
			case ev_pointer:
				if (!type_assignable (t1, t2) && !type_assignable (t2, t1))
					goto type_mismatch;
				type = t1;
				break;
			default:
			  type_mismatch:
				type = t1;
				break;
				//return type_mismatch (e1, e2, op);
		}
	} else {
		type = t1;
	}
	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			type = &type_integer;
		else
			type = &type_float;
	} else if (op == '*' && t1 == &type_vector && t2 == &type_vector) {
		type = &type_float;
	}
	if (!type)
		error (e1, "internal error");

	if (options.code.progsversion == PROG_ID_VERSION) {
		switch (op) {
			case '%':
				{
					expr_t     *tmp1, *tmp2;
					e = new_block_expr ();
					if (e2->type < ex_string)
						tmp1 = new_temp_def_expr (&type_float);
					else
						tmp1 = e2;
					tmp2 = new_temp_def_expr (&type_float);
					e2 = binary_expr ('&', e2, new_float_expr (-1.0));
					e1 = binary_expr ('&', e1, new_float_expr (-1.0));
					if (tmp1 != e2)
						append_expr (e, new_bind_expr (e2, tmp1));
					append_expr (e, new_bind_expr (binary_expr ('/', e1, tmp1),
												   tmp2));
					e2 = binary_expr ('&', tmp2, new_float_expr (-1.0));
					e->e.block.result = binary_expr ('-', tmp2, e2);
					e2 = e;
					e1 = tmp1;
					op = '*';
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
	convert_name (e);
	check_initialized (e);
	if (e->type == ex_error)
		return e;
	switch (op) {
		case '-':
			switch (e->type) {
				case ex_error:
				case ex_label:
				case ex_name:
				case ex_state:
					error (e, "internal error");
					abort ();
				case ex_uexpr:
					if (e->e.expr.op == '-')
						return e->e.expr.e1;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_bool:
				case ex_def:
				case ex_temp:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->e.expr.type = (e->type == ex_def)
							? e->e.def->type : e->e.expr.type;
						return n;
					}
				case ex_short:
					e->e.short_val *= -1;
					return e;
				case ex_integer:
				case ex_uinteger:
					e->e.integer_val *= -1;
					return e;
				case ex_float:
					e->e.float_val *= -1;
					return e;
				case ex_nil:
				case ex_string:
				case ex_entity:
				case ex_field:
				case ex_func:
				case ex_pointer:
					return error (e, "invalid type for unary -");
				case ex_vector:
					e->e.vector_val[0] *= -1;
					e->e.vector_val[1] *= -1;
					e->e.vector_val[2] *= -1;
					return e;
				case ex_quaternion:
					e->e.quaternion_val[0] *= -1;
					e->e.quaternion_val[1] *= -1;
					e->e.quaternion_val[2] *= -1;
					e->e.quaternion_val[3] *= -1;
					return e;
			}
			break;
		case '!':
			switch (e->type) {
				case ex_error:
				case ex_label:
				case ex_name:
				case ex_state:
					error (e, "internal error");
					abort ();
				case ex_bool:
					return new_bool_expr (e->e.bool.false_list,
										  e->e.bool.true_list, e);
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary !");
				case ex_uexpr:
				case ex_expr:
				case ex_def:
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
				case ex_short:
					e->e.short_val = !e->e.short_val;
					return e;
				case ex_integer:
				case ex_uinteger:
					e->e.integer_val = !e->e.integer_val;
					return e;
				case ex_float:
					e->e.integer_val = !e->e.float_val;
					e->type = ex_integer;
					return e;
				case ex_string:
					e->e.integer_val = !e->e.string_val || !e->e.string_val[0];
					e->type = ex_integer;
					return e;
				case ex_vector:
					e->e.integer_val = !e->e.vector_val[0]
						&& !e->e.vector_val[1]
						&& !e->e.vector_val[2];
					e->type = ex_integer;
					return e;
				case ex_quaternion:
					e->e.integer_val = !e->e.quaternion_val[0]
						&& !e->e.quaternion_val[1]
						&& !e->e.quaternion_val[2]
						&& !e->e.quaternion_val[3];
					e->type = ex_integer;
					return e;
				case ex_entity:
				case ex_field:
				case ex_func:
				case ex_pointer:
					error (e, "internal error");
					abort ();
			}
			break;
		case '~':
			switch (e->type) {
				case ex_error:
				case ex_label:
				case ex_name:
				case ex_state:
					error (e, "internal error");
					abort ();
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
				case ex_def:
				case ex_temp:
bitnot_expr:
					if (options.code.progsversion == PROG_ID_VERSION) {
						expr_t     *n1 = new_integer_expr (-1);
						return binary_expr ('-', n1, e);
					} else {
						expr_t     *n = new_unary_expr (op, e);
						type_t     *t = get_type (e);

						if (t != &type_integer && t != &type_float)
							return error (e, "invalid type for unary ~");
						n->e.expr.type = t;
						return n;
					}
				case ex_short:
					e->e.short_val = ~e->e.short_val;
					return e;
				case ex_integer:
				case ex_uinteger:
					e->e.integer_val = ~e->e.integer_val;
					return e;
				case ex_float:
					e->e.float_val = ~(int) e->e.float_val;
					return e;
				case ex_nil:
				case ex_string:
				case ex_vector:
				case ex_quaternion:
				case ex_entity:
				case ex_field:
				case ex_func:
				case ex_pointer:
					return error (e, "invalid type for unary ~");
			}
			break;
		case '.':
			if (extract_type (e) != ev_pointer)
				return error (e, "invalid type for unary .");
			e = new_unary_expr ('.', e);
			e->e.expr.type = get_type (e->e.expr.e1)->aux_type;
			return e;
	}
	error (e, "internal error");
	abort ();
}

static expr_t *
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
	if (ftype->num_parms < -1) {
		if (-arg_count > ftype->num_parms + 1) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		parm_count = -ftype->num_parms - 1;
	} else if (ftype->num_parms >= 0) {
		if (arg_count > ftype->num_parms) {
			return error (fexpr, "too many arguments");
		} else if (arg_count < ftype->num_parms) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		parm_count = ftype->num_parms;
	}
	for (i = arg_count - 1, e = params; i >= 0; i--, e = e->next) {
		type_t     *t = get_type (e);
 
		check_initialized (e);
		if (ftype->parm_types[i] == &type_float && e->type == ex_integer) {
			convert_int (e);
			t = &type_float;
		}
		if (i < parm_count) {
			if (e->type == ex_nil)
				convert_nil (e, t = ftype->parm_types[i]);
			if (e->type == ex_bool)
				convert_from_bool (e, ftype->parm_types[i]);
			if (e->type == ex_error)
				return e;
			if (!type_assignable (ftype->parm_types[i], t)) {
				//print_type (ftype->parm_types[i]); puts ("");
				//print_type (t); puts ("");
				err = error (e, "type mismatch for parameter %d of %s",
							 i + 1, fexpr->e.def->name);
			}
			t = ftype->parm_types[i];
		} else {
			if (e->type == ex_nil)
				convert_nil (e, t = &type_vector);	//XXX largest param size
			if (e->type == ex_bool)
				convert_from_bool (e, get_type (e));
			if (e->type == ex_integer
				&& options.code.progsversion == PROG_ID_VERSION)
				convert_int (e);
			if (e->type == ex_integer && options.warnings.vararg_integer)
				warning (e, "passing integer consant into ... function");
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
		// new_binary_expr calls inc_users for both args, but in_users doesn't
		// walk expression chains so only the first arg expression in the chain
		// (the last arg in the call) gets its user count incremented, thus
		// ensure all other arg expressions get their user counts incremented.
		if (a != &args)
			inc_users (*a);
		a = &(*a)->next;
	}
	for (i = 0; i < arg_expr_count - 1; i++) {
		append_expr (call, assign_expr (arg_exprs[i][1], arg_exprs[i][0]));
	}
	if (arg_expr_count) {
		e = new_bind_expr (arg_exprs[arg_expr_count - 1][0],
						   arg_exprs[arg_expr_count - 1][1]);
		inc_users (arg_exprs[arg_expr_count - 1][0]);
		inc_users (arg_exprs[arg_expr_count - 1][1]);
		append_expr (call, e);
	}
	e = new_binary_expr ('c', fexpr, args);
	e->e.expr.type = ftype->aux_type;
	append_expr (call, e);
	if (ftype->aux_type != &type_void) {
		call->e.block.result = new_ret_expr (ftype->aux_type);
	} else if (options.traditional) {
		call->e.block.result = new_ret_expr (&type_float);
	}
	return call;
}

expr_t *
function_expr (expr_t *fexpr, expr_t *params)
{
	type_t     *ftype;

	ftype = get_type (fexpr);

	if (fexpr->type == ex_error)
		return fexpr;
	if (ftype->type != ev_func) {
		if (fexpr->type == ex_def)
			return error (fexpr, "Called object \"%s\" is not a function",
						  fexpr->e.def->name);
		else
			return error (fexpr, "Called object is not a function");
	}

	if (fexpr->type == ex_def && params && params->type == ex_string) {
		// FIXME eww, I hate this, but it's needed :(
		// FIXME make a qc hook? :)
		def_t      *func = fexpr->e.def;
		def_t      *e = ReuseConstant (params, 0);

		if (strncmp (func->name, "precache_sound", 14) == 0)
			PrecacheSound (e, func->name[14]);
		else if (strncmp (func->name, "precache_model", 14) == 0)
			PrecacheModel (e, func->name[14]);
		else if (strncmp (func->name, "precache_file", 13) == 0)
			PrecacheFile (e, func->name[13]);
	}

	return build_function_call (fexpr, ftype, params);
}

expr_t *
return_expr (function_t *f, expr_t *e)
{
	type_t     *t;

	if (!e) {
		if (f->def->type->aux_type != &type_void) {
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
	if (f->def->type->aux_type == &type_void) {
		if (!options.traditional)
			return error (e, "returning a value for a void function");
		if (options.warnings.traditional)
			warning (e, "returning a value for a void function");
	}
	if (e->type == ex_bool)
		e = convert_from_bool (e, f->def->type->aux_type);
	if (f->def->type->aux_type == &type_float && e->type == ex_integer) {
		e->type = ex_float;
		e->e.float_val = e->e.integer_val;
		t = &type_float;
	}
	if (t == &type_void) {
		if (e->type == ex_nil) {
			t = f->def->type->aux_type;
			e->type = expr_types[t->type];
		} else {
			if (!options.traditional)
				return error (e, "void value not ignored as it ought to be");
			if (options.warnings.traditional)
				warning (e, "void value not ignored as it ought to be");
			//FIXME does anything need to be done here?
		}
	}
	if (!type_assignable (f->def->type->aux_type, t)) {
		if (!options.traditional)
			return error (e, "type mismatch for return value of %s",
						  f->def->name);
		if (options.warnings.traditional)
			warning (e, "type mismatch for return value of %s",
					 f->def->name);
	} else {
		if (f->def->type->aux_type != t)
			e = cast_expr (f->def->type->aux_type, e);
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
	append_expr (block, new_unary_expr ('g', elabel));
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
	expr_t     *incop;

	if (e->type == ex_error)
		return e;

	one = new_integer_expr (1);		// integer constants get auto-cast to float
	incop = asx_expr (op, e, one);
	if (postop) {
		expr_t     *temp;
		type_t     *type = get_type (e);
		expr_t     *block = new_block_expr ();

		temp = new_temp_def_expr (type);
		append_expr (block, assign_expr (temp, e));
		append_expr (block, incop);
		block->e.block.result = temp;
		return block;
	}
	return incop;
}

expr_t *
array_expr (expr_t *array, expr_t *index)
{
	type_t     *array_type = get_type (array);
	type_t     *index_type = get_type (index);
	expr_t     *scale;
	expr_t     *e;
	int         size;

	if (array->type == ex_error)
		return array;
	if (index->type == ex_error)
		return index;

	if (array_type->type != ev_pointer && array_type->type != ev_array)
		return error (array, "not an array");
	if (index_type != &type_integer && index_type != &type_uinteger)
		return error (index, "invalid array index type");
	if (array_type->num_parms
		&& index->type >= ex_integer
		&& index->e.uinteger_val >= (unsigned int) array_type->num_parms)
			return error (index, "array index out of bounds");
	size = type_size (array_type->aux_type);
	if (size > 1) {
		scale = new_expr ();
		scale->type = expr_types[index_type->type];
		scale->e.integer_val = size;
		index = binary_expr ('*', index, scale);
	}
	if ((index->type == ex_integer
		 && index->e.integer_val < 32768 && index->e.integer_val >= -32768)
		|| (index->type == ex_uinteger
			&& index->e.uinteger_val < 32768)) {
		index->type = ex_short;
	}
	if (array_type->type == ev_array) {
		e = address_expr (array, index, array_type->aux_type);
	} else {
		if (index->type != ex_short || index->e.integer_val) {
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
address_expr (expr_t *e1, expr_t *e2, type_t *t)
{
	expr_t     *e;
	type_t     *type;

	if (e1->type == ex_error)
		return e1;

	if (!t)
		t = get_type (e1);

	switch (e1->type) {
		case ex_def:
			{
				def_t      *def = e1->e.def;
				def->used = 1;
				type = def->type;
				if (type->type == ev_struct || type->type == ev_class) {
					e = new_expr ();
					e->line = e1->line;
					e->file = e1->file;
					e->type = ex_pointer;
					e->e.pointer.val = 0;
					e->e.pointer.type = t;
					e->e.pointer.def = def;
				} else if (type->type == ev_array) {
					e = e1;
					e->type = ex_pointer;
					e->e.pointer.val = 0;
					e->e.pointer.type = t;
					e->e.pointer.def = def;
				} else {
					e = new_unary_expr ('&', e1);
					e->e.expr.type = pointer_type (t);
				}
				break;
			}
		case ex_expr:
			if (e1->e.expr.op == '.') {
				e = e1;
				e->e.expr.op = '&';
				e->e.expr.type = pointer_type (e->e.expr.type);
				break;
			} else if (e1->e.expr.op == 'b') {
				e = new_unary_expr ('&', e1);
				e->e.expr.type = pointer_type (e1->e.expr.type);
				break;
			}
			return error (e1, "invalid type for unary &");
		case ex_uexpr:
			if (e1->e.expr.op == '.') {
				e = e1->e.expr.e1;
				if (e->type == ex_expr && e->e.expr.op == '.') {
					e->e.expr.type = e->e.expr.type;
					e->e.expr.op = '&';
				}
				break;
			}
			return error (e1, "invalid type for unary &");
		default:
			return error (e1, "invalid type for unary &");
	}
	if (e2) {
		if (e2->type == ex_error)
			return e2;
		if (e->type == ex_pointer && e2->type == ex_short) {
			e->e.pointer.val += e2->e.short_val;
		} else {
			if (e2->type != ex_short || e2->e.short_val) {
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

static inline int
is_indirect (expr_t *e)
{
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (!(e->type == ex_uexpr && e->e.expr.op == '.'))
		return 0;
	e = e->e.expr.e1;
	if (e->type != ex_pointer
		|| !(POINTER_VAL (e->e.pointer) >= 0
			 && POINTER_VAL (e->e.pointer) < 65536)) {
		return 1;
	}
	return 0;
}

static inline int
is_lvalue (expr_t *e)
{
	if (e->type == ex_def || e->type == ex_temp)
		return 1;
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (e->type == ex_uexpr && e->e.expr.op == '.')
		return 1;
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

	if (e1->type == ex_def)
		def_initialized (e1->e.def);

	if (!is_lvalue (e1)) {
		if (options.traditional)
			warning (e1, "invalid lvalue in assignment");
		else
			return error (e1, "invalid lvalue in assignment");
	}
	//XXX func = func ???
	check_initialized (e2);
	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2) {
		error (e1, "internal error");
		abort ();
	}
	if (e2->type == ex_bool)
		e2 = convert_from_bool (e2, t1);

	if (t1->type != ev_void && e2->type == ex_nil) {
		t2 = t1;
		convert_nil (e2, t2);
	}

	e2->rvalue = 1;

	if (!type_assignable (t1, t2)) {
		if (!options.traditional || t1->type != ev_func || t2->type != ev_func)
			return type_mismatch (e1, e2, op);
		if (options.warnings.traditional)
			warning (e1, "assignment between disparate function types");
	}
	type = t1;
	if (is_indirect (e1) && is_indirect (e2)) {
		if (extract_type (e2) == ev_struct) {
			e1 = address_expr (e1, 0, 0);
			e2 = address_expr (e2, 0, 0);
			e = new_move_expr (e1, e2, get_type (e2));
		} else {
			expr_t     *temp = new_temp_def_expr (t1);

			e = new_block_expr ();
			append_expr (e, assign_expr (temp, e2));
			append_expr (e, assign_expr (e1, temp));
			e->e.block.result = temp;
		}
		return e;
	} else if (is_indirect (e1)) {
		if (extract_type (e1) == ev_struct) {
			e1 = address_expr (e1, 0, 0);
			return new_move_expr (e1, e2, get_type (e2));
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
			if (e->type != ex_pointer
				|| !(POINTER_VAL (e->e.pointer) > 0
					 && POINTER_VAL (e->e.pointer) < 65536)) {
				e1 = e;
				op = PAS;
			}
		}
	} else if (is_indirect (e2)) {
		if (extract_type (e1) == ev_struct) {
			e2 = address_expr (e2, 0, 0);
			e2->rvalue = 1;
			return new_move_expr (e1, e2, get_type (e2));
		}
		if (e2->type == ex_uexpr) {
			e = e2->e.expr.e1;
			if (e->type != ex_pointer
				|| !(POINTER_VAL (e->e.pointer) > 0
					 && POINTER_VAL (e->e.pointer) < 65536)) {
				if (e->type == ex_expr && e->e.expr.op == '&'
					&& e->e.expr.type->type == ev_pointer
					&& e->e.expr.e1->type < ex_string) {
					e2 = e;
					e2->e.expr.op = '.';
					e2->e.expr.type = t2;
					e2->rvalue = 1;
				}
			}
		}
	}
	if (extract_type (e1) == ev_struct) {
		return new_move_expr (e1, e2, get_type (e2));
	}
	if (!type)
		error (e1, "internal error");

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

	check_initialized (e);

	e_type = get_type (e);

	if (type == e_type)
		return e;

	if (!(type->type == ev_pointer
		  && (e_type->type == ev_pointer
			  || e_type == &type_integer || e_type == &type_uinteger))
		&& !(type->type == ev_func && e_type->type == ev_func)
		&& !(((type == &type_integer || type == &type_uinteger)
			  && (e_type == &type_float || e_type == &type_integer
				  || e_type == &type_uinteger || e_type->type == ev_pointer))
			 || (type == &type_float
				 && (e_type == &type_integer || e_type == &type_uinteger)))) {
		return  error (e, "can not cast from %s to %s",
					   pr_type_name[extract_type (e)],
					   pr_type_name[type->type]);
	}
	if (e->type == ex_uexpr && e->e.expr.op == '.') {
		e->e.expr.type = type;
		c = e;
	} else {
		c = new_unary_expr ('C', e);
		c->e.expr.type = type;
	}
	return c;
}

void
init_elements (def_t *def, expr_t *eles)
{
	expr_t     *e, *c;
	int         count, i, num_params;
	pr_type_t  *g;
	def_t      *elements;

	if (def->type->type == ev_array) {
		elements = calloc (def->type->num_parms, sizeof (def_t));
		for (i = 0; i < def->type->num_parms; i++) {
			elements[i].type = def->type->aux_type;
			elements[i].ofs = def->ofs + i * type_size (def->type->aux_type);
		}
		num_params = i;
	} else if (def->type->type == ev_struct) {
		struct_field_t *field;

		for (i = 0, field = def->type->s.strct->struct_head; field;
			 i++, field = field->next)
			;
		elements = calloc (i, sizeof (def_t));
		for (i = 0, field = def->type->s.strct->struct_head; field;
			 i++, field = field->next) {
			elements[i].type = field->type;
			elements[i].ofs = def->ofs + field->offset;
		}
		num_params = i;
	} else {
		error (eles, "invalid initializer");
		return;
	}
	for (count = 0, e = eles->e.block.head; e; count++, e = e->next)
		if (e->type == ex_error) {
			free (elements);
			return;
		}
	if (count > num_params) {
		if (options.warnings.initializer)
			warning (eles, "excessive elements in initializer");
		count = num_params;
	}
	for (i = 0, e = eles->e.block.head; i < count; i++, e = e->next) {
		g = G_POINTER (pr_type_t, elements[i].ofs);
		c = constant_expr (e);
		if (c->type == ex_block) {
			if (elements[i].type->type != ev_array
				&& elements[i].type->type != ev_struct) {
				error (e, "type mismatch in initializer");
				continue;
			}
			init_elements (&elements[i], c);
		} else if (c->type >= ex_string) {
			if (c->type == ex_integer
				&& elements[i].type->type == ev_float)
				convert_int (c);
			else if (c->type == ex_integer
					 && elements[i].type->type == ev_uinteger)
				convert_int_uint (c);
			else if (c->type == ex_uinteger
					 && elements[i].type->type == ev_float)
				convert_uint (c);
			else if (c->type == ex_uinteger
					 && elements[i].type->type == ev_integer)
				convert_uint_int (c);
			if (get_type (c) != elements[i].type) {
				error (e, "type mismatch in initializer");
				continue;
			}
			if (c->type == ex_string) {
				EMIT_STRING (g->string_var, c->e.string_val);
			} else {
				memcpy (g, &c->e, type_size (get_type (c)) * 4);
			}
		} else {
			error (e, "non-constant initializer");
		}
	}
	free (elements);
}

expr_t *
selector_expr (keywordarg_t *selector)
{
	dstring_t  *sel_id = dstring_newstr ();
	dstring_t  *sel_types = dstring_newstr ();
	expr_t     *sel;
	def_t      *sel_def;
	int         index;

	selector = copy_keywordargs (selector);
	selector = (keywordarg_t *) reverse_params ((param_t *) selector);
	selector_name (sel_id, selector);
	selector_types (sel_types, selector);
	//printf ("'%s' '%s'\n", sel_id->str, sel_types->str);
	index = selector_index (sel_id->str, sel_types->str);
	index *= type_size (type_SEL.aux_type);
	sel_def = get_def (type_SEL.aux_type, "_OBJ_SELECTOR_TABLE", pr.scope,
					   st_extern);
	sel = new_def_expr (sel_def);
	dstring_delete (sel_id);
	dstring_delete (sel_types);
	return address_expr (sel, new_short_expr (index), 0);
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
	def_t      *super_d;
	expr_t     *super;
	expr_t     *e;
	expr_t     *super_block;
	class_t    *class;
	class_type_t _class_type;

	if (!class_type)
		return error (new_expr (),
					  "`super' used outside of class implementation");

	if (class_type->is_class)
		class = class_type->c.class;
	else
		class = class_type->c.category->class;

	if (!class->super_class)
		return error (new_expr (), "%s has no super class", class->name);

	super_d = get_def (type_Super.aux_type, ".super", current_func->scope,
					   st_local);
	def_initialized (super_d);
	super = new_def_expr (super_d);
	super_block = new_block_expr ();

	e = assign_expr (binary_expr ('.', super, new_name_expr ("self")),
								  new_name_expr ("self"));
	append_expr (super_block, e);

	_class_type.is_class = 1;
	_class_type.c.class = class;
	e = new_def_expr (class_def (&_class_type, 1));
	e = assign_expr (binary_expr ('.', super, new_name_expr ("class")),
					 binary_expr ('.', e, new_name_expr ("super_class")));
	append_expr (super_block, e);

	e = address_expr (super, 0, &type_void);
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
	int         super = 0, class_msg = 0;
	type_t     *rec_type;
	class_t    *class;
	method_t   *method;

	if (receiver->type == ex_name
		&& strcmp (receiver->e.string_val, "super") == 0) {
		super = 1;

		receiver = super_expr (current_class);

		if (receiver->type == ex_error)
			return receiver;
		if (current_class->is_class)
			class = current_class->c.class;
		else
			class = current_class->c.category->class;
		rec_type = class->type;
	} else {
		if (receiver->type == ex_name && get_class (receiver->e.string_val, 0))
			class_msg = 1;
		rec_type = get_type (receiver);

		if (receiver->type == ex_error)
			return receiver;

		if (rec_type->type != ev_pointer
			|| (rec_type->aux_type->type != ev_object
				&& rec_type->aux_type->type != ev_class))
			return error (receiver, "not a class/object");
		class = rec_type->aux_type->s.class;
	}

	method = class_message_response (class, class_msg, selector);
	if (method)
		rec_type = method->type->aux_type;

	for (m = message; m; m = m->next) {
		*a = m->expr;
		while ((*a))
			a = &(*a)->next;
	}
	*a = selector;
	a = &(*a)->next;
	*a = receiver;

	if (method) {
		expr_t      *err;
		if ((err = method_check_params (method, args)))
			return err;
		call = build_function_call (send_message (super), method->type, args);
	} else {
		call = build_function_call (send_message (super), &type_IMP, args);
	}

	if (call->type == ex_error)
		return receiver;

	call->e.block.result = new_ret_expr (rec_type);
	return call;
}

expr_t *
sizeof_expr (expr_t *expr, struct type_s *type)
{
	if (!((!expr) ^ (!type))) {
		error (0, "internal error");
		abort ();
	}
	if (!type)
		type = get_type (expr);
	expr = new_integer_expr (type_size (type));
	return expr;
}

static void
report_function (expr_t *e)
{
	static function_t *last_func = (function_t *)-1L;
	string_t    file = pr.source_file;

	if (e)
		file = e->file;

	if (current_func != last_func) {
		if (current_func) {
			fprintf (stderr, "%s: In function `%s':\n", G_GETSTR (file),
					 current_func->def->name);
		} else {
			fprintf (stderr, "%s: At top level:\n", G_GETSTR (file));
		}
	}
	last_func = current_func;
}

static void
_warning (expr_t *e, const char *fmt, va_list args)
{
	string_t    file = pr.source_file;
	int         line = pr.source_line;

	report_function (e);
	if (options.warnings.promote) {
		options.warnings.promote = 0;	// only want to do this once
		fprintf (stderr, "%s: warnings treated as errors\n", "qfcc");
		pr.error_count++;
	}

	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: warning: ", G_GETSTR (file), line);
	vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
}

void
notice (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	if (options.notices.silent)
		return;

	va_start (args, fmt);
	if (options.notices.promote) {
		_warning (e, fmt, args);
	} else {
		string_t    file = pr.source_file;
		int         line = pr.source_line;

		report_function (e);
		if (e) {
			file = e->file;
			line = e->line;
		}
		fprintf (stderr, "%s:%d: notice: ", G_GETSTR (file), line);
		vfprintf (stderr, fmt, args);
		fputs ("\n", stderr);
	}
	va_end (args);
}

void
warning (expr_t *e, const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	_warning (e, fmt, args);
	va_end (args);
}

expr_t *
error (expr_t *e, const char *fmt, ...)
{
	va_list     args;
	string_t    file = pr.source_file;
	int         line = pr.source_line;

	report_function (e);
	va_start (args, fmt);
	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: ", G_GETSTR (file), line);
	vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
	va_end (args);
	pr.error_count++;

	if (e) {
		e->type = ex_error;
	}
	return e;
}
