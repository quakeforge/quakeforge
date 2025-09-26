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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/dstring.h>
#include <QF/mathlib.h>

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

typedef const expr_t *(*operation_t) (int op, const expr_t *e,
									  const expr_t *e1, const expr_t *e2);
typedef const expr_t *(*unaryop_t) (int op, const expr_t *e,
									const expr_t *e1);

static const expr_t *
cmp_result_expr (int result)
{
	if (is_float (type_default)) {
		return new_float_expr (result, false);
	} else {
		return new_int_expr (result, false);
	}
}

static const expr_t *
do_op_string (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	const char *s1, *s2;
	static dstring_t *temp_str;

	if (!is_constant (e1) || !is_constant (e2))
		return e;

	s1 = expr_string (e1);
	s2 = expr_string (e2);
	if (!s1)
		s1 = "";
	if (!s2)
		s2 = "";

	const expr_t *new = 0;
	switch (op) {
		case '+':
			if (!temp_str)
				temp_str = dstring_newstr ();
			dstring_clearstr (temp_str);
			dstring_appendstr (temp_str, s1);
			dstring_appendstr (temp_str, s2);
			new = new_string_expr (save_string (temp_str->str));
			break;
		case QC_LT:
			new = cmp_result_expr (strcmp (s1, s2) < 0);
			break;
		case QC_GT:
			new = cmp_result_expr (strcmp (s1, s2) > 0);
			break;
		case QC_LE:
			new = cmp_result_expr (strcmp (s1, s2) <= 0);
			break;
		case QC_GE:
			new = cmp_result_expr (strcmp (s1, s2) >= 0);
			break;
		case QC_EQ:
			new = cmp_result_expr (strcmp (s1, s2) == 0);
			break;
		case QC_NE:
			new = cmp_result_expr (strcmp (s1, s2));
			break;
		default:
			internal_error (e1, 0);
	}
	return new;
}

static const expr_t *
convert_to_float (const expr_t *e)
{
	if (is_float(get_type (e)))
		return e;

	scoped_src_loc (e);
	return cast_expr (&type_float, e);
}

static float
get_rep_float (const expr_t *e)
{
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_float (e);
	}
	pr_type_t components[type_size (type)];
	value_store (components, type, e);
	float val = components[0].float_value;
	for (int i = 0; i < type_count (type); i++) {
		if (components[i].float_value != val) {
			return -1;
		}
	}
	return val;
}

static pr_double_t
get_rep_double (const expr_t *e)
{
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_double (e);
	}
	pr_type_t c[type_size (type)];
	value_store (c, type, e);
	pr_double_t *components = (pr_double_t *) &c[0];
	pr_double_t val = components[0];
	for (int i = 0; i < type_count (type); i++) {
		if (components[i] != val) {
			return -1;
		}
	}
	return val;
}

static pr_int_t
get_rep_int (const expr_t *e)
{
	if (!is_integral_val (e)) {
		return -1;
	}
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_integral (e);
	}
	pr_type_t components[type_size (type)];
	value_store (components, type, e);
	pr_int_t val = components[0].value;
	for (int i = 0; i < type_count (type); i++) {
		if (components[i].value != val) {
			return -1;
		}
	}
	return val;
}

static pr_uint_t
get_rep_uint (const expr_t *e)
{
	if (!is_integral_val (e)) {
		return -1;
	}
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_integral (e);
	}
	pr_type_t components[type_size (type)];
	value_store (components, type, e);
	pr_uint_t val = components[0].uint_value;
	for (int i = 0; i < type_count (type); i++) {
		if (components[i].uint_value != val) {
			return -1;
		}
	}
	return val;
}

static pr_long_t
get_rep_long (const expr_t *e)
{
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_long (e);
	}
	pr_type_t c[type_size (type)];
	value_store (c, type, e);
	pr_long_t *components = (pr_long_t *) &c[0];
	pr_long_t val = components[0];
	for (int i = 0; i < type_count (type); i++) {
		if (components[i] != val) {
			return -1;
		}
	}
	return val;
}

static pr_ulong_t
get_rep_ulong (const expr_t *e)
{
	auto type = get_type (e);
	if (is_scalar (type)) {
		return expr_ulong (e);
	}
	pr_type_t c[type_size (type)];
	value_store (c, type, e);
	pr_ulong_t *components = (pr_ulong_t *) &c[0];
	pr_ulong_t val = components[0];
	for (int i = 0; i < type_count (type); i++) {
		if (components[i] != val) {
			return -1;
		}
	}
	return val;
}

