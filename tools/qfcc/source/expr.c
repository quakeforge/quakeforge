/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
static const char rcsid[] = 
	"$Id$";

#include <stdlib.h>

#include <QF/mathlib.h>
#include <QF/sys.h>
#include <QF/va.h>

#include "qfcc.h"
#include "scope.h"
#include "qc-parse.h"

extern function_t *current_func;

int lineno_base;

etype_t qc_types[] = {
	ev_void,		// ex_label
	ev_void,		// ex_block
	ev_void,		// ex_expr
	ev_void,		// ex_uexpr
	ev_void,		// ex_def
	ev_void,		// ex_temp
	ev_void,		// ex_nil

	ev_string,		// ex_string
	ev_float,		// ex_float
	ev_vector,		// ex_vector
	ev_entity,		// ex_entity
	ev_field,		// ex_field
	ev_func,		// ex_func
	ev_pointer,		// ex_pointer
	ev_quaternion,	// ex_quaternion
	ev_integer,		// ex_integer
};

type_t *types[] = {
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
};

expr_type expr_types[] = {
	ex_nil,			// ev_void
	ex_string,		// ev_string
	ex_float,		// ev_float
	ex_vector,		// ev_vector
	ex_entity,		// ev_entity
	ex_field,		// ev_field
	ex_func,		// ev_func
	ex_pointer,		// ev_pointer
	ex_quaternion,	// ev_quaternion
	ex_integer,		// ev_integer
};

const char *type_names[] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"quaternion",
	"int",
};

