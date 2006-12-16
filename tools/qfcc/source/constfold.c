/*
	constfold.c

	expression constant folding

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/01/22

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

#include "expr.h"
#include "options.h"
#include "qc-parse.h"
#include "qfcc.h"
#include "type.h"

static __attribute__ ((noreturn)) void
internal_error (expr_t *e)
{
	error (e, "internal error");
	abort ();
}

static int
valid_op (int op, int *valid_ops)
{
	while (*valid_ops && op != *valid_ops)
		valid_ops++;
	return *valid_ops == op;
}

static expr_t *
do_op_string (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	const char *s1, *s2;
	static dstring_t *temp_str;
	static int  valid[] = {'=', 'b', '+', LT, GT, LE, GE, EQ, NE, 0};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for string");

	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_string;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

	s1 = e1->e.string_val ? e1->e.string_val : "";
	s2 = e2->e.string_val ? e2->e.string_val : "";

	switch (op) {
		case '+':
			if (!temp_str)
				temp_str = dstring_newstr ();
			dstring_clearstr (temp_str);
			dstring_appendstr (temp_str, s1);
			dstring_appendstr (temp_str, s2);
			e1->e.string_val = save_string (temp_str->str);
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
			internal_error (e1);
	}
	return e1;
}

static expr_t *
convert_to_float (expr_t *e)
{
	if (get_type (e) == &type_float)
		return e;

	switch (e->type) {
		case ex_integer:
			convert_int (e);
			return e;
		case ex_uinteger:
			convert_uint (e);
			return e;
		case ex_short:
			convert_short (e);
			return e;
		case ex_def:
		case ex_expr:
		case ex_uexpr:
		case ex_temp:
		case ex_block:
			return cast_expr (&type_float, e);
		default:
			internal_error (e);
	}
}

static expr_t *
do_op_float (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	float       f1, f2;
	expr_t     *conv;
	type_t     *type = &type_float;
	static int  valid[] = {
		'=', 'b', '+', '-', '*', '/', '&', '|', '^', '%',
		SHL, SHR, AND, OR, LT, GT, LE, GE, EQ, NE, 0
	};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for float");

	if (op == 'b') {
		// bind is backwards to assign (why did I do that? :P)
		if ((type = get_type (e2)) != &type_float) {
			//FIXME optimize casting a constant
			e->e.expr.e1 = e1 = cast_expr (type, e1);
		} else if ((conv = convert_to_float (e1)) != e1) {
			e->e.expr.e1 = e1 = conv;
		}
	} else if (op == '=' || op == PAS) {
		if ((type = get_type (e1)) != &type_float) {
			//FIXME optimize casting a constant
			e->e.expr.e2 = e2 = cast_expr (type, e2);
		} else if ((conv = convert_to_float (e2)) != e2) {
			e->e.expr.e2 = e2 = conv;
		}
	} else {
		if ((conv = convert_to_float (e1)) != e1) {
			e->e.expr.e1 = e1 = conv;
		}
		if ((conv = convert_to_float (e2)) != e2) {
			e->e.expr.e2 = e2 = conv;
		}
	}
	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			type = &type_integer;
		else
			type = &type_float;
	}
	e->e.expr.type = type;

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

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
			if (!f2)
				return error (e1, "divide by zero");
			e1->e.float_val /= f2;
			break;
		case '&':
			e1->e.float_val = (int) f1 & (int) f2;
			break;
		case '|':
			e1->e.float_val = (int) f1 | (int) f2;
			break;
		case '^':
			e1->e.float_val = (int) f1 ^ (int) f2;
			break;
		case '%':
			e1->e.float_val = (int) f1 % (int) f2;
			break;
		case SHL:
			e1->e.float_val = (int) f1 << (int) f2;
			break;
		case SHR:
			e1->e.float_val = (int) f1 >> (int) f2;
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
			internal_error (e1);
	}
	return e1;
}

static expr_t *
do_op_vector (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	float      *v1, *v2;
	static int  valid[] = {'=', 'b', '+', '-', '*', EQ, NE, 0};
	expr_t     *t;

	if (get_type (e1) != &type_vector) {

		if (op != '*')
			return error (e1, "invalid operand for vector");

		t = e1;
		e->e.expr.e1 = e1 = e2;
		e2 = t;
	}
	if (get_type (e2) != &type_vector) {
		e->e.expr.e2 = e2 = convert_to_float (e2);
		if (op != '*' && op != '/')
			return error (e1, "invalid operand for vector");
	} else {
		if (!valid_op (op, valid))
			return error (e1, "invalid operand for vector");
	}
	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else if (op == '*' && get_type (e2) == &type_vector) {
		e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_vector;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

	v1 = e1->e.vector_val;
	v2 = e2->e.vector_val;

	switch (op) {
		case '+':
			VectorAdd (v1, v2, v1);
			break;
		case '-':
			VectorSubtract (v1, v2, v1);
			break;
		case '/':
			if (!v2[0])
				return error (e1, "divide by zero");
			VectorScale (v1, 1 / v2[0], v1);
			break;
		case '*':
			if (get_type (e2) == &type_vector) {
				e1->type = ex_float;
				e1->e.float_val = DotProduct (v1, v2);
			} else {
				VectorScale (v1, v2[0], v1);
			}
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = VectorCompare (v1, v2);
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = !VectorCompare (v1, v2);
			break;
		default:
			internal_error (e1);
	}
	return e1;
}

static expr_t *
do_op_entity (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	type_t     *type = get_type (e2);

	if ((op == '.' || op == '&') && type->type == ev_field) {
		return e;
	}
	if (op == EQ || op == NE) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
		return e;
	}
	if ((op != '=' && op != 'b') || type != &type_entity)
		return error (e1, "invalid operand for entity");
	e->e.expr.type = &type_entity;
	return e;
}

static expr_t *
do_op_field (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	if (op != '=' && op != 'b')
		return error (e1, "invalid operand for field");
	e->e.expr.type = &type_field;
	return e;
}

static expr_t *
do_op_func (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	if (op == 'c') {
		e->e.expr.type = get_type (e1)->aux_type;
		return e;
	}
	if (op == EQ || op == NE) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
		return e;
	}
	if (op != '=' && op != 'b')
		return error (e1, "invalid operand for func");
	e->e.expr.type = &type_function;
	return e;
}

static expr_t *
do_op_pointer (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	type_t     *type;
	static int  valid[] = {'=', 'b', PAS, '&', 'M', '.', EQ, NE, 0};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for pointer");

	if (op == PAS && (type = get_type (e1)->aux_type) != get_type (e2)) {
		// make sure auto-convertions happen
		expr_t     *tmp = new_temp_def_expr (type);
		expr_t     *ass = new_binary_expr ('=', tmp, e2);

		tmp->file = e1->file;
		ass->line = e2->line;
		ass->file = e2->file;
		ass = fold_constants (ass);
		if (e->e.expr.e2 == tmp)
			internal_error (e2);
		e->e.expr.e2 = ass->e.expr.e2;
	}
	if (op == EQ || op == NE) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	}
	if (op != PAS && op != '.' && op != '&' && op != 'M'
		&& extract_type (e1) != extract_type (e2))
		return type_mismatch (e1, e2, op);
	if ((op == '.' || op == '&') && get_type (e2) == &type_uinteger) {
		//FIXME should implement unsigned addressing
		e->e.expr.e2 = cast_expr (&type_integer, e2);
	}
	return e;
}

static expr_t *
do_op_quaternion (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	float      *q1, *q2;
	static int  valid[] = {'=', 'b', '+', '-', '*', EQ, NE, 0};
	expr_t     *t;

	if (get_type (e1) != &type_quaternion) {

		if (op != '*')
			return error (e1, "invalid operand for quaternion");

		t = e1;
		e->e.expr.e1 = e1 = e2;
		e2 = t;
	}
	if (get_type (e2) != &type_quaternion) {
		e->e.expr.e2 = e2 = convert_to_float (e2);
		if (op != '*' && op != '/')
			return error (e1, "invalid operand for quaternion");
	} else {
		if (!valid_op (op, valid))
			return error (e1, "invalid operand for quaternion");
	}
	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_quaternion;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

	q1 = e1->e.quaternion_val;
	q2 = e2->e.quaternion_val;

	switch (op) {
		case '+':
			QuatAdd (q1, q2, q1);
			break;
		case '-':
			QuatSubtract (q1, q2, q1);
			break;
		case '/':
			if (!q2[0])
				return error (e1, "divide by zero");
			QuatScale (q1, 1 / q2[0], q1);
			q1[3] /= q2[0];
			break;
		case '*':
			if (get_type (e2) == &type_quaternion) {
				QuatMult (q1, q2, q1);
			} else {
				QuatScale (q1, q2[0], q1);
				q1[3] *= q2[0];
			}
			break;
		case EQ:
			e1->type = ex_integer;
			e1->e.integer_val = QuatCompare (q1, q2);
			break;
		case NE:
			e1->type = ex_integer;
			e1->e.integer_val = !QuatCompare (q1, q2);
			break;
		default:
			internal_error (e1);
	}
	return e1;
}

static expr_t *
do_op_integer (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	int         i1, i2;
	static int  valid[] = {
		'=', 'b', '+', '-', '*', '/', '&', '|', '^', '%',
		SHL, SHR, AND, OR, LT, GT, LE, GE, EQ, NE, 0
	};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for integer");

	if (e1->type == ex_short)
		convert_short_int (e1);

	if (e2->type == ex_short)
		convert_short_int (e2);

	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_integer;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

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
			if (options.warnings.integer_divide)
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
			internal_error (e1);
	}
	return e1;
}

static expr_t *
convert_to_uinteger (expr_t *e)
{
	if (get_type (e) == &type_uinteger)
		return e;

	switch (e->type) {
		case ex_integer:
			convert_int_uint (e);
			return e;
		case ex_short:
			convert_short_uint (e);
			return e;
		case ex_def:
		case ex_expr:
		case ex_uexpr:
		case ex_temp:
		case ex_block:
			return cast_expr (&type_uinteger, e);
		default:
			internal_error (e);
	}
}

static expr_t *
do_op_uinteger (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	expr_t     *conv;
	type_t     *type;
	unsigned    i1, i2;
	static int  valid[] = {
		'=', 'b', '+', '-', '*', '/', '&', '|', '^', '%',
		SHL, SHR, AND, OR, LT, GT, LE, GE, EQ, NE, 0
	};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for uinteger");

	if (e1->type == ex_short)
		convert_short_uint (e1);
	if (e1->type == ex_integer)
		convert_int_uint (e1);
	if (op == 'b') {
		// bind is backwards to assign (why did I do that? :P)
		if ((type = get_type (e2)) != &type_uinteger) {
			e->e.expr.e1 = e1 = cast_expr (type, e1);
		} else if ((conv = convert_to_uinteger (e1)) != e1) {
			e->e.expr.e1 = e1 = conv;
		}
	} else if (op == '=' || op == PAS) {
		if ((type = get_type (e1)) != &type_uinteger) {
			e->e.expr.e2 = e2 = cast_expr (type, e2);
		} else if ((conv = convert_to_uinteger (e2)) != e2) {
			e->e.expr.e2 = e2 = conv;
		}
	} else {
		if (get_type (e1) != &type_uinteger) {
			e->e.expr.e1 = e1 = cast_expr (&type_uinteger, e1);
		}
		if (e2->type == ex_short)
			convert_short_uint (e2);
		if (e2->type == ex_integer)
			convert_int_uint (e2);
		if (get_type (e2) != &type_uinteger) {
			e->e.expr.e2 = e2 = cast_expr (&type_uinteger, e2);
		}
		if ((conv = convert_to_uinteger (e2)) != e2) {
			e->e.expr.e2 = e2 = conv;
		}
	}

	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_uinteger;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

	i1 = e1->e.uinteger_val;
	i2 = e2->e.uinteger_val;

	switch (op) {
		case '+':
			e1->e.uinteger_val += i2;
			break;
		case '-':
			e1->e.uinteger_val -= i2;
			break;
		case '*':
			e1->e.uinteger_val *= i2;
			break;
		case '/':
			if (options.warnings.integer_divide)
				warning (e2, "%d / %d == %d", i1, i2, i1 / i2);
			e1->e.uinteger_val /= i2;
			break;
		case '&':
			e1->e.uinteger_val = i1 & i2;
			break;
		case '|':
			e1->e.uinteger_val = i1 | i2;
			break;
		case '^':
			e1->e.uinteger_val = i1 ^ i2;
			break;
		case '%':
			e1->e.uinteger_val = i1 % i2;
			break;
		case SHL:
			e1->e.uinteger_val = i1 << i2;
			break;
		case SHR:
			e1->e.uinteger_val = i1 >> i2;
			break;
		case AND:
			e1->e.uinteger_val = i1 && i2;
			break;
		case OR:
			e1->e.uinteger_val = i1 || i2;
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
			return error (e1, "invalid operand for uinteger");
	}
	return e1;
}

static expr_t *
do_op_short (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	short       i1, i2;
	static int  valid[] = {
		'=', 'b', '+', '-', '*', '/', '&', '|', '^', '%',
		SHL, SHR, AND, OR, LT, GT, LE, GE, EQ, NE, 0
	};

	if (!valid_op (op, valid))
		return error (e1, "invalid operand for short");

	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion > PROG_ID_VERSION)
			e->e.expr.type = &type_integer;
		else
			e->e.expr.type = &type_float;
	} else {
		e->e.expr.type = &type_short;
	}

	if (op == '=' || op == 'b' || !is_constant (e1) || !is_constant (e2))
		return e;

	i1 = e1->e.short_val;
	i2 = e2->e.short_val;

	switch (op) {
		case '+':
			e1->e.short_val += i2;
			break;
		case '-':
			e1->e.short_val -= i2;
			break;
		case '*':
			e1->e.short_val *= i2;
			break;
		case '/':
			if (options.warnings.integer_divide)
				warning (e2, "%d / %d == %d", i1, i2, i1 / i2);
			e1->e.short_val /= i2;
			break;
		case '&':
			e1->e.short_val = i1 & i2;
			break;
		case '|':
			e1->e.short_val = i1 | i2;
			break;
		case '^':
			e1->e.short_val = i1 ^ i2;
			break;
		case '%':
			e1->e.short_val = i1 % i2;
			break;
		case SHL:
			e1->e.short_val = i1 << i2;
			break;
		case SHR:
			e1->e.short_val = i1 >> i2;
			break;
		case AND:
			e1->e.short_val = i1 && i2;
			break;
		case OR:
			e1->e.short_val = i1 || i2;
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
			internal_error (e1);
	}
	return e1;
}

static expr_t *
do_op_struct (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	type_t     *type;

	if (op != '=' && op != 'b' && op != 'M')
		return error (e1, "invalid operand for struct");
	if ((type = get_type (e1)) != get_type (e2))
		return type_mismatch (e1, e2, op);
	e->e.expr.type = type;
	return e;
}

static expr_t *
do_op_array (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	return e;
}

static expr_t *
do_op_invalid (int op, expr_t *e, expr_t *e1, expr_t *e2)
{
	print_expr (e),puts("");
	return error (e1, "invalid operands for binary %s: %s %s",
				  get_op_string (op), pr_type_name[extract_type (e1)],
				  pr_type_name[extract_type (e2)]);
}

typedef expr_t *(*operation_t) (int op, expr_t *e, expr_t *e1, expr_t *e2);

static operation_t op_void[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_string[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_string,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_float[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_float,						// ev_float
	do_op_vector,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_quaternion,					// ev_quaternion
	do_op_float,						// ev_integer
	do_op_float,						// ev_uinteger
	do_op_float,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_vector[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_vector,						// ev_float
	do_op_vector,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_vector,						// ev_integer
	do_op_vector,						// ev_uinteger
	do_op_vector,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_entity[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_entity,						// ev_entity
	do_op_entity,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_field[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_field,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_func[ev_type_count] = {
	do_op_func,							// ev_void
	do_op_func,							// ev_string
	do_op_func,							// ev_float
	do_op_func,							// ev_vector
	do_op_func,							// ev_entity
	do_op_func,							// ev_field
	do_op_func,							// ev_func
	do_op_func,							// ev_pointer
	do_op_func,							// ev_quaternion
	do_op_func,							// ev_integer
	do_op_func,							// ev_uinteger
	do_op_func,							// ev_short
	do_op_func,							// ev_struct
	do_op_func,							// ev_object
	do_op_func,							// ev_class
	do_op_func,							// ev_sel
	do_op_func,							// ev_array
};

static operation_t op_pointer[ev_type_count] = {
	do_op_pointer,						// ev_void
	do_op_pointer,						// ev_string
	do_op_pointer,						// ev_float
	do_op_pointer,						// ev_vector
	do_op_pointer,						// ev_entity
	do_op_pointer,						// ev_field
	do_op_pointer,						// ev_func
	do_op_pointer,						// ev_pointer
	do_op_pointer,						// ev_quaternion
	do_op_pointer,						// ev_integer
	do_op_pointer,						// ev_uinteger
	do_op_pointer,						// ev_short
	do_op_pointer,						// ev_struct
	do_op_pointer,						// ev_object
	do_op_pointer,						// ev_class
	do_op_pointer,						// ev_sel
	do_op_pointer,						// ev_array
};

static operation_t op_quaternion[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_quaternion,					// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_quaternion,					// ev_quaternion
	do_op_quaternion,					// ev_integer
	do_op_quaternion,					// ev_uinteger
	do_op_quaternion,					// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_integer[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_float,						// ev_float
	do_op_vector,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_quaternion,					// ev_quaternion
	do_op_integer,						// ev_integer
	do_op_uinteger,						// ev_uinteger
	do_op_integer,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_uinteger[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_float,						// ev_float
	do_op_vector,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_quaternion,					// ev_quaternion
	do_op_uinteger,						// ev_integer
	do_op_uinteger,						// ev_uinteger
	do_op_uinteger,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_short[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_float,						// ev_float
	do_op_vector,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_quaternion,					// ev_quaternion
	do_op_integer,						// ev_integer
	do_op_uinteger,						// ev_uinteger
	do_op_short,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_struct[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_pointer,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_struct,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_object[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_class[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_sel[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_invalid,						// ev_integer
	do_op_invalid,						// ev_uinteger
	do_op_invalid,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t op_array[ev_type_count] = {
	do_op_invalid,						// ev_void
	do_op_invalid,						// ev_string
	do_op_invalid,						// ev_float
	do_op_invalid,						// ev_vector
	do_op_invalid,						// ev_entity
	do_op_invalid,						// ev_field
	do_op_invalid,						// ev_func
	do_op_invalid,						// ev_pointer
	do_op_invalid,						// ev_quaternion
	do_op_array,						// ev_integer
	do_op_array,						// ev_uinteger
	do_op_array,						// ev_short
	do_op_invalid,						// ev_struct
	do_op_invalid,						// ev_object
	do_op_invalid,						// ev_class
	do_op_invalid,						// ev_sel
	do_op_invalid,						// ev_array
};

static operation_t *do_op[ev_type_count] = {
	op_void,							// ev_void
	op_string,							// ev_string
	op_float,							// ev_float
	op_vector,							// ev_vector
	op_entity,							// ev_entity
	op_field,							// ev_field
	op_func,							// ev_func
	op_pointer,							// ev_pointer
	op_quaternion,						// ev_quaternion
	op_integer,							// ev_integer
	op_uinteger,						// ev_uinteger
	op_short,							// ev_short
	op_struct,							// ev_struct
	op_object,							// ev_object
	op_class,							// ev_class
	op_sel,								// ev_sel
	op_array,							// ev_array
};

expr_t *
fold_constants (expr_t *e)
{
	int         op;
	expr_t     *e1, *e2;
	etype_t     t1, t2;

	if (e->type == ex_block) {
		expr_t     *block = new_block_expr ();
		expr_t     *next;

		block->e.block.result = e->e.block.result;
		block->line = e->line;
		block->file = e->file;

		for (e = e->e.block.head; e; e = next) {
			next = e->next;
			e = fold_constants (e);
			e->next = 0;
			append_expr (block, e);
		}

		return block;
	}
	if (e->type == ex_bool) {
		e->e.bool.e = fold_constants (e->e.bool.e);
		return e;
	}
	if (e->type == ex_uexpr) {
		if (e->e.expr.e1)
			e->e.expr.e1 = fold_constants (e->e.expr.e1);
		return e;
	}

	if (e->type != ex_expr)
		return e;

	op = e->e.expr.op;

	e->e.expr.e1 = e1 = fold_constants (e->e.expr.e1);
	if (e1->type == ex_error)
		return e1;
	t1 = extract_type (e1);

	if (op == 'i' || op == 'n' || op == 'c')
		return e;

	e->e.expr.e2 = e2 = fold_constants (e->e.expr.e2);
	if (e2->type == ex_error)
		return e2;

	if (e2->type == ex_label)
		return e;

	t2 = extract_type (e2);

	if (op == 's')
		return e;

	if (!do_op[t1][t2])
		internal_error (e);
	return do_op[t1][t2] (op, e, e1, e2);
}