static const expr_t *
do_op_float (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	float       f1 = -1, f2 = -1;

	if (is_constant (e1)) {
		f1 = get_rep_float (e1);
	}
	if (is_constant (e2)) {
		f2 = get_rep_float (e2);
	}

	if (op == QC_SCALE && is_constant (e2) && f2 == 1)
		return e1;
	if (op == QC_SCALE && is_constant (e2) && f2 == 0)
		return new_zero_expr (get_type (e1));
	if (op == '*' && is_constant (e1) && f1 == 1)
		return e2;
	if (op == '*' && is_constant (e2) && f2 == 1)
		return e1;
	if (op == '*' && is_constant (e1) && f1 == 0)
		return e1;
	if (op == '*' && is_constant (e2) && f2 == 0)
		return e2;
	if (op == '/' && is_constant (e2) && f2 == 1)
		return e1;
	if (op == '/' && is_constant (e2) && f2 == 0) {
		warning (e, "division by zero");
		return e;
	}
	if (op == '/' && is_constant (e1) && f1 == 0)
		return e1;
	if (op == '+' && is_constant (e1) && f1 == 0)
		return e2;
	if (op == '+' && is_constant (e2) && f2 == 0)
		return e1;
	if (op == '-' && is_constant (e2) && f2 == 0)
		return e1;

	return e;
}

static const expr_t *
do_op_double (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	double      d1 = -1, d2 = -1;

	if (is_constant (e1)) {
		d1 = get_rep_double (e1);
	}
	if (is_constant (e2)) {
		d2 = get_rep_double (e2);
	}

	if (!is_scalar (get_type (e1)) || !is_scalar (get_type (e2))) {
		return e;
	}

	if (op == '*' && d1 == 1)
		return e2;
	if (op == '*' && d2 == 1)
		return e1;
	if (op == '*' && d1 == 0)
		return e1;
	if (op == '*' && d2 == 0)
		return e2;
	if (op == '/' && d2 == 1)
		return e1;
	if (op == '/' && d2 == 0) {
		warning (e, "division by zero");
		return e;
	}
	if (op == '/' && d1 == 0)
		return e1;
	if (op == '+' && d1 == 0)
		return e2;
	if (op == '+' && d2 == 0)
		return e1;
	if (op == '-' && d2 == 0)
		return e1;

	if (!is_constant (e1) || !is_constant (e2))
		return e;

	d1 = expr_double (e1);
	d2 = expr_double (e2);
	bool implicit = e1->implicit && e2->implicit;

	const expr_t *new = 0;
	switch (op) {
		case '+':
			new = new_double_expr (d1 + d2, implicit);
			break;
		case '-':
			new = new_double_expr (d1 - d2, implicit);
			break;
		case '*':
			new = new_double_expr (d1 * d2, implicit);
			break;
		case '/':
			if (!d2)
				return error (e1, "divide by zero");
			new = new_double_expr (d1 / d2, implicit);
			break;
		case '%':
			new = new_double_expr ((int)d1 % (int)d2, implicit);
			break;
		case QC_LT:
			new = cmp_result_expr (d1 < d2);
			break;
		case QC_GT:
			new = cmp_result_expr (d1 > d2);
			break;
		case QC_LE:
			new = cmp_result_expr (d1 <= d2);
			break;
		case QC_GE:
			new = cmp_result_expr (d1 >= d2);
			break;
		case QC_EQ:
			new = cmp_result_expr (d1 == d2);
			break;
		case QC_NE:
			new = cmp_result_expr (d1 != d2);
			break;
		default:
			internal_error (e1, 0);
	}
	return new;
}

