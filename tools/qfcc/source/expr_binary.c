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

#include "tools/qfcc/include/algebra.h"
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
static expr_t *vector_dot (int op, expr_t *e1, expr_t *e2);
static expr_t *vector_multiply (int op, expr_t *e1, expr_t *e2);
static expr_t *vector_scale (int op, expr_t *e1, expr_t *e2);
static expr_t *entity_compare (int op, expr_t *e1, expr_t *e2);

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
	{'*',	.process = vector_scale },
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
	{EQ,	.process = double_compare},
	{NE,	.process = double_compare},
	{LE,	.process = double_compare},
	{GE,	.process = double_compare},
	{LT,	.process = double_compare},
	{GT,	.process = double_compare},
	{0, 0}
};

static expr_type_t vector_float[] = {
	{'*',	.process = vector_scale },
	{'/',	.process = inverse_multiply},
	{0, 0}
};

static expr_type_t vector_vector[] = {
	{'+',	&type_vector},
	{'-',	&type_vector},
	{DOT,	.process = vector_dot},
	{CROSS,	&type_vector},
	{HADAMARD,	&type_vector},
	{'*',	.process = vector_multiply},
	{EQ,	.process = vector_compare},
	{NE,	.process = vector_compare},
	{0, 0}
};

#define vector_int vector_float
#define vector_uint vector_float
#define vector_short vector_float

static expr_type_t vector_double[] = {
	{'*',	&type_vector, 0, &type_float, vector_scale},
	{'/',	.process = inverse_multiply},
	{0, 0}
};

static expr_type_t entity_entity[] = {
	{EQ,	&type_int, 0, 0, entity_compare},
	{NE,	&type_int, 0, 0, entity_compare},
	{0, 0}
};

static expr_type_t field_field[] = {
	{EQ,	&type_int},
	{NE,	&type_int},
	{0, 0}
};

static expr_type_t func_func[] = {
	{EQ,	.process = func_compare},
	{NE,	.process = func_compare},
	{0, 0}
};

static expr_type_t pointer_pointer[] = {
	{'-',	.process = pointer_arithmetic},
	{EQ,	.process = pointer_compare},
	{NE,	.process = pointer_compare},
	{LE,	.process = pointer_compare},
	{GE,	.process = pointer_compare},
	{LT,	.process = pointer_compare},
	{GT,	.process = pointer_compare},
	{0, 0}
};

static expr_type_t pointer_int[] = {
	{'+',	.process = pointer_arithmetic},
	{'-',	.process = pointer_arithmetic},
	{0, 0}
};
#define pointer_uint pointer_int
#define pointer_short pointer_int

static expr_type_t quat_float[] = {
	{'*',	&type_quaternion},
	{'/',	.process = inverse_multiply},
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
	{'/',	.process = inverse_multiply},
	{0, 0}
};
#define quat_uint quat_int
#define quat_short quat_int

static expr_type_t quat_double[] = {
	{'*',	&type_quaternion},
	{'/',	.process = inverse_multiply},
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
	{'*',	&type_vector, &type_float, 0, vector_scale},
	{0, 0}
};

static expr_type_t int_pointer[] = {
	{'+',	.process = pointer_arithmetic},
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
	{EQ,	&type_long, &type_double, 0},
	{NE,	&type_long, &type_double, 0},
	{LE,	&type_long, &type_double, 0},
	{GE,	&type_long, &type_double, 0},
	{LT,	&type_long, &type_double, 0},
	{GT,	&type_long, &type_double, 0},
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
	{EQ,	.process = double_compare},
	{NE,	.process = double_compare},
	{LE,	.process = double_compare},
	{GE,	.process = double_compare},
	{LT,	.process = double_compare},
	{GT,	.process = double_compare},
	{0, 0}
};

static expr_type_t double_vector[] = {
	{'*',	&type_vector, &type_float, 0, vector_scale},
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
	{EQ,	.process = double_compare},
	{NE,	.process = double_compare},
	{LE,	.process = double_compare},
	{GE,	.process = double_compare},
	{LT,	.process = double_compare},
	{GT,	.process = double_compare},
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
	{EQ,	&type_long},
	{NE,	&type_long},
	{LE,	&type_long},
	{GE,	&type_long},
	{LT,	&type_long},
	{GT,	&type_long},
	{0, 0}
};

static expr_type_t long_long[] = {
	{'+',	&type_long},
	{'-',	&type_long},
	{'*',	&type_long},
	{'/',	&type_long},
	{'&',	&type_long},
	{'|',	&type_long},
	{'^',	&type_long},
	{'%',	&type_long},
	{MOD,	&type_long},
	{SHL,	&type_long},
	{SHR,	&type_long},
	{EQ,	&type_long},
	{NE,	&type_long},
	{LE,	&type_long},
	{GE,	&type_long},
	{LT,	&type_long},
	{GT,	&type_long},
	{0, 0}
};

