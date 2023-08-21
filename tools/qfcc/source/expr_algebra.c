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

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

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
	if (vector->type == ex_vector) {
		internal_error (vector, "multiproduce scale not implemented");
		auto comp = vector->e.vector.list;
		auto prod = scalar_product (scalar, comp);
		while (comp->next) {
			comp = comp->next;
			auto p = scalar_product (scalar, comp);
			p->next = prod;
			prod = p;
		}
		return fold_constants (prod);
	} else {
		auto vector_type = get_type (vector);
		auto scalar_type = base_type (vector_type);
		scalar = promote_scalar (scalar_type, scalar);
		if (type_width (vector_type) > 4) {
			internal_error (vector, "scalar * %d-vector not implemented",
							type_width (vector_type));
		}
		auto prod = new_binary_expr (SCALE, vector, scalar);
		prod->e.expr.type = vector_type;
		return fold_constants (prod);
	}
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
component_sum (int op, expr_t **components, expr_t *e,
			   const basis_layout_t *layout)
{
	int  group;
	auto t = get_type (e);
	if (is_scalar (t)) {
		group = layout->group_map[layout->mask_map[0]][0];
	} else {
		group = t->t.multivec->element;
	}
	if (components[group]) {
		if (t != get_type (components[group])) {
			internal_error (e, "tangled multivec types");
		}
		components[group] = new_binary_expr (op, components[group], e);
		components[group]->e.expr.type = t;
	} else {
		components[group] = scalar_product (new_float_expr (-1), e);
	}
	components[group] = fold_constants (components[group]);
}

static expr_t *
multivector_sum (int op, expr_t *e1, expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto alg = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &alg->layout;
	expr_t *components[layout->count] = {};
	if (e1->type == ex_multivec) {
		for (auto c = e1->e.multivec.components; c; c = c->next) {
			auto ct = get_type (c);
			int  group;
			if (is_scalar (ct)) {
				group = layout->group_map[layout->mask_map[0]][0];
			} else {
				group = ct->t.multivec->element;
			}
			components[group] = c;
		}
	} else {
		int  group;
		if (is_scalar (t1)) {
			group = layout->group_map[layout->mask_map[0]][0];
		} else {
			group = t1->t.multivec->element;
		}
		components[group] = e1;
	}
	if (e2->type == ex_multivec) {
		for (auto c = e1->e.multivec.components; c; c = c->next) {
			component_sum (op, components, c, layout);
		}
	} else {
		component_sum (op, components, e2, layout);
	}
	int count = 0;
	expr_t *sum = 0;
	for (int i = 0; i < layout->count; i++) {
		if (components[i]) {
			count++;
			sum = components[i];
		}
	}
	if (count == 1) {
		return sum;
	}

	sum = new_expr ();
	sum->type = ex_multivec;
	sum->e.multivec.algebra = alg;
	for (int i = layout->count; i-- > 0; ) {
		if (components[i]) {
			components[i]->next = sum->e.multivec.components;
			sum->e.multivec.components = components[i];
			sum->e.multivec.count++;
		}
	}

	return sum;
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

	if (type_size (srcType) == type_size (dstType)) {
		return new_assign_expr (dst, src);
	}

	if (dstType->meta != ty_algebra && dstType->type != ev_invalid) {
		return 0;
	}
	auto layout = &dstType->t.algebra->layout;
	expr_t *components[layout->count] = {};
	if (src->type == ex_multivec) {
		for (auto c = src->e.multivec.components; c; c = c->next) {
			auto ct = get_type (c);
			int  group;
			if (is_scalar (ct)) {
				group = layout->group_map[layout->mask_map[0]][0];
			} else {
				group = ct->t.multivec->element;
			}
			components[group] = c;
		}
	} else {
		int  group;
		if (is_scalar (srcType)) {
			group = layout->group_map[layout->mask_map[0]][0];
		} else {
			group = srcType->t.multivec->element;
		}
		components[group] = src;
	}
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
			auto dst_type = layout->groups[i].type;
			auto dst_alias = new_offset_alias_expr (dst_type, dst, offset);
			append_expr (block, new_assign_expr (dst_alias, components[i]));
			offset += type_size (dst_type);
			memset_base = offset;
		} else {
			offset += type_size (layout->groups[i].type);
			memset_size += type_size (layout->groups[i].type);
		}
	}
	if (memset_size) {
		zero_components (block, dst, memset_base, memset_size);
	}
	return block;
}