static const expr_t *
do_op_vector (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	if (!is_vector(get_type (e1))) {
		scoped_src_loc (e);
		auto t = new_binary_expr (op, e2, e1);
		t->expr.type = e->expr.type;
		return do_op_vector (op, e, e2, e1);
	}

	if (!is_vector(get_type (e2))) {
		if (!is_float (get_type (e2))) {
			e2 = convert_to_float (e2);
			auto ne = new_binary_expr (op, e1, e2);
			ne->expr.type = e->expr.type;
			e = edag_add_expr (ne);
		}
	} else {
	}
	if (is_compare (op) || is_logic (op)) {
		//if (options.code.progsversion > PROG_ID_VERSION)
		//	e->expr.type = &type_int;
		//else
		//	e->expr.type = &type_float;
	} else if (op == QC_DOT && is_vector(get_type (e2))) {
		//e->expr.type = &type_float;
	} else if (op == '/' && !is_constant (e1)) {
		e2 = fold_constants (binary_expr ('/', new_float_expr (1, false), e2));
		e = fold_constants (binary_expr ('*', e1, e2));
	} else {
		//e->expr.type = &type_vector;
	}

	if (op == QC_SCALE && is_float_val (e2) && expr_float (e2) == 1)
		return e1;
	if (op == QC_SCALE && is_float_val (e2) && expr_float (e2) == 0)
		return new_vector_expr (vec3_origin);
	if (op == '/' && is_float_val (e2) && expr_float (e2) == 1)
		return e1;
	if (op == '/' && is_float_val (e2) && expr_float (e2) == 0) {
		warning (e, "division by zero");
		return e;
	}
	if (op == '+' && is_constant (e1) && VectorIsZero (expr_vector (e1)))
		return e2;
	if (op == '+' && is_constant (e2) && VectorIsZero (expr_vector (e2)))
		return e1;
	if (op == '-' && is_constant (e1) && VectorIsZero (expr_vector (e1))
		&& is_constant (e2)) {
		vec3_t      v;
		VectorNegate (expr_vector (e2), v);
		auto new = new_vector_expr (v);
		return new;
	}
	if (op == '-' && is_constant (e2) && VectorIsZero (expr_vector (e2)))
		return e1;

	return e;
}

static const expr_t *
do_op_entity (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	auto type = get_type (e2);

	if (op == '.' && type->type == ev_field) {
		return e;
	}

	return e;
}

static const expr_t *
do_op_field (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	return error (e1, "invalid operator for field");
}

static const expr_t *
do_op_func (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	return e;
}

static const expr_t *
do_op_pointer (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	return e;
}

static const expr_t *
do_op_quaternion (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	if (op == '*' && is_float_val (e2) && expr_float (e2) == 1)
		return e1;
	if (op == '*' && is_float_val (e2) && expr_float (e2) == 0)
		return new_quaternion_expr (quat_origin);
	if (op == '/' && is_float_val (e2) && expr_float (e2) == 1)
		return e1;
	if (op == '/' && is_float_val (e2) && expr_float (e2) == 0) {
		warning (e, "division by zero");
		return e;
	}
	if (op == '+' && is_constant (e1) && QuatIsZero (expr_quaternion (e1)))
		return e2;
	if (op == '+' && is_constant (e2) && QuatIsZero (expr_quaternion (e2)))
		return e1;
	if (op == '-' && is_constant (e2) && QuatIsZero (expr_quaternion (e2)))
		return e1;

	return e;
}

static const expr_t *
do_op_int (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	int         val1 = -1, val2 = -1;

	if (is_constant (e1)) {
		val1 = get_rep_int (e1);
	}
	if (is_constant (e2)) {
		val2 = get_rep_int (e2);
	}

	if (op == '*' && val1 == 1)
		return e2;
	if (op == '*' && val2 == 1)
		return e1;
	if (op == '*' && val1 == 0)
		return e1;
	if (op == '*' && val2 == 0)
		return e2;
	if (op == '/' && val2 == 1)
		return e1;
	if (op == '/' && val2 == 0)
		return error (e, "division by zero");
	if (op == '/' && val1 == 0)
		return e1;
	if (op == '+' && val1 == 0)
		return e2;
	if (op == '+' && val2 == 0)
		return e1;
	if (op == '-' && val2 == 0)
		return e1;
	return e;
}

static const expr_t *
do_op_uint (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	unsigned    val1 = -1, val2 = -1;

	if (is_constant (e1)) {
		val1 = get_rep_uint (e1);
	}
	if (is_constant (e2)) {
		val2 = get_rep_uint (e2);
	}

	if (op == '*' && val1 == 1)
		return e2;
	if (op == '*' && val2 == 1)
		return e1;
	if (op == '*' && val1 == 0)
		return e1;
	if (op == '*' && val2 == 0)
		return e2;
	if (op == '/' && val2 == 1)
		return e1;
	if (op == '/' && val2 == 0)
		return error (e, "division by zero");
	if (op == '/' && val1 == 0)
		return e1;
	if (op == '+' && val1 == 0)
		return e2;
	if (op == '+' && val2 == 0)
		return e1;
	if (op == '-' && val2 == 0)
		return e1;

	return e;
}