static expr_type_t ulong_ulong[] = {
	{'+',	&type_ulong},
	{'-',	&type_ulong},
	{'*',	&type_ulong},
	{'/',	&type_ulong},
	{'&',	&type_ulong},
	{'|',	&type_ulong},
	{'^',	&type_ulong},
	{'%',	&type_ulong},
	{MOD,	&type_ulong},
	{SHL,	&type_ulong},
	{SHR,	&type_ulong},
	{EQ,	&type_long},
	{NE,	&type_long},
	{LE,	&type_long},
	{GE,	&type_long},
	{LT,	&type_long},
	{GT,	&type_long},
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

static expr_type_t *long_x[ev_type_count] = {
	[ev_long] = long_long,
};

static expr_type_t *ulong_x[ev_type_count] = {
	[ev_ulong] = ulong_ulong,
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
	[ev_double] = double_x,
	[ev_long] = long_x,
	[ev_ulong] = ulong_x,
};

// supported operators for scalar-vector expressions
static int scalar_vec_ops[] = { '*', '/', '%', MOD, 0 };
static expr_t *
convert_scalar (expr_t *scalar, int op, expr_t *vec)
{
	int        *s_op = scalar_vec_ops;
	while (*s_op && *s_op != op) {
		s_op++;
	}
	if (!*s_op) {
		return 0;
	}

	// expand the scalar to a vector of the same width as vec
	type_t     *vec_type = get_type (vec);

	if (is_constant (scalar)) {
		for (int i = 1; i < type_width (get_type (vec)); i++) {
			expr_t     *s = copy_expr (scalar);
			s->next = scalar;
			scalar = s;
		}
		return new_vector_list (scalar);
	}

	return new_extend_expr (scalar, vec_type, 2, false);//2 = copy
}

static expr_t *
pointer_arithmetic (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	expr_t     *ptr = 0;
	expr_t     *offset = 0;
	expr_t     *psize;
	type_t     *ptype = 0;

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
	if (options.code.progsversion == PROG_ID_VERSION) {
		e->e.expr.type = &type_float;
	}
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
		if (options.code.progsversion == PROG_ID_VERSION) {
			e->e.expr.type = &type_float;
		}
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
vector_dot (int op, expr_t *e1, expr_t *e2)
{
	expr_t     *e = new_binary_expr (DOT, e1, e2);
	e->e.expr.type = &type_float;
	return e;
}

static expr_t *
vector_multiply (int op, expr_t *e1, expr_t *e2)
{
	if (options.math.vector_mult == DOT) {
		// vector * vector is dot product in v6 progs (ick)
		return vector_dot (op, e1, e2);
	}
	// component-wise multiplication
	expr_t     *e = new_binary_expr ('*', e1, e2);
	e->e.expr.type = &type_vector;
	return e;
}

static expr_t *
vector_scale (int op, expr_t *e1, expr_t *e2)
{
	// Ensure the expression is always vector * scalar. The operation is
	// always commutative, and the Ruamoko ISA supports only vector * scalar
	// (though v6 does support scalar * vector, one less if).
	if (is_scalar (get_type (e1))) {
		expr_t     *t = e1;
		e1 = e2;
		e2 = t;
	}
	expr_t     *e = new_binary_expr (SCALE, e1, e2);
	e->e.expr.type = get_type (e1);
	return e;
}

static expr_t *
double_compare (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1 = get_type (e1);
	type_t     *t2 = get_type (e2);
	expr_t     *e;

	if (is_constant (e1) && e1->implicit && is_double (t1) && is_float (t2)) {
		t1 = &type_float;
		e1 = cast_expr (t1, e1);
	}
	if (is_float (t1) && is_constant (e2) && e2->implicit && is_double (t2)) {
		t2 = &type_float;
		e2 = cast_expr (t2, e2);
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
	e->e.expr.type = &type_long;
	return e;
}

static expr_t *
entity_compare (int op, expr_t *e1, expr_t *e2)
{
	if (options.code.progsversion == PROG_VERSION) {
		e1 = new_alias_expr (&type_int, e1);
		e2 = new_alias_expr (&type_int, e2);
	}
	expr_t     *e = new_binary_expr (op, e1, e2);
	e->e.expr.type = &type_int;
	if (options.code.progsversion == PROG_ID_VERSION) {
		e->e.expr.type = &type_float;
	}
	return e;
}

#define invalid_binary_expr(_op, _e1, _e2) \
	_invalid_binary_expr(_op, _e1, _e2, __FILE__, __LINE__, __FUNCTION__)
static expr_t *
_invalid_binary_expr (int op, expr_t *e1, expr_t *e2,
					  const char *file, int line, const char *func)
{
	type_t     *t1, *t2;
	t1 = get_type (e1);
	t2 = get_type (e2);
	return _error (e1, file, line, func, "invalid binary expression: %s %s %s",
				  get_type_string (t1), get_op_string (op),
				  get_type_string (t2));
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
					&&(e2->e.expr.op == OR || e2->e.expr.op == AND))) {
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
					&&(e2->e.expr.op == OR || e2->e.expr.op == AND))) {
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

static type_t *
promote_type (type_t *dst, type_t *src)
{
	if (is_vector (dst) || is_quaternion (dst)) {
		return dst;
	}
	return vector_type (base_type (dst), type_width (src));
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	type_t     *t1, *t2;
	etype_t     et1, et2;
	expr_t     *e;
	expr_type_t *expr_type;

	convert_name (e1);
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

	if (is_algebra (t1) || is_algebra (t2)) {
		return algebra_binary_expr (op, e1, e2);
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

	if (is_constant (e1) && is_double (t1) && e1->implicit && is_float (t2)) {
		t1 = float_type (t2);
		e1 = cast_expr (t1, e1);
	}
	if (is_constant (e2) && is_double (t2) && e2->implicit && is_float (t1)) {
		t2 = float_type (t1);
		e2 = cast_expr (t2, e2);
	}
	if (is_array (t1) && (is_ptr (t2) || is_integral (t2))) {
		t1 = pointer_type (t1->t.array.type);
		e1 = cast_expr (t1, e1);
	}
	if (is_array (t2) && (is_ptr (t1) || is_integral (t1))) {
		t2 = pointer_type (t2->t.array.type);
		e2 = cast_expr (t2, e2);
	}

	et1 = low_level_type (t1);
	et2 = low_level_type (t2);

	if (et1 >= ev_type_count || !binary_expr_types[et1])
		return invalid_binary_expr(op, e1, e2);
	if (et2 >= ev_type_count || !binary_expr_types[et1][et2])
		return invalid_binary_expr(op, e1, e2);

	if ((t1->width > 1 || t2->width > 1)) {
		// vector/quaternion and scalar won't get here as vector and quaternion
		// are distict types with type.width == 1, but vector and vec3 WILL get
		// here because of vec3 being float{3}
		if (t1 != t2) {
			type_t     *pt1 = t1;
			type_t     *pt2 = t2;
			if (is_float (base_type (t1)) && is_double (base_type (t2))
				&& e2->implicit) {
				pt2 = promote_type (t1, t2);
			} else if (is_double (base_type (t1)) && is_float (base_type (t2))
					   && e1->implicit) {
				pt1 = promote_type (t2, t1);
			} else if (type_promotes (base_type (t1), base_type (t2))) {
				pt2 = promote_type (t1, t2);
			} else if (type_promotes (base_type (t2), base_type (t1))) {
				pt1 = promote_type (t2, t1);
			} else if (base_type (t1) == base_type (t2)) {
				if (is_vector (t1) || is_quaternion (t1)) {
					pt2 = t1;
				} else if (is_vector (t2) || is_quaternion (t2)) {
					pt1 = t2;
				}
			} else {
				debug (e1, "%d %d\n", e1->implicit, e2->implicit);
				return invalid_binary_expr (op, e1, e2);
			}
			if (pt1 != t1) {
				e1 = cast_expr (pt1, e1);
				t1 = pt1;
			}
			if (pt2 != t2) {
				e2 = cast_expr (pt2, e2);
				t2 = pt2;
			}
		}
		int         scalar_op = 0;
		if (type_width (t1) == 1) {
			// scalar op vec
			if (!(e = convert_scalar (e1, op, e2))) {
				return invalid_binary_expr (op, e1, e2);
			}
			scalar_op = 1;
			e1 = e;
			t1 = get_type (e1);
		}
		if (type_width (t2) == 1) {
			// vec op scalar
			if (!(e = convert_scalar (e2, op, e1))) {
				return invalid_binary_expr (op, e1, e2);
			}
			scalar_op = 1;
			e2 = e;
			t2 = get_type (e2);
		}
		if (scalar_op && op == '*') {
			op = HADAMARD;
		}
		if (type_width (t1) != type_width (t2)) {
			// vec op vec of different widths
			return invalid_binary_expr (op, e1, e2);
		}
		t1 = get_type (e1);
		t2 = get_type (e2);
		et1 = low_level_type (t1);
		et2 = low_level_type (t2);
		// both widths are the same at this point
		if (t1->width > 1) {
			e = new_binary_expr (op, e1, e2);
			if (is_compare (op)) {
				t1 = int_type (t1);
			}
			if (op == DOT) {
				if (!is_real (t1)) {
					return invalid_binary_expr (op, e1, e2);
				}
				t1 = base_type (t1);
			}
			e->e.expr.type = t1;
			return e;
		}
	}

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
