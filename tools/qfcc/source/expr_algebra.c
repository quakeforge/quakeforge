/*
	expr_algebra.c

	goemetric algebra expressions

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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

#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

static expr_t *
mvec_expr (expr_t *expr, algebra_t *algebra)
{
	auto mvtype = get_type (expr);
	if (expr->type == ex_multivec || is_scalar (mvtype)) {
		return expr;
	}
	if (!is_algebra (mvtype)) {
		return error (expr, "invalid operand for GA");
	}

	auto layout = &algebra->layout;
	pr_uint_t group_mask = (1u << (layout->count + 1)) - 1;
	if (mvtype->type != ev_invalid) {
		group_mask = mvtype->t.multivec->group_mask;
	}
	if (!(group_mask & (group_mask - 1))) {
		return expr;
	}
	auto mvec = new_expr ();
	mvec->type = ex_multivec;
	mvec->e.multivec = (ex_multivec_t) {
		.type = algebra_mvec_type (algebra, group_mask),
		.algebra = algebra,
	};
	expr_t **c = &mvec->e.multivec.components;
	int comp_offset = 0;
	for (int i = 0; i < layout->count; i++) {
		pr_uint_t   mask = 1u << i;
		if (mask & group_mask) {
			auto comp_type = algebra_mvec_type (algebra, mask);
			*c = new_offset_alias_expr (comp_type, expr, comp_offset);
			c = &(*c)->next;
			mvec->e.multivec.count++;
			comp_offset += algebra->layout.groups[i].count;
		}
	}

	return mvec;
}

static void
mvec_scatter (expr_t **components, expr_t *mvec, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	int  group;

	if (mvec->type != ex_multivec) {
		auto type = get_type (mvec);
		if (!is_algebra (type)) {
			group = layout->group_map[layout->mask_map[0]][0];
		} else {
			if (type->type == ev_invalid) {
				internal_error (mvec, "full algebra in mvec_scatter");
			}
			pr_uint_t mask = type->t.multivec->group_mask;
			if (mask & (mask - 1)) {
				internal_error (mvec, "bare multivector in mvec_scatter");
			}
			group = BITOP_LOG2 (mask);
		}
		components[group] = mvec;
		return;
	}
	for (auto c = mvec->e.multivec.components; c; c = c->next) {
		auto ct = get_type (c);
		if (is_scalar (ct)) {
			group = layout->group_map[layout->mask_map[0]][0];
			components[group] = mvec;
		} else if (ct->meta == ty_algebra && ct->type != ev_invalid) {
			pr_uint_t mask = ct->t.multivec->group_mask;
			if (mask & (mask - 1)) {
				internal_error (mvec, "multivector in multivec expression");
			}
			group = BITOP_LOG2 (mask);
		} else {
			internal_error (mvec, "invalid type in multivec expression");
		}
		components[group] = c;
	}
}

static expr_t *
mvec_gather (expr_t **components, algebra_t *algebra)
{
	auto layout = &algebra->layout;

	pr_uint_t group_mask = 0;
	int count = 0;
	expr_t *mvec = 0;
	for (int i = 0; i < layout->count; i++) {
		if (components[i]) {
			count++;
			mvec = components[i];
			group_mask |= 1 << i;
		}
	}
	if (count == 1) {
		return mvec;
	}

	mvec = new_expr ();
	mvec->type = ex_multivec;
	mvec->e.multivec = (ex_multivec_t) {
		.type = algebra_mvec_type (algebra, group_mask),
		.algebra = algebra,
	};
	for (int i = layout->count; i-- > 0; ) {
		if (components[i]) {
			components[i]->next = mvec->e.multivec.components;
			mvec->e.multivec.components = components[i];
			mvec->e.multivec.count++;
		}
	}
	return mvec;
}

static expr_t *
promote_scalar (type_t *dst_type, expr_t *scalar)
{
	auto scalar_type = get_type (scalar);
	if (scalar_type != dst_type) {
		if (!type_promotes (dst_type, scalar_type)) {
			warning (scalar, "demoting %s to %s (use a cast)",
					 get_type_string (scalar_type),
					 get_type_string (dst_type));
		}
		scalar = cast_expr (dst_type, scalar);
	}
	return scalar;
}

static expr_t *
scalar_product (expr_t *e1, expr_t *e2)
{
	auto scalar = is_scalar (get_type (e1)) ? e1 : e2;
	auto vector = is_scalar (get_type (e1)) ? e2 : e1;
	auto algebra = algebra_get (get_type (vector));
	auto layout = &algebra->layout;

	scalar = promote_scalar (algebra->type, scalar);

	expr_t *components[layout->count] = {};
	vector = mvec_expr (vector, algebra);
	mvec_scatter (components, vector, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!components[i]) {
			continue;
		}
		auto comp_type = get_type (components[i]);
		if (type_width (comp_type) == 1) {
			auto prod = new_binary_expr ('*', components[i], scalar);
			prod->e.expr.type = comp_type;
			components[i] = fold_constants (prod);
		} else if (type_width (comp_type) > 4) {
			internal_error (vector, "scalar * %d-vector not implemented",
							type_width (comp_type));
		} else {
			auto prod = new_binary_expr (SCALE, components[i], scalar);
			prod->e.expr.type = comp_type;
			components[i] = fold_constants (prod);
		}
	}
	return mvec_gather (components, algebra);
}

static expr_t *
inner_product (expr_t *e1, expr_t *e2)
{
	if (is_scalar (get_type (e1)) || is_scalar (get_type (e2))) {
		auto scalar = is_scalar (get_type (e1)) ? e1 : e2;
		notice (scalar,
				"the inner product of a scalar with any other grade is 0");
		pr_type_t zero[type_size (get_type (scalar))] = {};
		return new_value_expr (new_type_value (get_type (scalar), zero));
	}
	internal_error (e1, "not implemented");
}

static expr_t *
outer_product (expr_t *e1, expr_t *e2)
{
	if (is_scalar (get_type (e1)) || is_scalar (get_type (e2))) {
		return scalar_product (e1, e2);
	}
	internal_error (e1, "not implemented");
}

static expr_t *
regressive_product (expr_t *e1, expr_t *e2)
{
	internal_error (e1, "not implemented");
}

static void
component_sum (int op, expr_t **c, expr_t **a, expr_t **b,
			   algebra_t *algebra)
{
	auto layout = &algebra->layout;
	for (int i = 0; i < layout->count; i++) {
		if (a[i] && b[i]) {
			if (get_type (a[i]) != get_type (b[i])) {
				internal_error (a[i], "tangled multivec types");
			}
			c[i] = new_binary_expr (op, a[i], b[i]);
			c[i]->e.expr.type = get_type (a[i]);
			c[i] = fold_constants (c[i]);
		} else if (a[i]) {
			c[i] = a[i];
		} else if (b[i]) {
			if (op == '+') {
				c[i] = b[i];
			} else {
				c[i] = scalar_product (new_float_expr (-1), b[i]);
				c[i] = fold_constants (c[i]);
			}
		} else {
			c[i] = 0;
		}
	}
}

static expr_t *
multivector_sum (int op, expr_t *e1, expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	expr_t *a[layout->count] = {};
	expr_t *b[layout->count] = {};
	expr_t *c[layout->count];
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);
	component_sum (op, c, a, b, algebra);
	return mvec_gather (c, algebra);
}

static expr_t *
geometric_product (expr_t *e1, expr_t *e2)
{
	if (is_scalar (get_type (e1)) || is_scalar (get_type (e2))) {
		return scalar_product (e1, e2);
	}
	internal_error (e1, "not implemented");
}

static expr_t *
commutator_product (expr_t *e1, expr_t *e2)
{
	auto ab = geometric_product (e1, e2);
	auto ba = geometric_product (e2, e1);
	return multivector_sum ('-', ab, ba);
}

static expr_t *
multivector_divide (expr_t *e1, expr_t *e2)
{
	internal_error (e1, "not implemented");
}

expr_t *
algebra_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	switch (op) {
		case DOT:
			return inner_product (e1, e2);
		case WEDGE:
			return outer_product (e1, e2);
		case REGRESSIVE:
			return regressive_product (e1, e2);
		case CROSS:
			return commutator_product (e1, e2);
		case '+':
		case '-':
			return multivector_sum (op, e1, e2);
		case '/':
			return multivector_divide (e1, e2);
		case '*':
		case GEOMETRIC:
			return geometric_product (e1, e2);
	}
	return error (e1, "invalid operator");
}

expr_t *
algebra_negate (expr_t *e)
{
	if (e) {
		internal_error (e, "not implemented");
	}
	notice (e, "not implemented");
	return 0;
}

expr_t *
algebra_dual (expr_t *e)
{
	if (e) {
		internal_error (e, "not implemented");
	}
	notice (e, "not implemented");
	return 0;
}

expr_t *
algebra_reverse (expr_t *e)
{
	if (e) {
		internal_error (e, "not implemented");
	}
	notice (e, "not implemented");
	return 0;
}

expr_t *
algebra_cast_expr (type_t *dstType, expr_t *e)
{
	type_t *srcType = get_type (e);
	if (dstType->type == ev_invalid
		|| srcType->type == ev_invalid
		|| type_size (dstType) != type_size (srcType)
		|| type_width (dstType) != type_width (srcType)) {
		return cast_error (e, srcType, dstType);
	}
	return new_alias_expr (dstType, e);
}

static void
zero_components (expr_t *block, expr_t *dst, int memset_base, int memset_size)
{
	auto base = new_offset_alias_expr (&type_int, dst, memset_base);
	auto zero = new_int_expr (0);
	auto size = new_int_expr (memset_size);
	append_expr (block, new_memset_expr (base, zero, size));
}

expr_t *
algebra_assign_expr (expr_t *dst, expr_t *src)
{
	type_t *srcType = get_type (src);
	type_t *dstType = get_type (dst);

	if (src->type != ex_multivec) {
		if (type_size (srcType) == type_size (dstType)) {
			return new_assign_expr (dst, src);
		}
	}

	if (dstType->meta != ty_algebra && dstType != srcType) {
		return 0;
	}
	auto algebra = algebra_get (dstType);
	auto layout = &algebra->layout;
	expr_t *components[layout->count] = {};
	src = mvec_expr (src, algebra);
	mvec_scatter (components, src, algebra);

	auto block = new_block_expr ();
	int  memset_base = 0;
	int  memset_size = 0;
	int  offset = 0;
	for (int i = 0; i < layout->count; i++) {
		if (components[i]) {
			if (memset_size) {
				zero_components (block, dst, memset_base, memset_size);
				memset_size = 0;
			}
			auto dst_type = algebra_mvec_type (algebra, 1 << i);
			auto dst_alias = new_offset_alias_expr (dst_type, dst, offset);
			append_expr (block, new_assign_expr (dst_alias, components[i]));
			offset += type_size (dst_type);
			memset_base = offset;
		} else {
			if (dstType->type == ev_invalid) {
				auto dst_type = algebra_mvec_type (algebra, 1 << i);
				offset += type_size (dst_type);
				memset_size += type_size (dst_type);
			}
		}
	}
	if (memset_size) {
		zero_components (block, dst, memset_base, memset_size);
	}
	return block;
}