static const expr_t *
do_op_long (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	pr_long_t   val1 = -1, val2 = -1;

	if (is_constant (e1)) {
		val1 = get_rep_long (e1);
	}
	if (is_constant (e2)) {
		val2 = get_rep_long (e2);
	}

	if (op == '*' && val1 == 1)
		return e2;
	if (op == '*' && val2 == 1)
		return e1;
	if (op == '*' && val1 == 0)
		return e1;
	if (op == '*' && val2 == 0)
		return e2;
	if (op == '/' && val2 == 1)
		return e1;
	if (op == '/' && val2 == 0)
		return error (e, "division by zero");
	if (op == '/' && val1 == 0)
		return e1;
	if (op == '+' && val1 == 0)
		return e2;
	if (op == '+' && val2 == 0)
		return e1;
	if (op == '-' && val2 == 0)
		return e1;
	return e;
}

static const expr_t *
do_op_ulong (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	pr_ulong_t  val1 = -1, val2 = -1;

	if (is_constant (e1)) {
		val1 = get_rep_ulong (e1);
	}
	if (is_constant (e2)) {
		val2 = get_rep_ulong (e2);
	}

	if (op == '*' && val1 == 1)
		return e2;
	if (op == '*' && val2 == 1)
		return e1;
	if (op == '*' && val1 == 0)
		return e1;
	if (op == '*' && val2 == 0)
		return e2;
	if (op == '/' && val2 == 1)
		return e1;
	if (op == '/' && val2 == 0)
		return error (e, "division by zero");
	if (op == '/' && val1 == 0)
		return e1;
	if (op == '+' && val1 == 0)
		return e2;
	if (op == '+' && val2 == 0)
		return e1;
	if (op == '-' && val2 == 0)
		return e1;

	return e;
}

static const expr_t *
do_op_short (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	return e;
}

static operation_t *do_op[ev_type_count];
static const expr_t *
do_op_invalid (int op, const expr_t *e, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);

	dstring_t  *enc1 = dstring_newstr ();
	dstring_t  *enc2 = dstring_newstr ();

	print_type_str (enc1, t1);
	print_type_str (enc2, t2);

	//print_expr (e);
	e1 = error (e1, "invalid operands for binary %s: %s %s",
				get_op_string (op), enc1->str, enc2->str);
	dstring_delete (enc1);
	dstring_delete (enc2);
	return e1;
}

static operation_t op_void[ev_type_count] = {
};

static operation_t op_string[ev_type_count] = {
    [ev_string]     = do_op_string,
};

static operation_t op_float[ev_type_count] = {
    [ev_float]      = do_op_float,
};

static operation_t op_vector[ev_type_count] = {
    [ev_float]      = do_op_vector,
    [ev_vector]     = do_op_vector,
};

static operation_t op_entity[ev_type_count] = {
    [ev_entity]     = do_op_entity,
    [ev_field]      = do_op_entity,
};

static operation_t op_field[ev_type_count] = {
    [ev_field]      = do_op_field,
};

static operation_t op_func[ev_type_count] = {
    [ev_func]       = do_op_func,
};

static operation_t op_pointer[ev_type_count] = {
    [ev_ptr]        = do_op_pointer,
    [ev_int]        = do_op_pointer,
    [ev_uint]       = do_op_pointer,
};

static operation_t op_quaternion[ev_type_count] = {
    [ev_quaternion] = do_op_quaternion,
};

static operation_t op_int[ev_type_count] = {
    [ev_int]        = do_op_int,
    [ev_uint]       = do_op_int,
};

static operation_t op_uint[ev_type_count] = {
    [ev_int]        = do_op_uint,
    [ev_uint]       = do_op_uint,
};

static operation_t op_short[ev_type_count] = {
    [ev_short]      = do_op_short,
};

static operation_t op_double[ev_type_count] = {
    [ev_double]     = do_op_double,
};

static operation_t op_long[ev_type_count] = {
    [ev_long]       = do_op_long,
    [ev_ulong]      = do_op_long,
};

