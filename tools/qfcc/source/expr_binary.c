/*
	expr_binary.c

	Binary expression manipulation

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
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

typedef struct {
	bool      (*match_a) (const type_t *type);
	bool      (*match_b) (const type_t *type);
	bool      (*match_shape) (const type_t *a, const type_t *b);
	const type_t *(*res_type) (const type_t *a, const type_t *b);
	const expr_t *(*process) (int op, const expr_t *e1, const expr_t *e2);
	bool        promote;
	bool        no_implicit;
	bool      (*commutative) (void);
	bool      (*anticommute) (void);
	bool      (*associative) (void);
	int         true_op;
} expr_type_t;

static bool
is_vector_compat (const type_t *type)
{
	return is_vector (type) || (is_nonscalar (type) && type_width (type) == 3);
}

static bool
is_quaternion_compat (const type_t *type)
{
	return is_quaternion (type) || (is_nonscalar (type)
									&& type_width (type) == 4);
}

static const expr_t *
pointer_arithmetic (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	const expr_t *ptr = 0;
	const expr_t *offset = 0;
	const expr_t *psize;
	const type_t *ptype = 0;

	if (!is_pointer (t1) && !is_pointer (t2)) {
		internal_error (e1, "pointer arithmetic on non-pointers");
	}
	if (is_pointer (t1) && is_pointer (t2)) {
		if (op != '-') {
			return error (e2, "invalid pointer operation");
		}
		if (t1 != t2) {
			return error (e2, "cannot use %c on pointers of different types",
						  op);
		}
		e1 = cast_expr (&type_int, e1);
		e2 = cast_expr (&type_int, e2);
		psize = new_int_expr (type_size (t1->fldptr.type), false);
		return binary_expr ('/', binary_expr ('-', e1, e2), psize);
	} else if (is_pointer (t1)) {
		offset = cast_expr (&type_int, e2);
		ptr = e1;
		ptype = t1;
	} else if (is_pointer (t2)) {
		offset = cast_expr (&type_int, e1);
		ptr = e2;
		ptype = t2;
	}
	// op is known to be + or -
	psize = new_int_expr (type_size (ptype->fldptr.type), false);
	offset = unary_expr (op, binary_expr ('*', offset, psize));
	return offset_pointer_expr (ptr, offset);
}

static const expr_t *
pointer_compare (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
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
	e->expr.type = &type_bool;
	return e;
}

static const expr_t *
func_compare (int op, const expr_t *e1, const expr_t *e2)
{
	expr_t     *e;

	if (options.code.progsversion < PROG_VERSION) {
		e = new_binary_expr (op, e1, e2);
	} else {
		e = new_binary_expr (op, new_alias_expr (&type_int, e1),
							 new_alias_expr (&type_int, e2));
	}
	e->expr.type = &type_bool;
	if (options.code.progsversion == PROG_ID_VERSION) {
		e->expr.type = &type_float;
	}
	return e;
}

static const expr_t *
entity_compare (int op, const expr_t *e1, const expr_t *e2)
{
	if (options.code.progsversion == PROG_VERSION) {
		e1 = new_alias_expr (&type_int, e1);
		e2 = new_alias_expr (&type_int, e2);
	}
	expr_t     *e = new_binary_expr (op, e1, e2);
	e->expr.type = &type_bool;
	if (options.code.progsversion == PROG_ID_VERSION) {
		e->expr.type = &type_float;
	}
	return e;
}

static const expr_t *
target_shift_op (int op, const expr_t *e1, const expr_t *e2)
{
	return current_target.shift_op (op, e1, e2);
}

static const type_t *
promote_type (const type_t *dst, const type_t *src)
{
	if ((is_vector (dst) || is_quaternion (dst))
		&& type_width (dst) == type_width (src)) {
		return dst;
	}
	return vector_type (base_type (dst), type_width (src));
}

static void
promote_exprs (const expr_t **e1, const expr_t **e2)
{
	auto t1 = get_type (*e1);
	auto t2 = get_type (*e2);

	if (is_enum (t1) && is_enum (t2)) {
		//FIXME proper backing type for enum like handle
		t1 = type_default;
		t2 = type_default;
	} else if (is_math (t1) && is_enum (t2)) {
		t2 = promote_type (t1, t2);
	} else if (is_math (t2) && is_enum (t1)) {
		t1 = promote_type (t2, t1);
	} else if ((is_vector (t1) || is_quaternion (t1))
			   && (is_float (t2) || is_bool (t2))) {
		t2 = promote_type (t1, t2);
	} else if ((is_vector (t2) || is_quaternion (t2))
			   && (is_float (t1) || is_bool (t2))) {
		t1 = promote_type (t2, t1);
	} else if (type_promotes (t1, t2)) {
		t2 = promote_type (t1, t2);
	} else if (type_promotes (t2, t1)) {
		t1 = promote_type (t2, t1);
	} else if (base_type (t1) != base_type (t2)) {
		internal_error (*e1, "failed to promote types %s %s",
						get_type_string (t1), get_type_string (t2));
	}
	*e1 = cast_expr (t1, *e1);
	*e2 = cast_expr (t2, *e2);
}

static const expr_t *
math_compare (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	if (is_matrix (t1) || is_matrix (t2)
		|| type_width (t1) != type_width (t2)) {
		//FIXME glsl does not support comparison of vectors using operators
		// (it uses functions)
		return error (e1, "cannot compare %s and %s",
					  get_type_string (t1), get_type_string (t2));
	}
	if (t1 != t2) {
		if (e1->implicit && type_demotes (t2, t1)) {
			t1 = promote_type (t2, t1);
		} else if (e2->implicit && type_demotes (t1, t2)) {
			t2 = promote_type (t1, t2);
		}
		e1 = cast_expr (t1, e1);
		e2 = cast_expr (t2, e2);
	}
	if (!type_compares (t1, t2)) {
		warning (e2, "comparison between %s and %s",
				 get_type_string (t1),
				 get_type_string (t2));
	}
	if (t1 != t2) {
		promote_exprs (&e1, &e2);
		t1 = get_type (e1);
		t2 = get_type (e2);
	}
	if (is_vector (t1) || is_quaternion (t1)) {
		return current_target.vector_compare (op, e1, e2);
	}

	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = bool_type (t1);
	return e;
}

static const expr_t *
matrix_binary_expr (int op, const expr_t *a, const expr_t *b)
{
	auto ta = get_type (a);
	auto tb = get_type (b);

	int rowsa = type_rows (ta);
	int colsb = type_cols (tb);

	int rowsc, colsc;

	if (is_nonscalar (ta)) {
		// vectors * matrix treats vector as row matrix, resulting in vector
		rowsc = colsb;
		colsc = 1;
	} else {
		rowsc = rowsa;
		colsc = colsb;
	}

	auto type = matrix_type (base_type (ta), colsc, rowsc);
	auto e = typed_binary_expr (type, op, a, b);
	return e;
}

static const expr_t *
matrix_scalar_mul (int op, const expr_t *a, const expr_t *b)
{
	auto ta = get_type (a);
	auto tb = get_type (b);

	if (is_scalar (ta)) {
		// ensure the expression is always matrix * scalar
		auto te = a;
		a = b;
		b = te;
		ta = tb;
	}

	op = QC_SCALE;
	auto e = typed_binary_expr (ta, op, a, b);
	return e;
}

static const expr_t *
convert_scalar (const expr_t *scalar, const expr_t *vec)
{
	// expand the scalar to a vector of the same width as vec
	auto vec_type = get_type (vec);
	// vec might actually be a matrix, so get its column "width"
	vec_type = vector_type (base_type (vec_type), type_width (vec_type));

	if (is_constant (scalar)) {
		int width = type_width (get_type (vec));
		const expr_t *elements[width];
		for (int i = 0; i < width; i++) {
			elements[i] = scalar;
		}
		auto scalar_list = new_list_expr (nullptr);
		list_gather (&scalar_list->list, elements, width);
		return new_vector_list (scalar_list);
	}

	return new_extend_expr (scalar, vec_type, 2, false);//2 = copy
}

static const expr_t *
matrix_scalar_expr (int op, const expr_t *a, const expr_t *b)
{
	scoped_src_loc (a);
	bool left = is_scalar (get_type (a));
	auto mat_type = get_type (left ? b : a);
	int count = type_cols (mat_type);
	const expr_t *a_cols[count];
	const expr_t *b_cols[count];
	if (left) {
		a_cols[0] = convert_scalar (a, b);
		for (int i = 0; i < count; i++) {
			a_cols[i] = a_cols[0];
			b_cols[i] = get_column (b, i);
		}
	} else {
		b_cols[0] = convert_scalar (b, a);
		for (int i = 0; i < count; i++) {
			a_cols[i] = get_column (a, i);
			b_cols[i] = b_cols[0];
		}
	}
	auto params = new_list_expr (nullptr);
	for (int i = 0; i < count; i++) {
		expr_append_expr (params, binary_expr (op, a_cols[i], b_cols[i]));
	}
	return constructor_expr (new_type_expr (mat_type), params);
}

static const expr_t *
matrix_scalar_div (int op, const expr_t *a, const expr_t *b)
{
	auto ta = get_type (a);

	if (is_vector (ta) || is_quaternion (ta) || is_matrix (ta)) {
		// There is no vector/float or quaternion/float instruction and adding
		// one would mean the engine would have to do 1/f every time
		// similar for matrix
		auto one = new_float_expr (1, false);
		return binary_expr ('*', a, binary_expr ('/', one, b));
	}
	b = convert_scalar (b, a);
	auto e = typed_binary_expr (ta, op, a, b);
	return e;
}

static const expr_t *
vector_scalar_expr (int op, const expr_t *a, const expr_t *b)
{
	scoped_src_loc (a);
	bool left = is_scalar (get_type (a));
	if (left) {
		a = convert_scalar (a, b);
	} else {
		b = convert_scalar (b, a);
	}
	return binary_expr (op, a, b);
}

static const expr_t *
vector_vector_mul (int op, const expr_t *a, const expr_t *b)
{
	expr_t     *e = new_binary_expr ('*', a, b);
	if (options.math.vector_mult == QC_DOT) {
		// vector * vector is dot product in v6 progs (ick)
		e->expr.op = QC_DOT;
		e->expr.type = &type_float;
	} else {
		// component-wise multiplication
		e->expr.type = &type_vector;
	}
	return e;
}

static const expr_t *
quaternion_quaternion_expr (int op, const expr_t *a, const expr_t *b)
{
	return typed_binary_expr (&type_quaternion, QC_QMUL, a, b);
}

static const expr_t *
quaternion_vector_expr (int op, const expr_t *a, const expr_t *b)
{
	return typed_binary_expr (&type_vector, QC_QVMUL, a, b);
}

static const expr_t *
vector_quaternion_expr (int op, const expr_t *a, const expr_t *b)
{
	return typed_binary_expr (&type_vector, QC_VQMUL, a, b);
}

static const expr_t *
outer_product_expr (int op, const expr_t *a, const expr_t *b)
{
	auto ta = get_type (a);
	auto tb = get_type (b);
	if (is_integral (ta) || is_integral (tb)) {
		warning (a, "integral vectors in outer product");
		ta = float_type (ta);
		tb = float_type (tb);
		a = cast_expr (ta, a);
		b = cast_expr (tb, b);
	}
	int rows = type_width (ta);
	int cols = type_width (tb);
	auto type = matrix_type (ta, cols, rows);
	auto e = typed_binary_expr (type, QC_OUTER, a, b);
	return e;
}

static const expr_t *
dot_product_expr (int op, const expr_t *a, const expr_t *b)
{
	auto ta = get_type (a);
	auto tb = get_type (b);
	if (is_integral (ta) || is_integral (tb)) {
		warning (a, "integral vectors in dot product");
		ta = float_type (ta);
		tb = float_type (tb);
		a = cast_expr (ta, a);
		b = cast_expr (tb, b);
	}
	auto type = base_type (ta);
	auto e = typed_binary_expr (type, QC_DOT, a, b);
	return e;
}

static const expr_t *
boolean_op (int op, const expr_t *a, const expr_t *b)
{
	if (!is_boolean (get_type (a))) {
		a = test_expr (a);
	}
	if (!is_boolean (get_type (b))) {
		b = test_expr (b);
	}
	promote_exprs (&a, &b);
	auto type = base_type (get_type (a));
	auto e = typed_binary_expr (type, op, a, b);
	return e;
}

static const type_t *
bool_result (const type_t *a, const type_t *b)
{
	return bool_type (a);
}

static bool
shape_matrix (const type_t *a, const type_t *b)
{
	if (type_cols (a) == type_rows (b)) {
		return true;
	}
	error (0, "matrix colums != matrix rows: %d %d",
		   type_cols (a), type_rows (b));
	return false;
}

static bool
shape_matvec (const type_t *a, const type_t *b)
{
	if (type_cols (a) == type_width (b)) {
		return true;
	}
	error (0, "matrix colums != vectors width: %d %d",
		   type_cols (a), type_width (b));
	return false;
}

static bool
shape_vecmat (const type_t *a, const type_t *b)
{
	if (type_width (a) == type_rows (b)) {
		return true;
	}
	error (0, "vectors width != matrix rows: %d %d",
		   type_width (a), type_rows (b));
	return false;
}

static bool
shape_always (const type_t *a, const type_t *b)
{
	return true;
}

static expr_type_t equality_ops[] = {
	{	.match_a = is_string,  .match_b = is_string,
			.res_type = bool_result },
	{	.match_a = is_math,    .match_b = is_math,
			.process = math_compare },
	{	.match_a = is_entity,  .match_b = is_entity,
			.process = entity_compare },
	{	.match_a = is_field,   .match_b = is_field,   },
	{	.match_a = is_func,    .match_b = is_func,
			.process = func_compare },
	{	.match_a = is_pointer, .match_b = is_pointer,
			.process = pointer_compare },
	{	.match_a = is_handle,  .match_b = is_handle,  },

	{}
};

static expr_type_t compare_ops[] = {
	{   .match_a = is_string,  .match_b = is_string,
			.res_type = bool_result },
	{   .match_a = is_math,    .match_b = is_math,
			.process = math_compare },
	{   .match_a = is_pointer, .match_b = is_pointer,
			.process = pointer_compare },

	{}
};

static expr_type_t shift_ops[] = {
	{   .match_a = is_math,    .match_b = is_math,
			.process = target_shift_op },

	{}
};

static expr_type_t add_ops[] = {
	{   .match_a = is_ptr,      .match_b = is_integral,
			.process = pointer_arithmetic, },
	{   .match_a = is_integral, .match_b = is_ptr,
			.process = pointer_arithmetic, },
	{   .match_a = is_string,   .match_b = is_string,   },
	{   .match_a = is_matrix,   .match_b = is_scalar,
			.match_shape = shape_always,
			.process = matrix_scalar_expr, },
	{   .match_a = is_scalar,   .match_b = is_matrix,
			.match_shape = shape_always,
			.process = matrix_scalar_expr, },
	{   .match_a = is_nonscalar,.match_b = is_scalar,
			.match_shape = shape_always,
			.process = vector_scalar_expr, },
	{   .match_a = is_scalar,   .match_b = is_nonscalar,
			.match_shape = shape_always,
			.process = vector_scalar_expr, },
	{   .match_a = is_math,     .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t sub_ops[] = {
	{   .match_a = is_ptr,      .match_b = is_integral,
			.process = pointer_arithmetic, },
	{   .match_a = is_ptr,      .match_b = is_ptr,
			.process = pointer_arithmetic, },
	{   .match_a = is_matrix,   .match_b = is_scalar,
			.match_shape = shape_always,
			.process = matrix_scalar_expr, },
	{   .match_a = is_scalar,   .match_b = is_matrix,
			.match_shape = shape_always,
			.process = matrix_scalar_expr, },
	{   .match_a = is_nonscalar,.match_b = is_scalar,
			.match_shape = shape_always,
			.process = vector_scalar_expr, },
	{   .match_a = is_scalar,   .match_b = is_nonscalar,
			.match_shape = shape_always,
			.process = vector_scalar_expr, },
	{   .match_a = is_math,     .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t mul_ops[] = {
	{   .match_a = is_matrix,     .match_b = is_matrix,
			.match_shape = shape_matrix,
			.process = matrix_binary_expr, },
	{   .match_a = is_matrix,     .match_b = is_nonscalar,
			.match_shape = shape_matvec,
			.process = matrix_binary_expr, },
	{   .match_a = is_nonscalar,  .match_b = is_matrix,
			.match_shape = shape_vecmat,
			.process = matrix_binary_expr, },
	{   .match_a = is_matrix,     .match_b = is_scalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_mul, },
	{   .match_a = is_scalar,     .match_b = is_matrix,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_mul, },
	{   .match_a = is_nonscalar,  .match_b = is_scalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_mul, },
	{   .match_a = is_scalar,     .match_b = is_nonscalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_mul, },
	{	.match_a = is_vector,     .match_b = is_vector,
			.process = vector_vector_mul, },
	{	.match_a = is_vector,     .match_b = is_vector_compat,
			.promote = true, .process = vector_vector_mul, },
	{	.match_a = is_vector_compat, .match_b = is_vector,
			.promote = true, .process = vector_vector_mul, },
	{	.match_a = is_quaternion, .match_b = is_quaternion,
			.process = quaternion_quaternion_expr, },
	{	.match_a = is_quaternion, .match_b = is_quaternion_compat,
			.promote = true, .process = quaternion_quaternion_expr, },
	{	.match_a = is_quaternion_compat, .match_b = is_quaternion,
			.promote = true, .process = quaternion_quaternion_expr, },
	{	.match_a = is_quaternion, .match_b = is_vector_compat,
			.match_shape = shape_always,
			.promote = true, .process = quaternion_vector_expr, },
	{	.match_a = is_vector_compat,     .match_b = is_quaternion,
			.match_shape = shape_always,
			.promote = true, .process = vector_quaternion_expr, },
	{   .match_a = is_math,       .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t outer_ops[] = {
	{   .match_a = is_nonscalar, .match_b = is_nonscalar,
			.process = outer_product_expr, },

	{}
};

static expr_type_t cross_ops[] = {
	{   .match_a = is_vector, .match_b = is_vector, },
	{   .match_a = is_vector, .match_b = is_vector_compat,
			.promote = true },
	{   .match_a = is_vector_compat, .match_b = is_vector,
			.promote = true },

	{}
};

static expr_type_t dot_ops[] = {
	{   .match_a = is_nonscalar, .match_b = is_nonscalar,
			.process = dot_product_expr, },

	{}
};

static expr_type_t div_ops[] = {
	{   .match_a = is_matrix,     .match_b = is_scalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_div, },
	{   .match_a = is_nonscalar,  .match_b = is_scalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_div, },
	{   .match_a = is_math,     .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t mod_ops[] = {
	{   .match_a = is_matrix,   .match_b = is_matrix, },	// invalid op
	{   .match_a = is_nonscalar,  .match_b = is_scalar,
			.match_shape = shape_always,
			.promote = true, .process = matrix_scalar_div, },
	{   .match_a = is_math,     .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t bit_ops[] = {
	{   .match_a = is_math,     .match_b = is_math,
			.promote = true },

	{}
};

static expr_type_t bool_ops[] = {
	{   .match_a = is_boolean, .match_b = is_boolean, },
	{   .match_a = is_scalar, .match_b = is_scalar,
			.process = boolean_op },

	{}
};

static expr_type_t *expr_types[] = {
	[QC_EQ] = equality_ops,
	[QC_NE] = equality_ops,
	[QC_LE] = compare_ops,
	[QC_GE] = compare_ops,
	[QC_LT] = compare_ops,
	[QC_GT] = compare_ops,
	[QC_SHL] = shift_ops,
	[QC_SHR] = shift_ops,
	['+'] = add_ops,
	['-'] = sub_ops,
	['*'] = mul_ops,
	['/'] = div_ops,
	['&'] = bit_ops,
	['|'] = bit_ops,
	['^'] = bit_ops,
	['%'] = mod_ops,
	[QC_AND] = bool_ops,
	[QC_OR] = bool_ops,
	[QC_XOR] = bool_ops,
	[QC_MOD] = mod_ops,
	[QC_GEOMETRIC] = nullptr,	// handled by algebra_binary_expr
	[QC_HADAMARD] = mul_ops,
	[QC_CROSS] = cross_ops,
	[QC_DOT] = dot_ops,
	[QC_OUTER] = outer_ops,
	[QC_WEDGE] = nullptr,	// handled by algebra_binary_expr
	[QC_REGRESSIVE] = nullptr,	// handled by algebra_binary_expr
};

static const expr_t *
reimplement_binary_expr (int op, const expr_t *e1, const expr_t *e2)
{
	if (options.code.progsversion == PROG_ID_VERSION) {
		switch (op) {
			case '%':
				{
					auto div = paren_expr (binary_expr ('/', e1, e2));
					auto trn = binary_expr ('&', div, div);
					return binary_expr ('-', e1, binary_expr ('*', e2, trn));
				}
				break;
			case QC_MOD:
				{
					auto div = paren_expr (binary_expr ('/', e1, e2));
					auto trn = binary_expr ('&', div, div);
					auto one = binary_expr (QC_GT, trn, div);
					auto flr = binary_expr ('-', trn, one);
					return binary_expr ('-', e1, binary_expr ('*', e2, flr));
				}
				break;
		}
	}
	return nullptr;
}

static const expr_t *
check_precedence (int op, const expr_t *e1, const expr_t *e2)
{
	if (e1->type == ex_uexpr && e1->expr.op == '!' && !e1->paren) {
		if (options.traditional) {
			if (op != QC_AND && op != QC_OR && op != '=') {
				notice (e1, "precedence of `!' and `%s' inverted for "
							"traditional code", get_op_string (op));
				e1 = paren_expr (e1->expr.e1);
				return unary_expr ('!', binary_expr (op, e1, e2));
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
				 && (is_math_op (e2->expr.op) || is_compare (e2->expr.op)))
				|| (op == '='
					&&(e2->expr.op == QC_OR || e2->expr.op == QC_AND))) {
				notice (e1, "precedence of `%s' and `%s' inverted for "
							"traditional code", get_op_string (op),
							get_op_string (e2->expr.op));
				e1 = binary_expr (op, e1, e2->expr.e1);
				e1 = paren_expr (e1);
				return binary_expr (e2->expr.op, e1, e2->expr.e2);
			}
			if (((op == QC_EQ || op == QC_NE) && is_compare (e2->expr.op))
				|| (op == QC_OR && e2->expr.op == QC_AND)
				|| (op == '|' && e2->expr.op == '&')) {
				notice (e1, "precedence of `%s' raised to `%s' for "
							"traditional code", get_op_string (op),
							get_op_string (e2->expr.op));
				e1 = binary_expr (op, e1, e2->expr.e1);
				e1 = paren_expr (e1);
				return binary_expr (e2->expr.op, e1, e2->expr.e2);
			}
		} else if (e1->type == ex_expr && !e1->paren) {
			if (((op == '&' || op == '|')
				 && (is_math_op (e1->expr.op) || is_compare (e1->expr.op)))
				|| (op == '='
					&&(e2->expr.op == QC_OR || e2->expr.op == QC_AND))) {
				notice (e1, "precedence of `%s' and `%s' inverted for "
							"traditional code", get_op_string (op),
							get_op_string (e1->expr.op));
				e2 = binary_expr (op, e1->expr.e2, e2);
				e1 = paren_expr (e1->expr.e1);
				return binary_expr (e1->expr.op, e1, e2);
			}
		}
	} else {
		if (e2->type == ex_expr && !e2->paren) {
			if ((op == '&' || op == '|' || op == '^')
				&& (is_math_op (e2->expr.op)
					|| is_compare (e2->expr.op))) {
				if (options.warnings.precedence)
					warning (e2, "suggest parentheses around %s in "
							 "operand of %c",
							 is_compare (e2->expr.op)
									? "comparison"
									: get_op_string (e2->expr.op),
							 op);
			}
		}
		if (e1->type == ex_expr && !e1->paren) {
			if ((op == '&' || op == '|' || op == '^')
				&& (is_math_op (e1->expr.op)
					|| is_compare (e1->expr.op))) {
				if (options.warnings.precedence)
					warning (e1, "suggest parentheses around %s in "
							 "operand of %c",
							 is_compare (e1->expr.op)
									? "comparison"
									: get_op_string (e1->expr.op),
							 op);
			}
		}
	}
	return 0;
}

static int
is_call (const expr_t *e)
{
	return e->type == ex_block && e->block.is_call;
}

const expr_t *
binary_expr (int op, const expr_t *e1, const expr_t *e2)
{
	// FIXME this is target-specific info and should not be in the
	// expression tree
	if (e1->type == ex_alias && is_call (e1->alias.expr)) {
		// move the alias expression inside the block so the following check
		// can detect the call and move the temp assignment into the block
		auto block = (expr_t *) e1->alias.expr;
		auto ne = new_expr ();
		*ne = *e1;
		ne->alias.expr = block->block.result;
		block->block.result = ne;
		e1 = block;
	}
	if (e1->type == ex_block && e1->block.is_call
		&& has_function_call (e2) && e1->block.result) {
		// the temp assignment needs to be inside the block so assignment
		// code generation doesn't see it when applying right-associativity
		expr_t    *tmp = new_temp_def_expr (get_type (e1->block.result));
		auto ne = assign_expr (tmp, e1->block.result);
		auto nb = new_block_expr (e1);
		append_expr (nb, ne);
		nb->block.result = tmp;
		e1 = nb;
	}
	if (e1->type == ex_error)
		return e1;

	if (e2->type == ex_error)
		return e2;

	if (e1->type == ex_bool)
		e1 = convert_from_bool (e1, get_type (e2));
	if (e2->type == ex_bool)
		e2 = convert_from_bool (e2, get_type (e1));

	const expr_t *e;
	if ((e = check_precedence (op, e1, e2)))
		return e;

	if (is_reference (get_type (e1))) {
		e1 = pointer_deref (e1);
	}
	if (is_reference (get_type (e2))) {
		e2 = pointer_deref (e2);
	}

	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	if (!t1 || !t2)
		internal_error (e1, "expr with no type");

	if (is_algebra (t1) || is_algebra (t2)) {
		return algebra_binary_expr (op, e1, e2);
	}

	if (op == QC_EQ || op == QC_NE) {
		if (e1->type == ex_nil) {
			t1 = t2;
			e1 = convert_nil (e1, t1);
		} else if (e2->type == ex_nil) {
			t2 = t1;
			e2 = convert_nil (e2, t2);
		}
	}

	if (is_array (t1) && (is_pointer (t2) || is_integral (t2))) {
		t1 = pointer_type (dereference_type (t1));
		e1 = cast_expr (t1, e1);
	}
	if (is_array (t2) && (is_pointer (t1) || is_integral (t1))) {
		t1 = pointer_type (dereference_type (t2));
		e2 = cast_expr (t2, e2);
	}

	if ((unsigned) op > countof (expr_types) || !expr_types[op]) {
		internal_error (e1, "invalid operator: %s", get_op_string (op));
	}
	expr_type_t *expr_type = expr_types[op];
	for (; expr_type->match_a; expr_type++) {
		if (expr_type->match_a (t1) && expr_type->match_b (t2)) {
			break;
		}
	}
	if (!expr_type->match_a) {
		return error (e1, "invalid binary expression");
	}
	if (expr_type->match_shape) {
		scoped_src_loc (e1);//for error messages
		if (!expr_type->match_shape (t1, t2)) {
			return new_error_expr ();
		}
	} else {
		if (type_width (t1) != type_width (t2)
			|| type_cols(t1) != type_cols(t2)) {
			return error (e1, "operand size mismatch in binary expression");
		}
	}

	if (expr_type->promote) {
		if (e1->implicit && type_demotes (t2, t1)) {
			t1 = promote_type (t2, t1);
			e1 = cast_expr (t1, e1);
		} else if (e2->implicit && type_demotes (t1, t2)) {
			t2 = promote_type (t1, t2);
			e2 = cast_expr (t2, e2);
		} else {
			promote_exprs (&e1, &e2);
			t1 = get_type (e1);
			t2 = get_type (e2);
		}
	}

	if (expr_type->process) {
		auto e = expr_type->process (op, e1, e2);
		return edag_add_expr (fold_constants (e));
	}

	auto type = t1;
	if (expr_type->res_type) {
		type = expr_type->res_type (t1, t2);
		if (!type) {
			return new_error_expr ();
		}
	}

	if ((e = reimplement_binary_expr (op, e1, e2)))
		return edag_add_expr (fold_constants (e));

	if (expr_type->true_op) {
		op = expr_type->true_op;
	}

	auto ne = new_binary_expr (op, e1, e2);
	ne->expr.type = type;
	if (expr_type->commutative) {
		ne->expr.commutative = expr_type->commutative ();
	}
	if (expr_type->anticommute) {
		ne->expr.anticommute = expr_type->anticommute ();
	}
	if (expr_type->associative) {
		ne->expr.associative = expr_type->associative ();
	}
	return edag_add_expr (fold_constants (ne));
}