type_t *
get_type (expr_t *e)
{
	switch (e->type) {
		case ex_label:
			return 0;		// something went very wrong
		case ex_nil:
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
		case ex_pointer:
		case ex_quaternion:
			return types[qc_types[e->type]];
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
error (expr_t *e, const char *fmt, ...)
{
	va_list     args;
	string_t file = s_file;
	int line = pr_source_line;
	
	va_start (args, fmt);
	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: ", strings + file, line);
	vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
	va_end (args);
	pr_error_count++;

	if (e) {
		e = new_expr ();
		e->type = ex_integer;
	}
	return e;
}

void
warning (expr_t *e, const char *fmt, ...)
{
	va_list     args;
	string_t file = s_file;
	int line = pr_source_line;

	if (options.warnings.promote) {
		options.warnings.promote = 0;		// only want to do this once
		fprintf (stderr, "%s: warnings treated as errors\n", "qfcc");
		pr_error_count++;
	}
	
	va_start (args, fmt);
	if (e) {
		file = e->file;
		line = e->line;
	}
	fprintf (stderr, "%s:%d: warning: ", strings + file, line);
	vfprintf (stderr, fmt, args);
	fputs ("\n", stderr);
	va_end (args);
}

const char *
get_op_string (int op)
{
	switch (op) {
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
		case '(':	return "(";
		case '.':	return ".";
		case 'i':	return "<if>";
		case 'n':	return "<ifnot>";
		case 'g':	return "<goto>";
		case 'r':	return "<return>";
		default:	return "unknown";
	}
}

expr_t *
type_mismatch (expr_t *e1, expr_t *e2, int op)
{
	etype_t t1, t2;

	t1 = extract_type (e1);
	t2 = extract_type (e2);

	return error (e1, "type mismatch: %s %s %s",
				  type_names[t1], get_op_string (op), type_names[t2]);
}

void
check_initialized (expr_t *e)
{
	if (options.warnings.uninited_variable) {
		if (e->type == ex_def
			&& !(e->e.def->type->type == ev_func && !e->e.def->scope)
			&& !e->e.def->initialized) {
			warning (e, "%s may be used uninitialized", e->e.def->name);
			e->e.def->initialized = 1; // only warn once
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
	expr_t *e = calloc (1, sizeof (expr_t));
	e->line = pr_source_line;
	e->file = s_file;
	return e;
}

static int
num_digits (int val)
{
	int         num = 1;

	printf ("%d ", val);
	if (val < 0) {
		num++;
		val = -val;
	}
	while (val > 9) {
		val /= 10;
		num++;
	}
	printf ("%d\n", num);
	return num;
}

expr_t *
new_label_expr (void)
{
	static int  label = 0;
	int         lnum = ++label;
	const char *fname = current_func->def->name;
	int         len = 1 + strlen (fname) + 1 + num_digits (lnum) + 1;

	expr_t *l = new_expr ();
	l->type = ex_label;
	l->e.label.name = malloc (len);
	if (!l->e.label.name)
		Sys_Error ("new_label_expr: Memory Allocation Failure\n");
	sprintf (l->e.label.name, "$%s_%d", fname, lnum);
	return l;
}

expr_t *
new_block_expr (void)
{
	expr_t *b = new_expr ();

	b->type = ex_block;
	b->e.block.head = 0;
	b->e.block.tail = &b->e.block.head;
	return b;
}

expr_t *
new_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	expr_t *e = new_expr ();

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
	expr_t *e = new_expr ();

	inc_users (e1);

	e->type = ex_uexpr;
	e->e.expr.op = op;
	e->e.expr.e1 = e1;
	return e;
}

expr_t *
new_temp_def_expr (type_t *type)
{
	expr_t *e = new_expr ();

	e->type = ex_temp;
	e->e.temp.type = type;
	return e;
}

expr_t *
append_expr (expr_t *block, expr_t *e)
{
	if (block->type != ex_block)
		abort ();

	if (!e)
		return block;

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
				puts("");
			}
			printf ("}");
			break;
		case ex_expr:
			print_expr (e->e.expr.e1);
			if (e->e.expr.op == 'c') {
				expr_t *p = e->e.expr.e2;
				printf ("(");
				while (p) {
					print_expr (p);
					if (p->next)
						printf (",");
					p = p->next;
				}
				printf (")");
			} else if (e->e.expr.op == 'b') {
				print_expr (e->e.expr.e1);
				printf ("<-->");
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
			if (e->e.def->scope) {
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
					printf("%s", e->e.temp.def->name);
				} else {
					printf("<%d>", e->e.temp.def->ofs);
				}
			} else {
				printf("<>");
			}
			printf (":%s:%d)@", type_names [e->e.temp.type->type],
					e->e.temp.users);
			break;
		case ex_nil:
			printf ("NULL");
			break;
		case ex_string:
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
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_pointer:
		case ex_integer:
			printf ("%d", e->e.integer_val);
			break;
	}
}

static expr_t *
do_op_string (int op, expr_t *e1, expr_t *e2)
{
	int len;
	char *buf;
	char *s1, *s2;

	s1 = e1->e.string_val ? e1->e.string_val : "";
	s2 = e2->e.string_val ? e2->e.string_val : "";
	
	switch (op) {
		case '+':
			len = strlen (s1) + strlen (s2) + 1;
			buf = malloc (len);
			if (!buf)
				Sys_Error ("do_op_string: Memory Allocation Failure\n");
			strcpy (buf, s1);
			strcat (buf, s2);
			e1->e.string_val = buf;
			break;
		case LT:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) < 0;
			break;
		case GT:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) > 0;
			break;
		case LE:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) <= 0;
			break;
		case GE:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) >= 0;
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) == 0;
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = strcmp (s1, s2) != 0;
			break;
		default:
			return error (e1, "invalid operand for string");
	}
	return e1;
}

static expr_t *
do_op_float (int op, expr_t *e1, expr_t *e2)
{
	float f1, f2;

	f1 = e1->e.float_val;
	f2 = e2->e.float_val;
	
	switch (op) {
		case '+':
			e1->e.float_val += f2;
			break;
		case '-':
			e1->e.float_val -= f2;
			break;
		case '*':
			e1->e.float_val *= f2;
			break;
		case '/':
			e1->e.float_val /= f2;
			break;
		case '&':
			e1->e.float_val = (int)f1 & (int)f2;
			break;
		case '|':
			e1->e.float_val = (int)f1 | (int)f2;
			break;
		case '^':
			e1->e.float_val = (int)f1 ^ (int)f2;
			break;
		case '%':
			e1->e.float_val = (int)f1 % (int)f2;
			break;
		case SHL:
			e1->e.float_val = (int)f1 << (int)f2;
			break;
		case SHR:
			e1->e.float_val = (int)f1 >> (int)f2;
			break;
		case AND:
			e1->type = ex_integer;
			e1->e.integer_val = f1 && f2;
			break;
		case OR:
			e1->type = ex_integer;
			e1->e.integer_val = f1 || f2;
			break;
		case LT:
			e1->type = ex_integer;
			e1->e.integer_val = f1 < f2;
			break;
		case GT:
			e1->type = ex_integer;
			e1->e.integer_val = f1 > f2;
			break;
		case LE:
			e1->type = ex_integer;
			e1->e.integer_val = f1 <= f2;
			break;
		case GE:
			e1->type = ex_integer;
			e1->e.integer_val = f1 >= f2;
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = f1 == f2;
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = f1 != f2;
			break;
		default:
			return error (e1, "invalid operand for float");
	}
	return e1;
}