static operation_t op_ulong[ev_type_count] = {
    [ev_long]       = do_op_ulong,
    [ev_ulong]      = do_op_ulong,
};

static operation_t op_compound[ev_type_count] = {
};

static operation_t *do_op[ev_type_count] = {
    [ev_void]       = op_void,
    [ev_string]     = op_string,
    [ev_float]      = op_float,
    [ev_vector]     = op_vector,
    [ev_entity]     = op_entity,
    [ev_field]      = op_field,
    [ev_func]       = op_func,
    [ev_ptr]        = op_pointer,
    [ev_quaternion] = op_quaternion,
    [ev_int]        = op_int,
    [ev_uint]       = op_uint,
    [ev_short]      = op_short,
    [ev_double]     = op_double,
    [ev_long]       = op_long,
    [ev_ulong]      = op_ulong,
    [ev_invalid]    = op_compound,
};

static unaryop_t do_unary_op[ev_type_count];
static const expr_t *
uop_invalid (int op, const expr_t *e, const expr_t *e1)
{
	auto t1 = get_type (e1);
	if (is_scalar (t1)) {
		// The expression is an enum. Treat the enum as the default type.
		etype_t     t;
		t = type_default->type;
		return do_unary_op[t] (op, e, e1);
	} else {
		dstring_t  *enc1 = dstring_newstr ();

		print_type_str (enc1, t1);

		//print_expr (e);
		e1 = error (e1, "invalid operand for unary %s: %s",
					get_op_string (op), enc1->str);
		dstring_delete (enc1);
		return e1;
	}
}

static const expr_t *
uop_string (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_float (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_vector (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_entity (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_field (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_func (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_pointer (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_quaternion (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_int (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_uint (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_short (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_double (int op, const expr_t *e, const expr_t *e1)
{
	return e;
}

static const expr_t *
uop_compound (int op, const expr_t *e, const expr_t *e1)
{
	auto t1 = get_type (e1);

	if (is_scalar (t1)) {
		if (is_enum (t1)) {
			return uop_int (op, e, e1);
		}
	}
	if (is_handle (t1) && op == '!') {
		return e;
	}
	return error (e1, "invalid operand for unary %s", get_op_string (op));
}

static unaryop_t do_unary_op[ev_type_count] = {
	uop_invalid,						// ev_void
	uop_string,							// ev_string
	uop_float,							// ev_float
	uop_vector,							// ev_vector
	uop_entity,							// ev_entity
	uop_field,							// ev_field
	uop_func,							// ev_func
	uop_pointer,						// ev_ptr
	uop_quaternion,						// ev_quaternion
	uop_int,							// ev_int
	uop_uint,							// ev_uint
	uop_short,							// ev_short
	uop_double,							// ev_double
	uop_compound,						// ev_invalid
};

const expr_t *
fold_constants (const expr_t *e)
{
	int         op;
	const expr_t *e1, *e2;
	etype_t     t1, t2;

	if (e->type == ex_uexpr) {
		e1 = e->expr.e1;
		if (!e1) {
			return e;
		}
		if (options.code.progsversion == PROG_VERSION
			&& is_math (get_type (e1))) {
			return evaluate_constexpr (e);
		}
		op = e->expr.op;
		t1 = extract_type (e1);
		if (t1 >= ev_type_count || !do_unary_op[t1]) {
			print_expr (e);
			internal_error (e, "invalid type: %d", t1);
		}
		return do_unary_op[t1] (op, e, e1);
	} else if (e->type == ex_expr) {
		e1 = e->expr.e1;
		e2 = e->expr.e2;
		if (!is_constant (e1) && !is_constant (e2)) {
			return e;
		}

		if (options.code.progsversion == PROG_VERSION
			&& is_math_val (e1) && is_math_val (e2)) {
			return evaluate_constexpr (e);
		}

		op = e->expr.op;

		t1 = extract_type (e1);
		t2 = extract_type (e2);

		if (t1 >= ev_type_count || t2 >= ev_type_count
			|| !do_op[t1] || !do_op[t1][t2]) {
			return do_op_invalid (op, e, e1, e2);
		}
		return do_op[t1][t2] (op, e, e1, e2);
	} else if (e->type == ex_alias
			   || e->type == ex_field
			   || e->type == ex_swizzle) {
		if (options.code.progsversion == PROG_VERSION) {
			return evaluate_constexpr (e);
		}
	}
	return e;
}
