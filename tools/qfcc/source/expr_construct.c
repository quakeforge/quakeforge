/*
	expr_construct.c

	type constructor expressions

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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
#include <string.h>

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static const expr_t *
get_value (const expr_t *e, int i, int j)
{
	auto t = get_type (e);

	if (i < 0 || i >= type_cols (t) || j < 0 || j >= type_rows (t)) {
		internal_error (e, "invalid index");
	}
	if (type_cols (t) > 1) {
		auto ind = new_int_expr (i, false);
		e = array_expr (e, ind);
	}
	if (type_rows (t) > 1) {
		auto ind = new_int_expr (j, false);
		auto a = new_array_expr (e, ind);
		a->array.type = base_type (t);
		e = a;
	}
	return e;
}

static const expr_t *
construct_by_components (const type_t *type, const expr_t *params,
						 const expr_t *e)
{
	auto base = base_type (type);
	int num_comp = type_rows (type) * type_cols (type);
	const expr_t *components[num_comp] = {};

	int num_param = list_count (&params->list);
	const expr_t *param_exprs[num_param + 1] = {};
	list_scatter_rev (&params->list, param_exprs);

	bool all_constant = true;
	bool all_implicit = true;

	int c = 0, p = 0;
	int err = -1;
	while (c < num_comp) {
		if (p < num_param) {
			auto pexpr = param_exprs[p++];
			auto ptype = get_type (pexpr);
			if (!ptype) {
				continue;
			}
			if (is_reference (ptype)) {
				pexpr = pointer_deref (pexpr);
				ptype = dereference_type (ptype);
			}
			if (!is_math (ptype)) {
				err = c++;
				components[err] = error (pexpr, "invalid type for conversion");
				continue;
			}
			for (int i = 0; i < type_cols (ptype) && c < num_comp; i++) {
				for (int j = 0; j < type_rows (ptype) && c < num_comp; j++) {
					auto val = get_value (pexpr, i, j);
					all_implicit = all_implicit && val->implicit;
					all_constant = all_constant && is_constant (val);
					components[c++] = cast_expr (base, val);
				}
			}
		} else {
			break;
		}
	}
	if (err >= 0) {
		return components[err];
	}
	if (c < num_comp) {
		return error (e, "too few parameters for %s", type->name);
	}
	if (p < num_param) {
		return error (e, "too may parameters for %s", type->name);
	}
	if (num_comp == 1) {
		return components[0];
	}
	if (all_constant) {
		if (is_matrix (type)) {
			return new_matrix_value (base, type_cols (type), type_rows (type),
									 num_comp, components, all_implicit);
		} else {
			return new_vector_value (base, type_width (type),
									 num_comp, components, all_implicit);
		}
	}

	auto vec = new_expr ();
	vec->type = ex_vector;
	vec->vector.type = vector_type (base, num_comp);
	list_gather (&vec->vector.list, components, num_comp);
	return vec;
}

static const expr_t *
construct_diagonal (const type_t *type, const expr_t *scalar, const expr_t *e)
{
	scoped_src_loc (scalar);
	int cols = type_cols (type);
	int rows = type_rows (type);
	const expr_t *components[cols * rows + 1] = {};
	auto zero = new_nil_expr ();

	for (int i = 0; i < cols; i++) {
		for (int j = 0; j < rows; j++) {
			components[i * rows + j] = i == j ? scalar : zero;
		}
	}
	auto params = new_list_expr (nullptr);
	list_gather (&params->list, components, cols * rows);
	return construct_by_components (type, params, e);
}

static const expr_t *
construct_matrix (const type_t *type, const expr_t *matrix, const expr_t *e)
{
	scoped_src_loc (matrix);
	int cols = type_cols (type);
	int rows = type_rows (type);
	int src_cols = type_cols (get_type (matrix));
	int src_rows = type_rows (get_type (matrix));
	const expr_t *components[cols * rows + 1] = {};
	auto zero = new_nil_expr ();

	for (int i = 0; i < cols; i++) {
		for (int j = 0; j < rows; j++) {
			const expr_t *val;
			if (i < src_cols && j < src_rows) {
				val = get_value (matrix, i, j);
			} else {
				val = zero;
			}
			components[i * rows + j] = val;
		}
	}
	auto params = new_list_expr (nullptr);
	list_gather (&params->list, components, cols * rows);
	return construct_by_components (type, params, e);
}

static const expr_t *
construct_broadcast (const type_t *type, const expr_t *scalar, const expr_t *e)
{
	scoped_src_loc (scalar);
	int width = type_width (type);
	const expr_t *components[width + 1] = {};

	for (int i = 0; i < width; i++) {
		components[i] = scalar;
	}
	auto params = new_list_expr (nullptr);
	list_gather (&params->list, components, width);
	return construct_by_components (type, params, e);
}

static const expr_t *
math_constructor (const type_t *type, const expr_t *params, const expr_t *e)
{
	int num_param = list_count (&params->list);
	const expr_t *param_exprs[num_param + 1] = {};
	list_scatter_rev (&params->list, param_exprs);

	if (num_param == 1 && is_scalar (get_type (param_exprs[0]))) {
		if (is_matrix (type)) {
			return construct_diagonal (type, param_exprs[0], e);
		}
		if (is_vector (type)) {
			return construct_broadcast (type, param_exprs[0], e);
		}
	}
	if (num_param == 1 && is_matrix (get_type (param_exprs[0]))) {
		if (is_matrix (type)) {
			return construct_matrix (type, param_exprs[0], e);
		}
	}
	return construct_by_components (type, params, e);
}

const expr_t *
constructor_expr (const expr_t *e, const expr_t *params)
{
	auto type = e->symbol->type;
	if (is_algebra (type)) {
		return error (e, "algebra not implemented");
	}
	if (is_math (type)) {
		return math_constructor (type, params, e);
	}
	return error (e, "not implemented");
}