static expr_t *
do_op_vector (int op, expr_t *e1, expr_t *e2)
{
	float *v1, *v2;

	v1 = e1->e.vector_val;
	v2 = e2->e.vector_val;
	
	switch (op) {
		case '+':
			VectorAdd (v1, v2, v1);
			break;
		case '-':
			VectorSubtract (v1, v2, v1);
			break;
		case '*':
			e1->type = ex_float;
			e1->e.float_val = DotProduct (v1, v2);
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = (v1[0] == v2[0])
							&& (v1[1] == v2[1])
							&& (v1[2] == v2[2]);
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = (v1[0] == v2[0])
							|| (v1[1] != v2[1])
							|| (v1[2] != v2[2]);
			break;
		default:
			return error (e1, "invalid operand for vector");
	}
	return e1;
}

static expr_t *
do_op_integer (int op, expr_t *e1, expr_t *e2)
{
	int i1, i2;

	i1 = e1->e.integer_val;
	i2 = e2->e.integer_val;
	
	switch (op) {
		case '+':
			e1->e.integer_val += i2;
			break;
		case '-':
			e1->e.integer_val -= i2;
			break;
		case '*':
			e1->e.integer_val *= i2;
			break;
		case '/':
			warning (e2, "%d / %d == %d", i1, i2, i1 / i2);
			e1->e.integer_val /= i2;
			break;
		case '&':
			e1->e.integer_val = i1 & i2;
			break;
		case '|':
			e1->e.integer_val = i1 | i2;
			break;
		case '^':
			e1->e.integer_val = i1 ^ i2;
			break;
		case '%':
			e1->e.integer_val = i1 % i2;
			break;
		case SHL:
			e1->e.integer_val = i1 << i2;
			break;
		case SHR:
			e1->e.integer_val = i1 >> i2;
			break;
		case AND:
			e1->e.integer_val = i1 && i2;
			break;
		case OR:
			e1->e.integer_val = i1 || i2;
			break;
		case LT:
			e1->type = ex_integer;
			e1->e.integer_val = i1 < i2;
			break;
		case GT:
			e1->type = ex_integer;
			e1->e.integer_val = i1 > i2;
			break;
		case LE:
			e1->type = ex_integer;
			e1->e.integer_val = i1 <= i2;
			break;
		case GE:
			e1->type = ex_integer;
			e1->e.integer_val = i1 >= i2;
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = i1 == i2;
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = i1 != i2;
			break;
		default:
			return error (e1, "invalid operand for integer");
	}
	return e1;
}

static expr_t *
do_op_huh (int op, expr_t *e1, expr_t *e2)
{
	return error (e1, "funny constant");
}

static expr_t *(*do_op[]) (int op, expr_t *e1, expr_t *e2) = {
	do_op_huh,
	do_op_string,
	do_op_float,
	do_op_vector,
	do_op_huh,
	do_op_huh,
	do_op_huh,
	do_op_huh,
	do_op_huh,
	do_op_integer,
};

static expr_t *
binary_const (int op, expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;
	//expr_t *e;

	t1 = extract_type (e1);
	t2 = extract_type (e2);

	if (t1 == t2) {
		return do_op[t1](op, e1, e2);
	} else {
		return type_mismatch (e1, e2, op);
	}
}

static expr_t *
field_expr (expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;
	expr_t *e;

	t1 = extract_type (e1);
	t2 = extract_type (e2);

	if (t1 != ev_entity || t2 != ev_field) {
		return error (e1, "type missmatch for .");
	}

	e = new_binary_expr ('.', e1, e2);
	e->e.expr.type = (e2->type == ex_def)
					 ? e2->e.def->type->aux_type
					 : e2->e.expr.type;
	return e;
}

