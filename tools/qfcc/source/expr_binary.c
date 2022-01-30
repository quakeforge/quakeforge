/*
	exprtype.c

	Expression type manipulation

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/06/27

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

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/type.h"

#include "tools/qfcc/source/qc-parse.h"

typedef struct {
	int         op;
	type_t     *result_type;
	type_t     *a_cast;
	type_t     *b_cast;
	expr_t     *(*process)(int op, expr_t *e1, expr_t *e2);
} expr_type_t;

static expr_t *pointer_arithmetic (int op, expr_t *e1, expr_t *e2);
static expr_t *pointer_compare (int op, expr_t *e1, expr_t *e2);
static expr_t *func_compare (int op, expr_t *e1, expr_t *e2);
static expr_t *inverse_multiply (int op, expr_t *e1, expr_t *e2);
static expr_t *double_compare (int op, expr_t *e1, expr_t *e2);
static expr_t *vector_compare (int op, expr_t *e1, expr_t *e2);

static expr_type_t string_string[] = {
	{'+',	&type_string},
	{EQ,	&type_int},
	{NE,	&type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};

static expr_type_t float_float[] = {
	{'+',	&type_float},
	{'-',	&type_float},
	{'*',	&type_float},
	{'/',	&type_float},
	{'&',	&type_float},
	{'|',	&type_float},
	{'^',	&type_float},
	{'%',	&type_float},
	{MOD,	&type_float},
	{SHL,	&type_float},
	{SHR,	&type_float},
	{AND,	&type_int},
	{OR,	&type_int},
	{EQ,	&type_int},
	{NE,	&type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};

static expr_type_t float_vector[] = {
	{'*',	&type_vector},
	{0, 0}
};

static expr_type_t float_quat[] = {
	{'*',	&type_quaternion},
	{0, 0}
};

static expr_type_t float_int[] = {
	{'+',	&type_float, 0, &type_float},
	{'-',	&type_float, 0, &type_float},
	{'*',	&type_float, 0, &type_float},
	{'/',	&type_float, 0, &type_float},
	{'&',	&type_float, 0, &type_float},
	{'|',	&type_float, 0, &type_float},
	{'^',	&type_float, 0, &type_float},
	{'%',	&type_float, 0, &type_float},
	{MOD,	&type_float, 0, &type_float},
	{SHL,	&type_float, 0, &type_float},
	{SHR,	&type_float, 0, &type_float},
	{EQ,	&type_int, 0, &type_float},
	{NE,	&type_int, 0, &type_float},
	{LE,	&type_int, 0, &type_float},
	{GE,	&type_int, 0, &type_float},
	{LT,	&type_int, 0, &type_float},
	{GT,	&type_int, 0, &type_float},
	{0, 0}
};
#define float_uint float_int
#define float_short float_int

static expr_type_t float_double[] = {
	{'+',	&type_double, &type_double, 0},
	{'-',	&type_double, &type_double, 0},
	{'*',	&type_double, &type_double, 0},
	{'/',	&type_double, &type_double, 0},
	{'%',	&type_double, &type_double, 0},
	{MOD,	&type_double, &type_double, 0},
	{EQ,	0, 0, 0, double_compare},
	{NE,	0, 0, 0, double_compare},
	{LE,	0, 0, 0, double_compare},
	{GE,	0, 0, 0, double_compare},
	{LT,	0, 0, 0, double_compare},
	{GT,	0, 0, 0, double_compare},
	{0, 0}
};

static expr_type_t vector_float[] = {
	{'*',	&type_vector},
	{'/',	0, 0, 0, inverse_multiply},
	{0, 0}
};

static expr_type_t vector_vector[] = {
	{'+',	&type_vector},
	{'-',	&type_vector},
	{'*',	&type_float},
	{EQ,	0, 0, 0, vector_compare},
	{NE,	0, 0, 0, vector_compare},
	{0, 0}
};

#define vector_int vector_float
#define vector_uint vector_float
#define vector_short vector_float

static expr_type_t vector_double[] = {
	{'*',	&type_vector},
	{'/',	0, 0, 0, inverse_multiply},
	{0, 0}
};

static expr_type_t entity_entity[] = {
	{EQ,	&type_int},
	{NE,	&type_int},
	{0, 0}
};

static expr_type_t field_field[] = {
	{EQ,	&type_int},
	{NE,	&type_int},
	{0, 0}
};

static expr_type_t func_func[] = {
	{EQ,	0, 0, 0, func_compare},
	{NE,	0, 0, 0, func_compare},
	{0, 0}
};

static expr_type_t pointer_pointer[] = {
	{'-',	0, 0, 0, pointer_arithmetic},
	{EQ,	0, 0, 0, pointer_compare},
	{NE,	0, 0, 0, pointer_compare},
	{LE,	0, 0, 0, pointer_compare},
	{GE,	0, 0, 0, pointer_compare},
	{LT,	0, 0, 0, pointer_compare},
	{GT,	0, 0, 0, pointer_compare},
	{0, 0}
};

static expr_type_t pointer_int[] = {
	{'+',	0, 0, 0, pointer_arithmetic},
	{'-',	0, 0, 0, pointer_arithmetic},
	{0, 0}
};
#define pointer_uint pointer_int
#define pointer_short pointer_int

static expr_type_t quat_float[] = {
	{'*',	&type_quaternion},
	{'/',	0, 0, 0, inverse_multiply},
	{0, 0}
};

static expr_type_t quat_vector[] = {
	{'*',	&type_vector},
	{0, 0}
};

static expr_type_t quat_quat[] = {
	{'+',	&type_quaternion},
	{'-',	&type_quaternion},
	{'*',	&type_quaternion},
	{EQ,	&type_int},
	{NE,	&type_int},
	{0, 0}
};

static expr_type_t quat_int[] = {
	{'*',	&type_quaternion, 0, &type_float},
	{'/',	0, 0, 0, inverse_multiply},
	{0, 0}
};
#define quat_uint quat_int
#define quat_short quat_int

static expr_type_t quat_double[] = {
	{'*',	&type_quaternion},
	{'/',	0, 0, 0, inverse_multiply},
	{0, 0}
};

static expr_type_t int_float[] = {
	{'+',	&type_float, &type_float, 0},
	{'-',	&type_float, &type_float, 0},
	{'*',	&type_float, &type_float, 0},
	{'/',	&type_float, &type_float, 0},
	{'&',	&type_float, &type_float, 0},
	{'|',	&type_float, &type_float, 0},
	{'^',	&type_float, &type_float, 0},
	{'%',	&type_float, &type_float, 0},
	{MOD,	&type_float, &type_float, 0},
	{SHL,	&type_int, 0, &type_int},	//FIXME?
	{SHR,	&type_int, 0, &type_int},	//FIXME?
	{EQ,	&type_int, &type_float, 0},
	{NE,	&type_int, &type_float, 0},
	{LE,	&type_int, &type_float, 0},
	{GE,	&type_int, &type_float, 0},
	{LT,	&type_int, &type_float, 0},
	{GT,	&type_int, &type_float, 0},
	{0, 0}
};

static expr_type_t int_vector[] = {
	{'*',	&type_vector, &type_float, 0},
	{0, 0}
};

static expr_type_t int_pointer[] = {
	{'+',	0, 0, 0, pointer_arithmetic},
	{0, 0}
};

static expr_type_t int_quat[] = {
	{'*',	&type_quaternion, &type_float, 0},
	{0, 0}
};

static expr_type_t int_int[] = {
	{'+',	&type_int},
	{'-',	&type_int},
	{'*',	&type_int},
	{'/',	&type_int},
	{'&',	&type_int},
	{'|',	&type_int},
	{'^',	&type_int},
	{'%',	&type_int},
	{MOD,	&type_int},
	{SHL,	&type_int},
	{SHR,	&type_int},
	{AND,	&type_int},
	{OR,	&type_int},
	{EQ,	&type_int},
	{NE,	&type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};

static expr_type_t int_uint[] = {
	{'+',	&type_int, 0, &type_int},
	{'-',	&type_int, 0, &type_int},
	{'*',	&type_int, 0, &type_int},
	{'/',	&type_int, 0, &type_int},
	{'&',	&type_int, 0, &type_int},
	{'|',	&type_int, 0, &type_int},
	{'^',	&type_int, 0, &type_int},
	{'%',	&type_int, 0, &type_int},
	{MOD,	&type_int, 0, &type_int},
	{SHL,	&type_int, 0, &type_int},
	{SHR,	&type_int, 0, &type_int},
	{EQ,	&type_int, 0, &type_int},
	{NE,	&type_int, 0, &type_int},
	{LE,	&type_int, 0, &type_int},
	{GE,	&type_int, 0, &type_int},
	{LT,	&type_int, 0, &type_int},
	{GT,	&type_int, 0, &type_int},
	{0, 0}
};

static expr_type_t int_short[] = {
	{'+',	&type_int, 0, &type_int},
	{'-',	&type_int, 0, &type_int},
	{'*',	&type_int, 0, &type_int},
	{'/',	&type_int, 0, &type_int},
	{'&',	&type_int, 0, &type_int},
	{'|',	&type_int, 0, &type_int},
	{'^',	&type_int, 0, &type_int},
	{'%',	&type_int, 0, &type_int},
	{MOD,	&type_int, 0, &type_int},
	{SHL,	&type_int, 0, &type_int},
	{SHR,	&type_int, 0, &type_int},
	{EQ,	&type_int, 0, &type_int},
	{NE,	&type_int, 0, &type_int},
	{LE,	&type_int, 0, &type_int},
	{GE,	&type_int, 0, &type_int},
	{LT,	&type_int, 0, &type_int},
	{GT,	&type_int, 0, &type_int},
	{0, 0}
};

static expr_type_t int_double[] = {
	{'+',	&type_double, &type_double, 0},
	{'-',	&type_double, &type_double, 0},
	{'*',	&type_double, &type_double, 0},
	{'/',	&type_double, &type_double, 0},
	{'%',	&type_double, &type_double, 0},
	{MOD,	&type_double, &type_double, 0},
	{EQ,	&type_int, &type_double, 0},
	{NE,	&type_int, &type_double, 0},
	{LE,	&type_int, &type_double, 0},
	{GE,	&type_int, &type_double, 0},
	{LT,	&type_int, &type_double, 0},
	{GT,	&type_int, &type_double, 0},
	{0, 0}
};

#define uint_float int_float
#define uint_vector int_vector
#define uint_pointer int_pointer
#define uint_quat int_quat

static expr_type_t uint_int[] = {
	{'+',	&type_int, &type_int, &type_int },
	{'-',	&type_int, &type_int, &type_int },
	{'*',	&type_int, &type_int, &type_int },
	{'/',	&type_int, &type_int, &type_int },
	{'&',	&type_int, &type_int, &type_int },
	{'|',	&type_int, &type_int, &type_int },
	{'^',	&type_int, &type_int, &type_int },
	{'%',	&type_int, &type_int, &type_int },
	{MOD,	&type_int, &type_int, &type_int },
	{SHL,	&type_uint, &type_int, &type_int },
	{SHR,	&type_uint, 0,        &type_int },
	{EQ,	&type_int, &type_int, &type_int },
	{NE,	&type_int, &type_int, &type_int },
	{LE,	&type_int, &type_int, &type_int },
	{GE,	&type_int, &type_int, &type_int },
	{LT,	&type_int, &type_int, &type_int },
	{GT,	&type_int, &type_int, &type_int },
	{0, 0}
};

static expr_type_t uint_uint[] = {
	{'+',	&type_uint},
	{'-',	&type_uint},
	{'*',	&type_uint},
	{'/',	&type_uint},
	{'&',	&type_uint},
	{'|',	&type_uint},
	{'^',	&type_uint},
	{'%',	&type_uint},
	{MOD,	&type_uint},
	{SHL,	&type_uint},
	{SHR,	&type_uint},
	{EQ,	&type_int, &type_int, &type_int},
	{NE,	&type_int, &type_int, &type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};
#define uint_short uint_int
#define uint_double int_double

#define short_float int_float
#define short_vector int_vector
#define short_pointer int_pointer
#define short_quat int_quat

static expr_type_t short_int[] = {
	{'+',	&type_int, &type_int, 0},
	{'-',	&type_int, &type_int, 0},
	{'*',	&type_int, &type_int, 0},
	{'/',	&type_int, &type_int, 0},
	{'&',	&type_int, &type_int, 0},
	{'|',	&type_int, &type_int, 0},
	{'^',	&type_int, &type_int, 0},
	{'%',	&type_int, &type_int, 0},
	{MOD,	&type_int, &type_int, 0},
	{SHL,	&type_short},
	{SHR,	&type_short},
	{EQ,	&type_int, &type_int, 0},
	{NE,	&type_int, &type_int, 0},
	{LE,	&type_int, &type_int, 0},
	{GE,	&type_int, &type_int, 0},
	{LT,	&type_int, &type_int, 0},
	{GT,	&type_int, &type_int, 0},
	{0, 0}
};

static expr_type_t short_uint[] = {
	{'+',	&type_uint, &type_uint, 0},
	{'-',	&type_uint, &type_uint, 0},
	{'*',	&type_uint, &type_uint, 0},
	{'/',	&type_uint, &type_uint, 0},
	{'&',	&type_uint, &type_uint, 0},
	{'|',	&type_uint, &type_uint, 0},
	{'^',	&type_uint, &type_uint, 0},
	{'%',	&type_uint, &type_uint, 0},
	{MOD,	&type_uint, &type_uint, 0},
	{SHL,	&type_short},
	{SHR,	&type_short},
	{EQ,	&type_int, &type_uint, 0},
	{NE,	&type_int, &type_uint, 0},
	{LE,	&type_int, &type_uint, 0},
	{GE,	&type_int, &type_uint, 0},
	{LT,	&type_int, &type_uint, 0},
	{GT,	&type_int, &type_uint, 0},
	{0, 0}
};

static expr_type_t short_short[] = {
	{'+',	&type_short},
	{'-',	&type_short},
	{'*',	&type_short},
	{'/',	&type_short},
	{'&',	&type_short},
	{'|',	&type_short},
	{'^',	&type_short},
	{'%',	&type_short},
	{MOD,	&type_short},
	{SHL,	&type_short},
	{SHR,	&type_short},
	{EQ,	&type_int},
	{NE,	&type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};
#define short_double int_double

static expr_type_t double_float[] = {
	{'+',	&type_double, 0, &type_double},
	{'-',	&type_double, 0, &type_double},
	{'*',	&type_double, 0, &type_double},
	{'/',	&type_double, 0, &type_double},
	{'%',	&type_double, 0, &type_double},
	{MOD,	&type_double, 0, &type_double},
	{EQ,	0, 0, 0, double_compare},
	{NE,	0, 0, 0, double_compare},
	{LE,	0, 0, 0, double_compare},
	{GE,	0, 0, 0, double_compare},
	{LT,	0, 0, 0, double_compare},
	{GT,	0, 0, 0, double_compare},
	{0, 0}
};

static expr_type_t double_vector[] = {
	{'*',	&type_vector},
	{0, 0}
};

static expr_type_t double_quat[] = {
	{'*',	&type_quaternion},
	{0, 0}
};

static expr_type_t double_int[] = {
	{'+',	&type_double, 0, &type_double},
	{'-',	&type_double, 0, &type_double},
	{'*',	&type_double, 0, &type_double},
	{'/',	&type_double, 0, &type_double},
	{'%',	&type_double, 0, &type_double},
	{MOD,	&type_double, 0, &type_double},
	{EQ,	0, 0, 0, double_compare},
	{NE,	0, 0, 0, double_compare},
	{LE,	0, 0, 0, double_compare},
	{GE,	0, 0, 0, double_compare},
	{LT,	0, 0, 0, double_compare},
	{GT,	0, 0, 0, double_compare},
	{0, 0}
};
#define double_uint double_int
#define double_short double_int

static expr_type_t double_double[] = {
	{'+',	&type_double},
	{'-',	&type_double},
	{'*',	&type_double},
	{'/',	&type_double},
	{'%',	&type_double},
	{MOD,	&type_double},
	{EQ,	&type_int},
	{NE,	&type_int},
	{LE,	&type_int},
	{GE,	&type_int},
	{LT,	&type_int},
	{GT,	&type_int},
	{0, 0}
};

static expr_type_t *string_x[ev_type_count] = {
	[ev_string] = string_string,
};

static expr_type_t *float_x[ev_type_count] = {
	[ev_float] = float_float,
	[ev_vector] = float_vector,
	[ev_quaternion] = float_quat,
	[ev_int] = float_int,
	[ev_uint] = float_uint,
	[ev_short] = float_short,
	[ev_double] = float_double,
};

static expr_type_t *vector_x[ev_type_count] = {
	[ev_float] = vector_float,
	[ev_vector] = vector_vector,
	[ev_int] = vector_int,
	[ev_uint] = vector_uint,
	[ev_short] = vector_short,
	[ev_double] = vector_double,
};

static expr_type_t *entity_x[ev_type_count] = {
	[ev_entity] = entity_entity,
};

static expr_type_t *field_x[ev_type_count] = {
	[ev_field] = field_field,
};

static expr_type_t *func_x[ev_type_count] = {
	[ev_func] = func_func,
};

static expr_type_t *pointer_x[ev_type_count] = {
	[ev_ptr] = pointer_pointer,
	[ev_int] = pointer_int,
	[ev_uint] = pointer_uint,
	[ev_short] = pointer_short,
};

static expr_type_t *quat_x[ev_type_count] = {
	[ev_float] = quat_float,
	[ev_vector] = quat_vector,
	[ev_quaternion] = quat_quat,
	[ev_int] = quat_int,
	[ev_uint] = quat_uint,
	[ev_short] = quat_short,
	[ev_double] = quat_double,
};

static expr_type_t *int_x[ev_type_count] = {
	[ev_float] = int_float,
	[ev_vector] = int_vector,
	[ev_ptr] = int_pointer,
	[ev_quaternion] = int_quat,
	[ev_int] = int_int,
	[ev_uint] = int_uint,
	[ev_short] = int_short,
	[ev_double] = int_double,
};

static expr_type_t *uint_x[ev_type_count] = {
	[ev_float] = uint_float,
	[ev_vector] = uint_vector,
	[ev_ptr] = uint_pointer,
	[ev_quaternion] = uint_quat,
	[ev_int] = uint_int,
	[ev_uint] = uint_uint,
	[ev_short] = uint_short,
	[ev_double] = uint_double,
};

static expr_type_t *short_x[ev_type_count] = {
	[ev_float] = short_float,
	[ev_vector] = short_vector,
	[ev_ptr] = short_pointer,
	[ev_quaternion] = short_quat,
	[ev_int] = short_int,
	[ev_uint] = short_uint,
	[ev_short] = short_short,
	[ev_double] = short_double,
};

static expr_type_t *double_x[ev_type_count] = {
	[ev_float] = double_float,
	[ev_vector] = double_vector,
	[ev_quaternion] = double_quat,
	[ev_int] = double_int,
	[ev_uint] = double_uint,
	[ev_short] = double_short,
	[ev_double] = double_double,
};

static expr_type_t **binary_expr_types[ev_type_count] = {
	[ev_string] = string_x,
	[ev_float] = float_x,
	[ev_vector] = vector_x,
	[ev_entity] = entity_x,
	[ev_field] = field_x,
	[ev_func] = func_x,
	[ev_ptr] = pointer_x,
	[ev_quaternion] = quat_x,
	[ev_int] = int_x,
	[ev_uint] = uint_x,
	[ev_short] = short_x,
	[ev_double] = double_x
};

static expr_t *
pointer_arithmetic (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	expr_t     *ptr;
	expr_t     *offset;
	expr_t     *psize;
	type_t     *ptype;

	if (!is_ptr (t1) && !is_ptr (t2)) {
		internal_error (e1, "pointer arithmetic on non-pointers");
	}
	if (is_ptr (t1) && is_ptr (t2)) {
		if (op != '-') {
			return error (e2, "invalid pointer operation");
		}
		if (t1 != t2) {
			return error (e2, "cannot use %c on pointers of different types",
						  op);
		}
		e1 = cast_expr (&type_int, e1);
		e2 = cast_expr (&type_int, e2);
		psize = new_int_expr (type_size (t1->t.fldptr.type));
		return binary_expr ('/', binary_expr ('-', e1, e2), psize);
	} else if (is_ptr (t1)) {
		offset = cast_expr (&type_int, e2);
		ptr = e1;
		ptype = t1;
	} else if (is_ptr (t2)) {
		offset = cast_expr (&type_int, e1);
		ptr = e2;
		ptype = t2;
	}
	// op is known to be + or -
	psize = new_int_expr (type_size (ptype->t.fldptr.type));
	offset = unary_expr (op, binary_expr ('*', offset, psize));
	return offset_pointer_expr (ptr, offset);
}

static expr_t *
pointer_compare (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	expr_t     *e;

	if (!type_assignable (t1, t2)) {
		return error (e2, "cannot use %s on pointers of different types",
					  get_op_string (op));
	}
	if (options.code.progsversion < PROG_VERSION) {
		e = new_binary_expr (op, e1, e2);
	} else {
		e = new_binary_expr (op, cast_expr (&type_int, e1),
							 cast_expr (&type_int, e2));
	}
	e->e.expr.type = &type_int;
	return e;
}

static expr_t *
func_compare (int op, expr_t *e1, expr_t *e2)
{
	expr_t     *e;

	if (options.code.progsversion < PROG_VERSION) {
		e = new_binary_expr (op, e1, e2);
	} else {
		e = new_binary_expr (op, new_alias_expr (&type_int, e1),
							 new_alias_expr (&type_int, e2));
	}
	e->e.expr.type = &type_int;
	return e;
}

static expr_t *
inverse_multiply (int op, expr_t *e1, expr_t *e2)
{
	// There is no vector/float or quaternion/float instruction and adding
	// one would mean the engine would have to do 1/f every time
	expr_t     *one = new_float_expr (1);
	return binary_expr ('*', e1, binary_expr ('/', one, e2));
}

static expr_t *
vector_compare (int op, expr_t *e1, expr_t *e2)
{
	if (options.code.progsversion < PROG_VERSION) {
		expr_t     *e = new_binary_expr (op, e1, e2);
		e->e.expr.type = &type_int;
		return e;
	}
	int         hop = op == EQ ? '&' : '|';
	e1 = new_alias_expr (&type_vec3, e1);
	e2 = new_alias_expr (&type_vec3, e2);
	expr_t     *e = new_binary_expr (op, e1, e2);
	e->e.expr.type = &type_ivec3;
	return new_horizontal_expr (hop, e, &type_int);
}

static expr_t *
double_compare (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	expr_t     *e;

	if (is_constant (e1) && e1->implicit && is_double (t1) && is_float (t2)) {
		t1 = &type_float;
		convert_double (e1);
	}
	if (is_float (t1) && is_constant (e2) && e2->implicit && is_double (t2)) {
		t2 = &type_float;
		convert_double (e2);
	}
	if (is_double (t1)) {
		if (is_float (t2)) {
			warning (e2, "comparison between double and float");
		} else if (!is_constant (e2)) {
			warning (e2, "comparison between double and int");
		}
		e2 = cast_expr (&type_double, e2);
	} else if (is_double (t2)) {
		if (is_float (t1)) {
			warning (e1, "comparison between float and double");
		} else if (!is_constant (e1)) {
			warning (e1, "comparison between int and double");
		}
		e1 = cast_expr (&type_double, e1);
	}
	e = new_binary_expr (op, e1, e2);
	e->e.expr.type = &type_int;
	return e;
}

static expr_t *
invalid_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	etype_t     t1, t2;
	t1 = extract_type (e1);
	t2 = extract_type (e2);
	return error (e1, "invalid binary expression: %s %s %s",
				  pr_type_name[t1], get_op_string (op), pr_type_name[t2]);
}

static expr_t *
reimplement_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	expr_t     *e;

	if (options.code.progsversion == PROG_ID_VERSION) {
		switch (op) {
			case '%':
				{
					expr_t     *tmp1, *tmp2;
					e = new_block_expr ();
					tmp1 = new_temp_def_expr (&type_float);
					tmp2 = new_temp_def_expr (&type_float);

					append_expr (e, assign_expr (tmp1, binary_expr ('/', e1, e2)));
					append_expr (e, assign_expr (tmp2, binary_expr ('&', tmp1, tmp1)));
					e->e.block.result = binary_expr ('-', e1, binary_expr ('*', e2, tmp2));
					return e;
				}
				break;
		}
	}
	return 0;
}

static expr_t *
check_precedence (int op, expr_t *e1, expr_t *e2)
{
	if (e1->type == ex_uexpr && e1->e.expr.op == '!' && !e1->paren) {
		if (options.traditional) {
			if (op != AND && op != OR) {
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
				|| (e2->e.expr.op == OR || e2->e.expr.op == AND)) {
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
				|| (e1->e.expr.op == OR || e1->e.expr.op == AND)) {
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

static int is_call (expr_t *e)
{
	return e->type == ex_block && e->e.block.is_call;
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	etype_t     et1, et2;
	expr_t     *e;
	expr_type_t *expr_type;

	convert_name (e1);
	e1 = convert_vector (e1);
	// FIXME this is target-specific info and should not be in the
	// expression tree
	if (e1->type == ex_alias && is_call (e1->e.alias.expr)) {
		// move the alias expression inside the block so the following check
		// can detect the call and move the temp assignment into the block
		expr_t     *block = e1->e.alias.expr;
		e1->e.alias.expr = block->e.block.result;
		block->e.block.result = e1;
		e1 = block;
	}
	if (e1->type == ex_block && e1->e.block.is_call
		&& has_function_call (e2) && e1->e.block.result) {
		// the temp assignment needs to be insided the block so assignment
		// code generation doesn't see it when applying right-associativity
		expr_t    *tmp = new_temp_def_expr (get_type (e1->e.block.result));
		e = assign_expr (tmp, e1->e.block.result);
		append_expr (e1, e);
		e1->e.block.result = tmp;
	}
	if (e1->type == ex_error)
		return e1;

	convert_name (e2);
	e2 = convert_vector (e2);
	if (e2->type == ex_error)
		return e2;

	if (e1->type == ex_bool)
		e1 = convert_from_bool (e1, get_type (e2));
	if (e2->type == ex_bool)
		e2 = convert_from_bool (e2, get_type (e1));

	if ((e = check_precedence (op, e1, e2)))
		return e;

	t1 = get_type (e1);
	t2 = get_type (e2);
	if (!t1 || !t2)
		internal_error (e1, "expr with no type");

	if (op == EQ || op == NE) {
		if (e1->type == ex_nil) {
			t1 = t2;
			convert_nil (e1, t1);
		} else if (e2->type == ex_nil) {
			t2 = t1;
			convert_nil (e2, t2);
		}
	}

	if (is_constant (e1) && is_double (t1) && e1->implicit && is_float (t2)) {
		t1 = &type_float;
		convert_double (e1);
	}
	if (is_constant (e2) && is_double (t2) && e2->implicit && is_float (t1)) {
		t2 = &type_float;
		convert_double (e2);
	}

	et1 = low_level_type (t1);
	et2 = low_level_type (t2);

	if (et1 >= ev_type_count || !binary_expr_types[et1])
		return invalid_binary_expr(op, e1, e2);
	if (et2 >= ev_type_count || !binary_expr_types[et1][et2])
		return invalid_binary_expr(op, e1, e2);
	expr_type = binary_expr_types[et1][et2];
	while (expr_type->op && expr_type->op != op)
		expr_type++;
	if (!expr_type->op)
		return invalid_binary_expr(op, e1, e2);

	if (expr_type->a_cast)
		e1 = cast_expr (expr_type->a_cast, e1);
	if (expr_type->b_cast)
		e2 = cast_expr (expr_type->b_cast, e2);
	if (expr_type->process) {
		return fold_constants (expr_type->process (op, e1, e2));
	}

	if ((e = reimplement_binary_expr (op, e1, e2)))
		return fold_constants (e);

	e = new_binary_expr (op, e1, e2);
	e->e.expr.type = expr_type->result_type;
	if (is_compare (op) || is_logic (op)) {
		if (options.code.progsversion == PROG_ID_VERSION) {
			e->e.expr.type = &type_float;
		}
	}
	return fold_constants (e);
}
