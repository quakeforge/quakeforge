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
		e = array_expr (e, ind);
	}
	return e;
}

static const expr_t *
math_constructor (const type_t *type, const expr_t *params, const expr_t *e)
{
	auto base = base_type (type);
	int num_comp = type->width;
	const expr_t *components[num_comp] = {};

	int num_param = list_count (&params->list);
	const expr_t *param_exprs[num_param + 1] = {};
	list_scatter_rev (&params->list, param_exprs);
	bool all_constant = true;
	bool all_implicit = true;

	int p = 0;
	for (int c = 0; c < num_comp; ) {
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
				components[c++] = error (pexpr, "invalid type for conversion");
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
			components[c++] = new_nil_expr ();
		}
	}
	if (p < num_param) {
		return error (e, "too may parameters for %s", type->name);
	}
	if (num_comp == 1) {
		return components[0];
	}
	if (all_constant) {
		return new_vector_value (base, num_comp, num_comp, components,
								 all_implicit);
	}

	auto vec = new_expr ();
	vec->type = ex_vector;
	vec->vector.type = vector_type (base, num_comp);
	list_gather (&vec->vector.list, components, num_comp);
	return vec;
}

const expr_t *
constructor_expr (const expr_t *e, const expr_t *params)
{
	auto type = e->symbol->type;
	if (is_algebra (type)) {
		return error (e, "algebra not implemented");
	}
	if (is_matrix (type)) {
		return error (e, "matrix not implemented");
	}
	if (is_math (type)) {
		return math_constructor (type, params, e);
	}
	return error (e, "not implemented");
}