expr_t *
test_expr (expr_t *e, int test)
{
	expr_t *new = 0;

	check_initialized (e);

	if (!test)
		return unary_expr ('!', e);

	switch (extract_type (e)) {
		case ev_type_count:
			error (e, "internal error");
			abort ();
		case ev_void:
			error (e, "void has no value");
			break;
		case ev_string:
			new = new_expr ();
			new->type = ex_string;
			break;
		case ev_integer:
			return e;
		case ev_float:
			if (options.code.progsversion == PROG_ID_VERSION)
				return e;
			new = new_expr ();
			new->type = ex_float;
			break;
		case ev_vector:
			new = new_expr ();
			new->type = ex_vector;
			break;
		case ev_entity:
			new = new_expr ();
			new->type = ex_entity;
			break;
		case ev_field:
			new = new_expr ();
			new->type = ex_field;
			break;
		case ev_func:
			new = new_expr ();
			new->type = ex_func;
			break;
		case ev_pointer:
			new = new_expr ();
			new->type = ex_pointer;
			break;
		case ev_quaternion:
			new = new_expr ();
			new->type = ex_quaternion;
			break;
	}
	new->line = e->line;
	new->file = e->file;
	new = binary_expr (NE, e, new);
	new->line = e->line;
	new->file = e->file;
	return new;
}

void
convert_int (expr_t *e)
{
	e->type = ex_float;
	e->e.float_val = e->e.integer_val;
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	type_t     *type = 0;
	expr_t     *e;

	if (op != '=')
		check_initialized (e1);
	check_initialized (e2);

	if (op == '=' && e1->type == ex_def)
		PR_DefInitialized (e1->e.def);

	if (e1->type == ex_block && e1->e.block.is_call
		&& e2->type == ex_block && e2->e.block.is_call
		&& e1->e.block.result) {
		e = new_temp_def_expr (e1->e.block.result->e.def->type);
		inc_users (e);
		e1 = binary_expr ('=', e, e1);
	}

	if (op == '.')
		return field_expr (e1, e2);

	if (op == OR || op == AND) {
		e1 = test_expr (e1, true);
		e2 = test_expr (e2, true);
	}

	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2) {
		error (e1, "internal error");
		abort ();
	}

	if (e1->type == ex_integer
		&& (t2 == &type_float
			|| t2 == &type_vector
			|| t2 == &type_quaternion)) {
		convert_int (e1);
		t1 = &type_float;
	} else if (e2->type == ex_integer
			   && (t1 == &type_float
				   || t1 == &type_vector
				   || t1 == &type_quaternion)) {
		convert_int (e2);
		t2 = &type_float;
	}

	if (e1->type >= ex_string && e2->type >= ex_string)
		return binary_const (op, e1, e2);

	if ((op == '&' || op == '|')
		&& e1->type == ex_uexpr && e1->e.expr.op == '!' && !e1->paren) {
		warning (e1, "ambiguous logic. Suggest explicit parentheses with expressions involving ! and %c", op);
	}

	if (op == '=' && t1->type != ev_void && e2->type == ex_nil) {
		t2 = t1;
		e2->type = expr_types[t2->type];
	}

	if (t1 != t2) {
		switch (t1->type) {
			case ev_float:
				if (t2 == &type_vector && op == '*') {
					type = &type_vector;
				} else {
					goto type_mismatch;
				}
				break;
			case ev_vector:
				if (t2 == &type_float && op == '*') {
					type = &type_vector;
				} else {
					goto type_mismatch;
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
			default:
type_mismatch:
				return type_mismatch (e1, e2, op);
		}
	} else {
		type = t1;
	}
	if ((op >= OR && op <= GT) || op == '>' || op == '<') {
		if (options.code.progsversion > PROG_ID_VERSION)
			type = &type_integer;
		else
			type = &type_float;
	} else if (op == '*' && t1 == &type_vector && t2 == &type_vector) {
		type = &type_float;
	}
	if (op == '=' && e1->type == ex_expr && e1->e.expr.op == '.') {
		type_t      new;

		memset (&new, 0, sizeof (new));
		new.type = ev_pointer;
		type = new.aux_type = e1->e.expr.type;
		e1->e.expr.type = PR_FindType (&new);
	}
	if (!type)
		error (e1, "internal error");

	e = new_binary_expr (op, e1, e2);
	e->e.expr.type = type;
	return e;
}

expr_t *
asx_expr (int op, expr_t *e1, expr_t *e2)
{
	expr_t *e = new_expr ();
	*e = *e1;
	return binary_expr ('=', e, binary_expr (op, e1, e2));
}

expr_t *
unary_expr (int op, expr_t *e)
{
	check_initialized (e);
	switch (op) {
		case '-':
			switch (e->type) {
				case ex_label:
					error (e, "internal error");
					abort ();
				case ex_uexpr:
					if (e->e.expr.op == '-')
						return e->e.expr.e1;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_def:
				case ex_temp:
					{
						expr_t *n = new_unary_expr (op, e);
						n->e.expr.type = (e->type == ex_def)
										 ? e->e.def->type
										 : e->e.expr.type;
						return n;
					}
				case ex_integer:
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
				case ex_label:
					abort ();
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary !");
				case ex_uexpr:
				case ex_expr:
				case ex_def:
				case ex_temp:
					{
						expr_t *n = new_unary_expr (op, e);
						if (options.code.progsversion > PROG_ID_VERSION)
							n->e.expr.type = &type_integer;
						else
							n->e.expr.type = &type_float;
						return n;
					}
				case ex_nil:
					return error (e, "invalid type for unary !");
				case ex_integer:
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
				case ex_label:
					abort ();
				case ex_uexpr:
					if (e->e.expr.op == '~')
						return e->e.expr.e1;
				case ex_block:
					if (!e->e.block.result)
						return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_def:
				case ex_temp:
					{
						expr_t *n = new_unary_expr (op, e);
						type_t *t = get_type (e);
						if (t != &type_integer && t != &type_float)
							return error (e, "invalid type for unary ~");
						n->e.expr.type = t;
						return n;
					}
				case ex_integer:
					e->e.integer_val = ~e->e.integer_val;
					return e;
				case ex_float:
					e->e.float_val = ~(int)e->e.float_val;
					e->type = ex_integer;
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
		default:
			abort ();
	}
	error (e, "internal error");
	abort ();
}

int
has_function_call (expr_t *e)
{
	switch (e->type) {
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
function_expr (expr_t *e1, expr_t *e2)
{
	etype_t     t1;
	expr_t     *e;
	int         parm_count = 0;
	type_t     *ftype;
	int         i;
	expr_t     *args = 0, **a = &args;
	type_t     *arg_types[MAX_PARMS];
	expr_t     *arg_exprs[MAX_PARMS][2];
	int         arg_expr_count = 0;
	expr_t     *call;
	expr_t     *err = 0;

	t1 = extract_type (e1);

	if (t1 != ev_func) {
		if (e1->type == ex_def)
			return error (e1, "Called object \"%s\" is not a function",
						  e1->e.def->name);
		else
			return error (e1, "Called object is not a function");
	}

	if (e1->type == ex_def && e2 && e2->type == ex_string) {
		//FIXME eww, I hate this, but it's needed :(
		//FIXME make a qc hook? :)
		def_t *func = e1->e.def;
		def_t *e = PR_ReuseConstant (e2, 0);

		if (strncmp (func->name, "precache_sound", 14) == 0)
			PrecacheSound (e, func->name[4]);
		else if (strncmp (func->name, "precache_model", 14) == 0)
			PrecacheModel (e, func->name[14]);
		else if (strncmp (func->name, "precache_file", 13) == 0)
			PrecacheFile (e, func->name[13]);
	}

	ftype = e1->type == ex_def
			? e1->e.def->type
			: e1->e.expr.type;

	for (e = e2; e; e = e->next)
		parm_count++;
	if (parm_count > MAX_PARMS) {
		return error (e1, "more than %d parameters", MAX_PARMS);
	}
	if (ftype->num_parms != -1) {
		if (parm_count > ftype->num_parms) {
			return error (e1, "too many arguments");
		} else if (parm_count < ftype->num_parms) {
			return error (e1, "too few arguments");
		}
	}
	for (i = parm_count, e = e2; i > 0; i--, e = e->next) {
		type_t *t = get_type (e);

		if (ftype->parm_types[i - 1] == &type_float && e->type == ex_integer) {
			e->type = ex_float;
			e->e.float_val = e->e.integer_val;
			t = &type_float;
		}
		if (ftype->num_parms != -1) {
			if (t == &type_void) {
				t = ftype->parm_types[i - 1];
				e->type = expr_types[t->type];
			}
			if (t != ftype->parm_types[i - 1])
				err = error (e, "type mismatch for parameter %d of %s",
							 i, e1->e.def->name);
		} else {
			//if (e->type == ex_integer)
			//	warning (e, "passing integer consant into ... function");
		}
		arg_types[parm_count - i] = t;
	}
	if (err)
		return err;

	call = new_block_expr ();
	call->e.block.is_call = 1;
	for (e = e2, i = 0; e; e = e->next, i++) {
		if (has_function_call (e)) {
			*a = new_temp_def_expr (arg_types[i]);
			arg_exprs[arg_expr_count][0] = e;
			arg_exprs[arg_expr_count][1] = *a;
			arg_expr_count++;
		} else {
			*a = e;
		}
		a = &(*a)->next;
	}
	for (i = 0; i < arg_expr_count - 1; i++) {
		append_expr (call, binary_expr ('=', arg_exprs[i][1],
					                    arg_exprs[i][0]));
	}
	if (arg_expr_count) {
		e = new_binary_expr ('b', arg_exprs[arg_expr_count - 1][0],
							 arg_exprs[arg_expr_count - 1][1]);
		append_expr (call, e);
	}
	e = new_binary_expr ('c', e1, args);
	e->e.expr.type = ftype->aux_type;
	append_expr (call, e);
	if (ftype->aux_type != &type_void) {
		expr_t     *ret = new_expr ();
		ret->type = ex_def;
		ret->e.def = memcpy (malloc (sizeof (def_t)), &def_ret, sizeof (def_t));
		if (!ret->e.def)
			Sys_Error ("function_expr: Memory Allocation Failure\n");
		ret->e.def->type = ftype->aux_type;
		call->e.block.result = ret;
	}
	return call;
}

expr_t *
return_expr (function_t *f, expr_t *e)
{
	if (!e) {
		if (f->def->type->aux_type != &type_void)
			return error (e, "return from non-void function without a value");
	} else {
		type_t *t = get_type (e);

		if (f->def->type->aux_type == &type_void)
			return error (e, "returning a value for a void function");
		if (f->def->type->aux_type == &type_float && e->type == ex_integer) {
			e->type = ex_float;
			e->e.float_val = e->e.integer_val;
			t = &type_float;
		}
		if (t == &type_void) {
			t = f->def->type->aux_type;
			e->type = expr_types[t->type];
		}
		if (f->def->type->aux_type != t)
			return error (e, "type mismatch for return value of %s",
							f->def->name);
	}
	return new_unary_expr ('r', e);
}

expr_t *
conditional_expr (expr_t *cond, expr_t *e1, expr_t *e2)
{
	expr_t    *block = new_block_expr ();
	type_t    *type1 = get_type (e1);
	type_t    *type2 = get_type (e2);
	expr_t    *tlabel = new_label_expr ();
	expr_t    *elabel = new_label_expr ();

	block->e.block.result = (type1 == type2) ? new_temp_def_expr (type1) : 0;
	append_expr (block, new_binary_expr ('i', test_expr (cond, 1), tlabel));
	if (block->e.block.result)
		append_expr (block, new_binary_expr ('=', block->e.block.result, e2));
	else
		append_expr (block, e2);
	append_expr (block, new_unary_expr ('g', elabel));
	append_expr (block, tlabel);
	if (block->e.block.result)
		append_expr (block, new_binary_expr ('=', block->e.block.result, e1));
	else
		append_expr (block, e1);
	append_expr (block, elabel);
	return block;
}

expr_t *
incop_expr (int op, expr_t *e, int postop)
{
	expr_t     *one = new_expr ();
	expr_t     *incop;
	
	one->type = ex_integer;		// integer constants get auto-cast to float
	one->e.integer_val = 1;
	incop = asx_expr (op, e, one);
	if (postop) {
		expr_t     *temp;
		type_t     *type;
		expr_t     *block = new_block_expr ();

		type = e->type == ex_def
				? e->e.def->type
				: e->e.expr.type;
		temp = new_temp_def_expr (type);
		append_expr (block, binary_expr ('=', temp, e));
		append_expr (block, incop);
		block->e.block.result = temp;
		return block;
	}
	return incop;
}
