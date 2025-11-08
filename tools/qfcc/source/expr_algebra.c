/*
	expr_algebra.c

	geometric algebra expressions

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
#include <string.h>

#include "QF/fbsearch.h"
#include "QF/heapsort.h"
#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

pr_uint_t
get_group_mask (const type_t *type, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	if (!is_algebra (type)) {
		int group = layout->group_map[layout->mask_map[0]].num;
		return 1u << group;
	} else {
		type = unalias_type (type);
		if (type->type == ev_invalid) {
			return (1 << algebra->layout.count) - 1;
		}
		return type->multivec->group_mask;
	}
}

bool
is_neg (const expr_t *e)
{
	return e->type == ex_uexpr && e->expr.op == '-';
}

bool
is_anticommute (const expr_t *e)
{
	return e && e->type == ex_expr && e->expr.anticommute;
}

bool
is_commutative (const expr_t *e)
{
	return e && e->type == ex_expr && e->expr.commutative;
}

bool
is_associative (const expr_t *e)
{
	return e && e->type == ex_expr && e->expr.associative;
}

static bool __attribute__((const))
op_commute (int op, const type_t *l, const type_t *r)
{
	if ((is_matrix (l) && !is_scalar (r))
		|| (!is_scalar (l) && is_matrix (r))) {
		return false;
	}
	return (op == '+' || op == '*' || op == QC_HADAMARD || op == QC_DOT);
}

static bool __attribute__((const))
op_associate (int op, const type_t *l, const type_t *r)
{
	return ((op == '+' && options.code.assoc_float_add)
			|| op == '*' || op == QC_HADAMARD || op == QC_DOT);
}

static bool __attribute__((const))
op_anti_com (int op, const type_t *l, const type_t *r)
{
	return (op == '-' || op == QC_CROSS || op == QC_WEDGE);
}

const expr_t *
typed_binary_expr (const type_t *type, int op, const expr_t *e1, const expr_t *e2)
{
	auto l = get_type (e1);
	auto r = get_type (e2);
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = type;
	e->expr.commutative = is_math (type) && op_commute (op, l, r);
	e->expr.anticommute = is_math (type) && op_anti_com (op, l, r);
	e->expr.associative = is_math (type) && op_associate (op, l, r);
	return e;
}

const expr_t *
typed_unary_expr (const type_t *type, int op, const expr_t *e)
{
	auto u = new_unary_expr (op, e);
	u->expr.type = type;
	return u;
}

const expr_t *
neg_expr (const expr_t *e)
{
	if (!e) {
		// propagated zero
		return 0;
	}
	if (is_neg (e)) {
		return e->expr.e1;
	}
	if (is_scale (e) && is_constant (e->expr.e2)
		&& expr_floating (e->expr.e2) == -1) {
		return e->expr.e1;
	}
	auto type = get_type (e);
	if (e->type == ex_alias
		&& (!e->alias.offset || !expr_integral (e->alias.offset))
		&& is_anticommute (e->alias.expr)) {
		auto n = neg_expr (e->alias.expr);
		return n;
	}
	expr_t *neg;
	if (is_anticommute (e)) {
		neg = new_binary_expr (e->expr.op, e->expr.e2, e->expr.e1);
		neg->expr.commutative = e->expr.commutative;
		neg->expr.anticommute = e->expr.anticommute;
		neg->expr.associative = e->expr.associative;
	} else {
		if (e->type == ex_alias && is_algebra (type)) {
			int offset = 0;
			if (e->alias.offset) {
				offset = expr_integral (e->alias.offset);
			}
			auto ft = float_type (type);
			if (ft == get_type (e->alias.expr)) {
				e = e->alias.expr;
			} else {
				e = new_offset_alias_expr (ft, e->alias.expr, offset);
			}
			e = edag_add_expr (e);
		}
		neg = new_unary_expr ('-', e);
	}
	neg->expr.type = type;
	return edag_add_expr (fold_constants (neg));
}

const expr_t *
ext_expr (const expr_t *src, const type_t *type, int extend, bool reverse)
{
	if (!src) {
		return nullptr;
	}
	bool neg = false;
	if (is_neg (src)) {
		neg = true;
		src = neg_expr (src);
	}
	auto ext = edag_add_expr (new_extend_expr (src, type, extend, reverse));
	if (neg) {
		return neg_expr (ext);
	}
	return ext;
}

bool __attribute__((const))
ext_compat (const ex_extend_t *a, const ex_extend_t *b)
{
	return (a->extend == b->extend && a->reverse == b->reverse
			&& a->type == b->type);
}

const expr_t *
ext_swizzle (const expr_t *ext, const expr_t *swizzle)
{
	int dst_width = type_width (ext->extend.type);
	int src_width = type_width (get_type (ext->extend.src));
	unsigned nonzero = (1 << dst_width) - 1;
	unsigned extmask = (1 << src_width) - 1;
	if (ext->extend.reverse) {
		extmask <<= dst_width - src_width;
	}
	if (ext->extend.extend == 0) {
		nonzero &= extmask;
	}

	int swiz_width = type_width (swizzle->swizzle.type);
	unsigned swizmask = 0;
	bool ordered = true;
	for (int i = 0; i < swiz_width; i++) {
		auto comps = swizzle->swizzle.source;
		swizmask |= 1 << comps[i];
		if (i && comps[i] != comps[i - 1] + 1) {
			ordered = false;
		}
	}
	if (!(swizmask & nonzero)) {
		// extracted components are guaranteed to be 0
		return nullptr;
	}
	if (swizmask & ~extmask) {
		// swizzle crosses extend boundary
		return swizzle;
	}
	auto src = ext->extend.src;
	if (swiz_width == src_width && ordered) {
		// trivial swizzle
		return src;
	}
	auto new = new_expr_copy (swizzle);
	new->swizzle.src = src;
	return edag_add_expr (new);
}

bool __attribute__((const))
is_ext (const expr_t *e)
{
	return e && e->type == ex_extend;
}

static bool __attribute__((const))
is_zero (const expr_t *e)
{
	if (e->type != ex_value) {
		return false;
	}
	auto type = get_type (e);
	pr_type_t val[type_size (type)];
	value_store (val, type, e);
	for (int i = 0; i < type_size (type); i++) {
		if (val[i].float_value) {
			return false;
		}
	}
	return true;
}

const expr_t *
offset_cast (const type_t *type, const expr_t *expr, int offset)
{
	if (type->meta != ty_basic) {
		internal_error (expr, "offset cast to non-basic type");
	}
	if (!offset && type_same (type, get_type (expr))) {
		return expr;
	}
	if (expr->type == ex_expr
		&& (expr->expr.op == '+' || expr->expr.op == '-')) {
		auto e1 = offset_cast (type, expr->expr.e1, offset);
		auto e2 = offset_cast (type, expr->expr.e2, offset);
		int  op = expr->expr.op;
		if (!e1) {
			if (op == '-') {
				e2 = neg_expr (e2);
			}
			return e2;
		}
		if (!e2) {
			return e1;
		}
		auto cast = typed_binary_expr (type, op, e1, e2);
		return edag_add_expr (cast);
	}
	if (is_ext (expr)) {
		auto ext = expr->extend;
		if (type_width (get_type (ext.src)) == type_width (type)) {
			return offset_cast (type, ext.src, 0);
		}
		bool rev = ext.reverse;
		int  cwidth = type_width (type);
		int  dwidth = type_width (ext.type);
		int  swidth = type_width (get_type (ext.src));

		if ((!rev && offset >= swidth)
			|| (rev && offset + cwidth <= dwidth - swidth)) {
			return nullptr;
		}
	}
	if (expr->type == ex_vector) {
		int count = list_count (&expr->vector.list);
		const expr_t *c[count + 1] = {};
		list_scatter (&expr->vector.list, c);

		int width = type_width (type);
		int offs = offset;
		for (int i = 0; i < count && offs >= 0; i++) {
			int cwidth = type_width (get_type (c[i]));
			if (!offs && cwidth == width) {
				return c[i];
			}
			offs -= cwidth;
		}
		notice (expr, "unhandled vector extraction");
	}
	if (is_neg (expr)) {
		auto e = expr->expr.e1;
		return neg_expr (offset_cast (type, e, offset));
	}

	char swizzle_str[] = "xyzw";
	swizzle_str[offset + type_width (type)] = 0;
	auto swizzle = new_swizzle_expr (expr, swizzle_str + offset);
	swizzle = fold_constants (swizzle);
	if (is_error (swizzle) || is_zero (swizzle)) {
		return nullptr;
	}
	return edag_add_expr (swizzle);
}

static symbol_t *
get_mvec_sym (const type_t *type)
{
	auto symtab = get_mvec_struct (type);
	return symtab ? symtab->symbols : 0;
}

static const expr_t *
promote_scalar (const type_t *dst_type, const expr_t *scalar)
{
	auto scalar_type = get_type (scalar);
	if (scalar_type != dst_type) {
		if (!type_promotes (dst_type, scalar_type) && !scalar->implicit) {
			warning (scalar, "demoting %s to %s (use a cast)",
					 get_type_string (scalar_type),
					 get_type_string (dst_type));
		}
		scalar = cast_expr (dst_type, scalar);
	}
	return edag_add_expr (scalar);
}

static const expr_t *
forced_alias_expr (const type_t *type, const expr_t *e)
{
	e = fold_constants (e);
	auto a = new_expr ();
	a->type = ex_alias;
	a->alias = (ex_alias_t) {
		.type = type,
		.expr = edag_add_expr (e),
	};
	return a;
}

static expr_t *
new_mvec_expr (algebra_t *algebra, pr_uint_t group_mask)
{
	auto mvec = new_expr ();
	mvec->type = ex_multivec;
	mvec->multivec = (ex_multivec_t) {
		.type = algebra_mvec_type (algebra, group_mask),
		.algebra = algebra,
	};
	return mvec;
}

const expr_t *
mvec_expr (const expr_t *expr, algebra_t *algebra)
{
	expr = edag_add_expr (expr);

	auto mvtype = get_type (expr);
	if (is_reference (mvtype)) {
		mvtype = dereference_type (mvtype);
		expr = pointer_deref (expr);
	}

	if (expr->type == ex_multivec) {
		return expr;
	}

	expr_t *mvec;
	auto layout = &algebra->layout;
	const expr_t *components[layout->count] = {};
	int count = 0;
	if (is_scalar (mvtype) && !is_algebra (mvtype)) {
		auto c = &components[count++];
		expr = promote_scalar (algebra->type, expr);
		*c = forced_alias_expr (algebra->type, expr);

		// find the scalar's group (ie, no blades)
		int group = layout->group_map[layout->mask_map[0]].num;
		mvec = new_mvec_expr (algebra, 1 << group);
	} else {
		if (!is_algebra (mvtype)) {
			return error (expr, "invalid operand for GA");
		}

		pr_uint_t group_mask = (1u << layout->count) - 1;
		if (mvtype->type != ev_invalid) {
			group_mask = mvtype->multivec->group_mask;
		}
		mvec = new_mvec_expr (algebra, group_mask);
		if (!(group_mask & (group_mask - 1)) && expr->type == ex_alias) {
			auto c = &components[count++];
			*c = expr;
		} else {
			for (auto sym = get_mvec_sym (mvtype); sym; sym = sym->next) {
				auto c = &components[count++];
				auto e = new_field_sym_expr (expr, sym);
				e->field.member = edag_add_expr (e->field.member);
				e->field.type = float_type (sym->type);
				*c = forced_alias_expr (sym->type, e);
			}
		}
	}
	list_gather (&mvec->multivec.components, components, count);

	return mvec;
}

void
mvec_scatter (const expr_t **components, const expr_t *mvec, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	int  group;

	if (is_error (mvec)) {
		return;
	}

	if (mvec->type != ex_multivec) {
		auto type = get_type (mvec);
		if (!is_algebra (type)) {
			// find the scalar's group (ie, no blades)
			group = layout->group_map[layout->mask_map[0]].num;
		} else {
			if (type->type == ev_invalid) {
				internal_error (mvec, "full algebra in mvec_scatter");
			}
			pr_uint_t mask = type->multivec->group_mask;
			if (mask & (mask - 1)) {
				internal_error (mvec, "bare multivector in mvec_scatter");
			}
			group = BITOP_LOG2 (mask);
		}
		components[group] = edag_add_expr (mvec);
		return;
	}
	for (auto li = mvec->multivec.components.head; li; li = li->next) {
		auto c = li->expr;
		auto ct = get_type (c);
		if (c->type != ex_alias) {
			internal_error (mvec, "non-alias expression in multivec");
		}
		if (!is_algebra (ct)) {
			group = layout->group_map[layout->mask_map[0]].num;
		} else if (ct->meta == ty_algebra && ct->type != ev_invalid) {
			pr_uint_t mask = ct->multivec->group_mask;
			if (mask & (mask - 1)) {
				internal_error (mvec, "multivector in multivec expression");
			}
			group = BITOP_LOG2 (mask);
		} else {
			internal_error (mvec, "invalid type in multivec expression");
		}
		if (components[group]) {
			internal_error (mvec, "duplicate group in multivec expression");
		}
		components[group] = edag_add_expr (c->alias.expr);
	}
}

const expr_t *
mvec_gather (const expr_t **components, algebra_t *algebra)
{
	auto layout = &algebra->layout;

	pr_uint_t group_mask = 0;
	int count = 0;
	const expr_t *vec = 0;
	for (int i = 0; i < layout->count; i++) {
		if (components[i]) {
			count++;
			vec = components[i];
			auto type = algebra_mvec_type (algebra,
										   layout->groups[i].group_mask);
			vec = forced_alias_expr (type, vec);
			components[i] = vec;
			group_mask |= 1 << i;
		}
	}
	if (count == 1) {
		if (!is_algebra (get_type (vec))) {
			return vec->alias.expr;
		}
		return vec;
	}
	if (!count) {
		return new_zero_expr (algebra->type);
	}

	auto mvec = new_expr ();
	mvec->type = ex_multivec;
	mvec->multivec = (ex_multivec_t) {
		.type = algebra_mvec_type (algebra, group_mask),
		.algebra = algebra,
	};
	for (int i = layout->count; i-- > 0; ) {
		if (components[i]) {
			list_append (&mvec->multivec.components, components[i]);
		}
	}
	return mvec;
}

bool
is_scale (const expr_t *expr)
{
	return expr && expr->type == ex_expr && expr->expr.op == QC_SCALE;
}

const expr_t *
traverse_scale (const expr_t *expr)
{
	while (is_scale (expr)) {
		expr = expr->expr.e1;
	}
	return expr;
}

bool
is_sum (const expr_t *expr)
{
	return (expr && expr->type == ex_expr
			&& (expr->expr.op == '+' || expr->expr.op == '-'));
}

bool
is_mult (const expr_t *expr)
{
	return (expr && expr->type == ex_expr
			&& (expr->expr.op == '*' || expr->expr.op == QC_HADAMARD));
}

bool
is_quot (const expr_t *expr)
{
	return (expr && expr->type == ex_expr && expr->expr.op == '/');
}

int
count_terms (const expr_t *expr)
{
	if (!is_sum (expr)) {
		return 0;
	}
	auto e1 = expr->expr.e1;
	auto e2 = expr->expr.e2;
	int terms = (!is_sum (e1) || e1->paren) + (!is_sum (e2) || e2->paren);
	if (!e1->paren && is_sum (e1)) {
		terms += count_terms (expr->expr.e1);
	}
	if (!e2->paren && is_sum (e2)) {
		terms += count_terms (expr->expr.e2);
	}
	return terms;
}

static bool
can_factor (const expr_t *expr)
{
	if (!is_mult (expr) || expr->paren) {
		return false;
	}
	if (expr->expr.commutative && expr->expr.associative) {
		return true;
	}
	return false;
}

int __attribute__((pure))
count_factors (const expr_t *expr)
{
	if (is_neg (expr)) {
		expr = neg_expr (expr);
	}
	if (!is_mult (expr)) {
		return 0;
	}
	auto e1 = expr->expr.e1;
	auto e2 = expr->expr.e2;
	if (is_neg (e1)) {
		e1 = neg_expr (e1);
	}
	if (is_neg (e2)) {
		e2 = neg_expr (e2);
	}
	int factors = !can_factor (e1) + !can_factor (e2);
	if (can_factor (e1)) {
		factors += count_factors (expr->expr.e1);
	}
	if (can_factor (e2)) {
		factors += count_factors (expr->expr.e2);
	}
	return factors;
}

bool __attribute__((pure))
is_cross (const expr_t *expr)
{
	return (expr && expr->type == ex_expr && (expr->expr.op == QC_CROSS));
}

bool __attribute__((pure))
is_dot (const expr_t *expr)
{
	return (expr && expr->type == ex_expr && (expr->expr.op == QC_DOT));
}

static bool __attribute__((pure))
is_ortho (const expr_t *expr)
{
	if (!expr || expr->type != ex_swizzle
		|| type_width (get_type (expr->swizzle.src)) != 2) {
		return false;
	}
	auto c = expr->swizzle.source;
	unsigned neg = expr->swizzle.neg;
	return c[0] == 1 && c[1] == 0 && (neg == 1 || neg == 2);
}

static bool
scatter_factors_core (const expr_t *prod, const expr_t **factors, int *ind)
{
	bool neg = false;
	auto e1 = prod->expr.e1;
	auto e2 = prod->expr.e2;
	if (is_neg (e1)) {
		neg = !neg;
		e1 = neg_expr (e1);
	}
	if (is_neg (e2)) {
		neg = !neg;
		e2 = neg_expr (e2);
	}
	if (can_factor (e1)) {
		neg ^= scatter_factors_core (e1, factors, ind);
	} else {
		factors[(*ind)++] = e1;
	}
	if (can_factor (e2)) {
		neg ^= scatter_factors_core (e2, factors, ind);
	} else {
		factors[(*ind)++] = e2;
	}
	return neg;
}

bool
scatter_factors (const expr_t *prod, const expr_t **factors)
{
	bool neg = false;
	if (is_neg (prod)) {
		neg = true;
		prod = neg_expr (prod);
	}
	if (!is_mult (prod)) {
		factors[0] = prod;
		return neg;
	}
	int         ind = 0;
	return neg ^ scatter_factors_core (prod, factors, &ind);
}

static const expr_t *
gather_factors_core (int op, const expr_t **factors, int count)
{
	if (!count) {
		internal_error (0, "no factors to collect");
	}
	if (count == 1) {
		return *factors;
	}
	const expr_t *a, *b;
	if (count == 2) {
		a = factors[0];
		b = factors[1];
	} else {
		int mid = (count + 1) / 2;
		a = gather_factors_core (op, factors, mid);
		b = gather_factors_core (op, factors + mid, count - mid);
	}
	if (!a || !b) {
		return nullptr;
	}
	return binary_expr (op, a, b);
}

static int
expr_const_cmp (const void *_a, const void *_b)
{
	auto a = *(const expr_t **) _a;
	auto b = *(const expr_t **) _b;
	int cmp = is_constant (a) - is_constant (b);
	if (cmp == 0) {
		return a - b;
	}
	return cmp;
}

const expr_t *
gather_factors (int op, const type_t *type, const expr_t **factors, int count)
{
	if (!count) {
		internal_error (0, "no factors to collect");
	}
	bool commutative = true;
	for (int i = 0; i < count; i++) {
		if (!factors[i]) {
			return nullptr;
		}
		if (i) {
			commutative &= op_commute (op, get_type (factors[i - 1]),
									   get_type (factors[i]));
		}
	}
	if (commutative && count > 1) {
		heapsort (factors, count, sizeof (factors[0]), expr_const_cmp);
	}
	if (is_constant (factors[count - 1])) {
		for (int i = count - 1; i-- > 0; ) {
			if (!is_constant (factors[i])) {
				break;
			}
			auto e = typed_binary_expr (type, op, factors[i], factors[i + 1]);
			e = fold_constants (e);
			factors[i] = edag_add_expr (e);
			factors[i + 1] = 0;
			count--;
		}
		if (count > 1) {
			auto prod = gather_factors_core (op, factors, count - 1);
			prod = typed_binary_expr (type, op, prod, factors[count - 1]);
			return edag_add_expr (prod);
		}
	}
	return gather_factors_core (op, factors, count);
}

static int
expr_ptr_cmp (const void *_a, const void *_b)
{
	auto a = *(const expr_t **) _a;
	auto b = *(const expr_t **) _b;
	return a - b;
}
#if 0
static void
insert_expr (const expr_t **array, const expr_t *e, int count)
{
	auto dst = array;
	if (e > *dst) {
		dst = fbsearch (e, array, count, sizeof (dst[0]), expr_ptr_cmp);
	}
	memmove (dst + 1, dst, sizeof (expr_t *[count - (dst - array)]));
	*dst = e;
}
#endif
static const expr_t *
sort_factors (const type_t *type, const expr_t *e)
{
	if (!is_mult (e)) {
		internal_error (e, "not a product");
	}

	int count = count_factors (e);
	const expr_t *factors[count + 1] = {};
	scatter_factors (e, factors);
	heapsort (factors, count, sizeof (factors[0]), expr_ptr_cmp);
	auto mult = gather_factors ('*', type, factors, count);
	return mult;
}

static const expr_t *
sum_expr_low (int op, const expr_t *a, const expr_t *b)
{
	if (!a && !b) {
		return nullptr;
	}
	if (!a) {
		if (op == '-') {
			b = neg_expr (b);
		}
		return b;
	}
	if (!b) {
		return a;
	}
	if (op == '-' && a == b) {
		return nullptr;
	}

	auto type = get_type (a);
	auto sum = typed_binary_expr (type, op, a, b);
	sum = fold_constants (sum);
	sum = edag_add_expr (sum);
	return sum;
}

static void
scatter_terms_core (const expr_t *sum,
					   const expr_t **adds, int *addind,
					   const expr_t **subs, int *subind, bool negative)
{
	bool subtract = (sum->expr.op == '-') ^ negative;
	auto e1 = sum->expr.e1;
	auto e2 = sum->expr.e2;
	if (!e1->paren && is_sum (e1)) {
		scatter_terms_core (e1, adds, addind, subs, subind, negative);
	}
	if (!e2->paren && is_sum (e2)) {
		scatter_terms_core (e2, adds, addind, subs, subind, subtract);
	}
	if (e1->paren || !is_sum (e1)) {
		auto e = sum->expr.e1;
		auto arr = negative ^ is_neg (e) ? subs : adds;
		auto ind = negative ^ is_neg (e) ? subind : addind;
		if (is_neg (e)) {
			e = neg_expr (e);
		}
		arr[(*ind)++] = e;
	}
	if (e2->paren || !is_sum (e2)) {
		auto e = sum->expr.e2;
		auto arr = subtract ^ is_neg (e) ? subs : adds;
		auto ind = subtract ^ is_neg (e) ? subind : addind;
		if (is_neg (e)) {
			e = neg_expr (e);
		}
		arr[(*ind)++] = e;
	}
}

void
scatter_terms (const expr_t *sum, const expr_t **adds, const expr_t **subs)
{
	if (!is_sum (sum)) {
		internal_error (sum, "scatter_terms with no sum");
	}

	int         addind = 0;
	int         subind = 0;
	scatter_terms_core (sum, adds, &addind, subs, &subind, false);
}

const expr_t *
gather_terms (const type_t *type, const expr_t **adds, const expr_t **subs)
{
	const expr_t *a = 0;
	const expr_t *b = 0;
	for (auto s = adds; *s; s++) {
		a = sum_expr_low ('+', a, *s);
	}
	for (auto s = subs; *s; s++) {
		b = sum_expr_low ('+', b, *s);
	}
	auto sum = sum_expr_low ('-', a, b);
	if (sum && get_type (sum) != type) {
		sum = cast_expr (type, sum);
		sum = edag_add_expr (sum);
	}
	return sum;
}

const expr_t *
sum_expr (const expr_t *a, const expr_t *b)
{
	if (!a && !b) {
		return nullptr;
	}
	if (!a) {
		return b;
	}
	if (!b) {
		return a;
	}

	auto type = get_type (a);
	auto sum = typed_binary_expr (type, '+', a, b);
	int num_terms = count_terms (sum);
	const expr_t *adds[num_terms + 1] = {};
	const expr_t *subs[num_terms + 1] = {};
	scatter_terms (sum, adds, subs);
	const expr_t **dstadd, **srcadd;
	const expr_t **dstsub, **srcsub;

	merge_extends (adds, subs);

	for (auto term = adds; *term; term++) {
		if (is_mult (*term)) {
			*term = sort_factors (type, *term);
		}
	}

	for (auto term = subs; *term; term++) {
		if (is_mult (*term)) {
			*term = sort_factors (type, *term);
		}
	}

	for (dstadd = adds, srcadd = adds; *srcadd; srcadd++) {
		for (dstsub = subs, srcsub = subs; *srcsub; srcsub++) {
			if (*srcadd == *srcsub) {
				// found a-a
				break;
			}
		}
		dstsub = srcsub;
		if (*srcsub++) {
			// found a-a
			while (*srcsub) {
				*dstsub++ = *srcsub++;
			}
			*dstsub = 0;
			continue;
		}
		*dstadd++ = *srcadd;
	}
	*dstadd = 0;
	sum = gather_terms (type, adds, subs);
	return sum;
}

static void
component_sum (int op, const expr_t **c, const expr_t **a, const expr_t **b,
			   algebra_t *algebra)
{
	auto layout = &algebra->layout;
	for (int i = 0; i < layout->count; i++) {
		if (a[i] && b[i]) {
			if (get_type (a[i]) != get_type (b[i])) {
				internal_error (a[i], "tangled multivec types");
			}
			if (op == '+') {
				c[i] = sum_expr (a[i], b[i]);
			} else {
				c[i] = sum_expr (a[i], neg_expr (b[i]));
			}
		} else if (a[i]) {
			c[i] = a[i];
		} else if (b[i]) {
			if (op == '+') {
				c[i] = b[i];
			} else {
				c[i] = neg_expr (b[i]);
			}
		} else {
			c[i] = 0;
		}
	}
}

static const expr_t *
distribute_product (const expr_t *a, const expr_t *b,
					const expr_t *(*product) (const expr_t *a, const expr_t *b),
					bool anti_com)
{
	bool neg = false;
	if (is_neg (a)) {
		neg = !neg;
		a = neg_expr (a);
	}
	if (is_neg (b)) {
		neg = !neg;
		b = neg_expr (b);
	}
	if (anti_com && neg) {
		auto t = a;
		a = b;
		b = t;
		neg = false;
	}

	int a_terms = count_terms (a);
	int b_terms = count_terms (b);

	const expr_t *a_adds[a_terms + 2] = {};
	const expr_t *a_subs[a_terms + 2] = {};
	const expr_t *b_adds[b_terms + 2] = {};
	const expr_t *b_subs[b_terms + 2] = {};

	if (a_terms) {
		scatter_terms (a, a_adds, a_subs);
	} else {
		a_adds[0] = a;
	}

	if (b_terms) {
		scatter_terms (b, b_adds, b_subs);
	} else {
		b_adds[0] = b;
	}

	a = b = 0;

	for (auto i = a_adds; *i; i++) {
		for (auto j = b_adds; *j; j++) {
			auto p = product (*i, *j);
			if (p) {
				p = fold_constants (p);
				p = edag_add_expr (p);
				a = sum_expr (a, p);
			}
		}
	}
	for (auto i = a_subs; *i; i++) {
		for (auto j = b_subs; *j; j++) {
			auto p = product (*i, *j);
			if (p) {
				p = fold_constants (p);
				p = edag_add_expr (p);
				a = sum_expr (a, p);
			}
		}
	}
	for (auto i = a_adds; *i; i++) {
		for (auto j = b_subs; *j; j++) {
			auto p = product (*i, *j);
			if (p) {
				p = fold_constants (p);
				p = edag_add_expr (p);
				b = sum_expr (b, p);
			}
		}
	}
	for (auto i = a_subs; *i; i++) {
		for (auto j = b_adds; *j; j++) {
			auto p = product (*i, *j);
			if (p) {
				p = fold_constants (p);
				p = edag_add_expr (p);
				b = sum_expr (b, p);
			}
		}
	}
	if (neg) {
		// note order ----------------V--V
		auto sum = sum_expr_low ('-', b, a);
		return sum;
	} else {
		// note order ----------------V--V
		auto sum = sum_expr_low ('-', a, b);
		return sum;
	}
}

static const expr_t *
extract_scale (const expr_t **expr, const expr_t *prod)
{
	if (is_scale (*expr)) {
		auto s = (*expr)->expr.e2;
		prod = prod ? scale_expr (prod, s) : s;
		*expr = (*expr)->expr.e1;
	}
	return prod;
}

static const expr_t *
apply_scale (const type_t *type, const expr_t *expr, const expr_t *prod)
{
	if (expr && prod) {
		expr = fold_constants (expr);
		expr = edag_add_expr (expr);
		expr = scale_expr (expr, prod);
	}
	return expr;
}

static const expr_t *
do_mult (const expr_t *a, const expr_t *b)
{
	auto type = get_type (a);
	auto mult = typed_binary_expr (type, '*', a, b);
	return edag_add_expr (mult);
}

static const expr_t *
do_scale (const expr_t *a, const expr_t *b)
{
	const expr_t *prod = extract_scale (&a, b);
	if (prod) {
		b = prod;
	}
	auto type = get_type (a);
	auto scale = typed_binary_expr (type, QC_SCALE, a, b);
	return edag_add_expr (scale);
}

const expr_t *
scale_expr (const expr_t *a, const expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}
	if (!is_scalar (get_type (b)) || !is_real (get_type (b))) {
		internal_error (b, "not a real scalar type");
	}
	if (!is_real (get_type (b))) {
		internal_error (b, "not a real scalar type");
	}
	if (is_constant (b)) {
		double s = expr_floating (b);
		if (s == 1) {
			return a;
		}
		if (s == -1) {
			return neg_expr (a);
		}
	}

	a = cast_expr (float_type (get_type (a)), a);

	auto op = is_scalar (get_type (a)) ? do_mult : do_scale;
	auto scale = distribute_product (a, b, op, false);
	if (!scale) {
		return 0;
	}
	scale = edag_add_expr (scale);
	return scale;
}

static const expr_t *
mult_expr (const expr_t *a, const expr_t *b)
{
	bool neg = false;
	if (is_neg (a)) {
		a = neg_expr (a);
		neg = !neg;
	}
	if (is_neg (b)) {
		b = neg_expr (b);
		neg = !neg;
	}
	auto mult = do_mult (b, a);
	if (neg) {
		mult = neg_expr (mult);
	}
	return mult;
}

bool __attribute__((pure))
reject_dot (const expr_t *a, const expr_t *b)
{
	a = traverse_scale (a);
	b = traverse_scale (b);
	if (is_cross (a)) {
		if (b == traverse_scale (a->expr.e1)
			|| b == traverse_scale (a->expr.e2)) {
			return true;
		}
	}
	if (is_ortho (a)) {
		if (b == traverse_scale (a->swizzle.src)) {
			return true;
		}
	}
	if (is_cross (b)) {
		if (a == traverse_scale (b->expr.e1)
			|| a == traverse_scale (b->expr.e2)) {
			return true;
		}
	}
	if (is_ortho (b)) {
		if (a == traverse_scale (b->swizzle.src)) {
			return true;
		}
	}
	return false;
}

static const expr_t *
do_dot (const expr_t *a, const expr_t *b)
{
	if (reject_dot (a, b)) {
		return 0;
	}

	const expr_t *prod = 0;
	prod = extract_scale (&a, prod);
	prod = extract_scale (&b, prod);

	auto type = base_type (get_type (a));
	auto dot = typed_binary_expr (type, QC_DOT, a, b);
	dot = edag_add_expr (dot);
	dot = apply_scale (type, dot, prod);
	return dot;
}

const expr_t *
dot_expr (const expr_t *a, const expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}

	auto dot = distribute_product (a, b, do_dot, false);
	return dot;
}

static bool __attribute__((pure))
reject_cross (const expr_t *a, const expr_t *b)
{
	return traverse_scale (a) == traverse_scale (b);
}

static const expr_t *
do_cross (const expr_t *a, const expr_t *b)
{
	if (reject_cross (a, b)) {
		return 0;
	}

	const expr_t *prod = 0;
	prod = extract_scale (&a, prod);
	prod = extract_scale (&b, prod);

	auto type = get_type (a);
	auto cross = typed_binary_expr (type, QC_CROSS, a, b);
	cross = edag_add_expr (cross);
	cross = apply_scale (type, cross, prod);
	return cross;
}

static const expr_t *
cross_expr (const expr_t *a, const expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}

	auto cross = distribute_product (a, b, do_cross, true);
	return cross;
}

static bool __attribute__((pure))
reject_wedge (const expr_t *a, const expr_t *b)
{
	return traverse_scale (a) == traverse_scale (b);
}

static const expr_t *
do_wedge (const expr_t *a, const expr_t *b)
{
	if (reject_wedge (a, b)) {
		return 0;
	}

	const expr_t *prod = 0;
	prod = extract_scale (&a, prod);
	prod = extract_scale (&b, prod);

	auto type = base_type (get_type (a));
	auto wedge = typed_binary_expr (type, QC_WEDGE, a, b);
	wedge = edag_add_expr (wedge);
	wedge = apply_scale (type, wedge, prod);
	return wedge;
}

static const expr_t *
wedge_expr (const type_t *type, const expr_t *a, const expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}
	auto wedge = distribute_product (a, b, do_wedge, true);
	return wedge;
}

typedef void (*pga_func) (const expr_t **c,
						  const expr_t *a, int ga, const expr_t *b, int gb,
						  algebra_t *alg);

static void
scale_component (const expr_t **c,
				 const expr_t *a, int ga, const expr_t *b, int gb,
				 algebra_t *alg)
{
	int group = ga;
	if (is_nonscalar (get_type (b))) {
		auto t = a;
		a = b;
		b = t;
		group = gb;
	}
	if (is_scalar (get_type (a))) {
		a = promote_scalar (alg->type, a);
	}
	b = promote_scalar (alg->type, b);
	auto scale = scale_expr (a, b);
	c[group] = scale;
}

static void
pga3_scale_component (const expr_t **c,
					  const expr_t *a, int ga, const expr_t *b, int gb,
					  algebra_t *alg)
{
	int group = ga;
	if (is_nonscalar (get_type (b))) {
		auto t = a;
		a = b;
		b = t;
		group = gb;
	}
	b = promote_scalar (alg->type, b);

	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto scale_type = float_type (get_type (a));
	auto va = scale_expr (offset_cast (vtype, a, 0), b);
	auto sa = scale_expr (offset_cast (stype, a, 3), b);
	auto scale = sum_expr (ext_expr (va, scale_type, 0, false),
						   ext_expr (sa, scale_type, 0, true));
	c[group] = scale;
}

static void
pga3_scale_wxyz (const expr_t **c,
				 const expr_t *a, int ga,
				 const expr_t *b, int gb,
				 algebra_t *alg)
{
	c[4] = mult_expr (b, a);
}

static void
pga3_wxyz_scale (const expr_t **c,
				 const expr_t *a, int ga,
				 const expr_t *b, int gb,
				 algebra_t *alg)
{
	c[4] = mult_expr (b, a);
}

static void
pga3_x_y_z_w_dot_x_y_z_w (const expr_t **c,
						  const expr_t *a, int ga,
						  const expr_t *b, int gb,
						  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = dot_expr (va, vb);
}

static void
pga3_x_y_z_w_dot_yz_zx_xy (const expr_t **c,
						   const expr_t *a, int ga,
						   const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = ext_expr (cross_expr (vb, va), vec_type, 0, false);
}

static void
pga3_x_y_z_w_dot_wx_wy_wz (const expr_t **c,
						   const expr_t *a, int ga,
						   const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = neg_expr (dot_expr (vb, va));
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_x_y_z_w_dot_wxyz (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	auto cv = scale_expr (va, sb);
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_x_y_z_w_dot_wzy_wxz_wyx_xyz (const expr_t **c,
								  const expr_t *a, int ga,
								  const expr_t *b, int gb,
								  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	c[1] = scale_expr (va, sb);
	c[3] = cross_expr (vb, va);
}

static void
pga3_yz_zx_xy_dot_x_y_z_w (const expr_t **c,
						   const expr_t *a, int ga,
						   const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = ext_expr (cross_expr (vb, va), vec_type, 0, false);
}

static void
pga3_yz_zx_xy_dot_yz_zx_xy (const expr_t **c,
							const expr_t *a, int ga,
							const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = neg_expr (dot_expr (va, vb));
}

static void
pga3_yz_zx_xy_dot_wxyz (const expr_t **c,
						const expr_t *a, int ga,
						const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (va, sb));
}

static void
pga3_yz_zx_xy_dot_wzy_wxz_wyx_xyz (const expr_t **c,
								   const expr_t *a, int ga,
								   const expr_t *b, int gb,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);

	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);

	auto cv = scale_expr (va, sb);
	auto cs = dot_expr (va, vb);

	cv = ext_expr (neg_expr (cv), vec_type, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cv, cs);
}

static void
pga3_wx_wy_wz_dot_x_y_z_w (const expr_t **c,
						   const expr_t *a, int ga,
						   const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (va, vb);
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_wxyz_dot_x_y_z_w (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vb = edag_add_expr (new_swizzle_expr (b, "-x-y-z0"));
	auto sa = offset_cast (stype, a, 0);
	c[5] = scale_expr (vb, sa);
}

static void
pga3_wxyz_dot_yz_zx_xy (const expr_t **c,
						const expr_t *a, int ga,
						const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = neg_expr (scale_expr (vb, sa));
}

static void
pga3_wxyz_dot_wzy_wxz_wyx_xyz (const expr_t **c,
							   const expr_t *a, int ga,
							   const expr_t *b, int gb,
							   algebra_t *alg)
{
	auto stype = alg->type;
	auto vec_type = algebra_float_type (alg, 0x01);
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cs = neg_expr (scale_expr (sa, sb));
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_x_y_z_w (const expr_t **c,
								  const expr_t *a, int ga,
								  const expr_t *b, int gb,
								  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = scale_expr (vb, sa);
	c[3] = cross_expr (va, vb);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_yz_zx_xy (const expr_t **c,
								   const expr_t *a, int ga,
								   const expr_t *b, int gb,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);

	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);

	auto cv = scale_expr (vb, sa);
	auto cs = dot_expr (vb, va);

	cv = ext_expr (neg_expr (cv), vec_type, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cv, cs);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wxyz (const expr_t **c,
							   const expr_t *a, int ga,
							   const expr_t *b, int gb,
							   algebra_t *alg)
{
	auto stype = alg->type;
	auto vec_type = algebra_float_type (alg, 0x01);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 0);
	auto cs = scale_expr (sa, sb);
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wzy_wxz_wyx_xyz (const expr_t **c,
										  const expr_t *a, int ga,
										  const expr_t *b, int gb,
										  algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[2] = neg_expr (scale_expr (sa, sb));
}

static pga_func pga3_dot_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_dot_x_y_z_w,
		[1] = pga3_x_y_z_w_dot_yz_zx_xy,
		[2] = pga3_scale_component,
		[3] = pga3_x_y_z_w_dot_wx_wy_wz,
		[4] = pga3_x_y_z_w_dot_wxyz,
		[5] = pga3_x_y_z_w_dot_wzy_wxz_wyx_xyz,
	},
	[1] = {
		[0] = pga3_yz_zx_xy_dot_x_y_z_w,
		[1] = pga3_yz_zx_xy_dot_yz_zx_xy,
		[2] = scale_component,
		[4] = pga3_yz_zx_xy_dot_wxyz,
		[5] = pga3_yz_zx_xy_dot_wzy_wxz_wyx_xyz,
	},
	[2] = {
		[0] = pga3_scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = pga3_scale_wxyz,
		[5] = pga3_scale_component,
	},
	[3] = {
		[0] = pga3_wx_wy_wz_dot_x_y_z_w,
		[2] = scale_component,
	},
	[4] = {
		[0] = pga3_wxyz_dot_x_y_z_w,
		[1] = pga3_wxyz_dot_yz_zx_xy,
		[2] = pga3_wxyz_scale,
		[5] = pga3_wxyz_dot_wzy_wxz_wyx_xyz,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_dot_x_y_z_w,
		[1] = pga3_wzy_wxz_wyx_xyz_dot_yz_zx_xy,
		[2] = pga3_scale_component,
		[4] = pga3_wzy_wxz_wyx_xyz_dot_wxyz,
		[5] = pga3_wzy_wxz_wyx_xyz_dot_wzy_wxz_wyx_xyz,
	},
};

static void
pga2_wxy_scale (const expr_t **c,
				const expr_t *a, int ga,
				const expr_t *b, int gb,
				algebra_t *alg)
{
	c[1] = do_mult (a, b);
}

static void
pga2_yw_wx_xy_dot_yw_wx_xy (const expr_t **c,
							const expr_t *a, int ga,
							const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 2);

	c[3] = neg_expr (scale_expr (sa, sb));
}

static void
pga2_yw_wx_xy_dot_x_y_w (const expr_t **c,
						 const expr_t *a, int ga,
						 const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (wtype, a, 0);
	auto sa = offset_cast (stype, a, 2);
	auto vb = offset_cast (wtype, b, 0);
	auto cv = edag_add_expr (new_swizzle_expr (vb, "y-x"));
	auto cs = wedge_expr (stype, vb, va);
	cv = ext_expr (scale_expr (cv, sa), vtype, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cv, cs);
}

static void
pga2_yw_wx_xy_dot_wxy (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 0);
	auto cs = neg_expr (scale_expr (sa, sb));
	c[2] = ext_expr (cs, bvec_type, 0, true);
}

static void
pga2_x_y_w_dot_yw_wx_xy (const expr_t **c,
						 const expr_t *a, int ga,
						 const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto va = offset_cast (wtype, a, 0);
	auto vb = offset_cast (wtype, b, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cv = scale_expr (edag_add_expr (new_swizzle_expr (va, "-yx")), sb);
	auto cs = wedge_expr (stype, vb, va);
	cv = ext_expr (cv, vec_type, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cs, cv);
}

static void
pga2_x_y_w_dot_x_y_w (const expr_t **c,
					  const expr_t *a, int ga,
					  const expr_t *b, int gb,
					  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (va, vb);
	c[3] = cs;
}

static void
pga2_x_y_w_dot_wxy (const expr_t **c,
					const expr_t *a, int ga,
					const expr_t *b, int gb,
					algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto va = offset_cast (vtype, a, 0);
	auto cv = scale_expr (va, b);

	c[2] = ext_expr (cv, bvec_type, 0, false);
}

static void
pga2_wxy_dot_yw_wx_xy (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cs = neg_expr (scale_expr (sa, sb));
	c[2] = ext_expr (cs, bvec_type, 0, true);
}

static void
pga2_wxy_dot_x_y_w (const expr_t **c,
					const expr_t *a, int ga,
					const expr_t *b, int gb,
					algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = scale_expr (vb, sa);
	c[2] = ext_expr (cv, bvec_type, 0, false);
}

static pga_func pga2_dot_funcs[4][4] = {
	[0] = {
		[0] = pga2_x_y_w_dot_x_y_w,
		[1] = pga2_x_y_w_dot_wxy,
		[2] = pga2_x_y_w_dot_yw_wx_xy,
		[3] = scale_component,
	},
	[1] = {
		[0] = pga2_wxy_dot_x_y_w,
		[2] = pga2_wxy_dot_yw_wx_xy,
		[3] = pga2_wxy_scale,
	},
	[2] = {
		[0] = pga2_yw_wx_xy_dot_x_y_w,
		[1] = pga2_yw_wx_xy_dot_wxy,
		[2] = pga2_yw_wx_xy_dot_yw_wx_xy,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = pga2_wxy_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
vga3_xyz_scale (const expr_t **c,
				const expr_t *a, int ga,
				const expr_t *b, int gb,
				algebra_t *alg)
{
	c[1] = mult_expr (b, a);
}

static void
vga3_x_y_z_dot_x_y_z (const expr_t **c,
					  const expr_t *a, int ga,
					  const expr_t *b, int gb,
					  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = dot_expr (va, vb);
}

static void
vga3_x_y_z_dot_yz_zx_xy (const expr_t **c,
						 const expr_t *a, int ga,
						 const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = cross_expr (vb, va);
}

static void
vga3_x_y_z_dot_xyz (const expr_t **c,
					const expr_t *a, int ga,
					const expr_t *b, int gb,
					algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[2] = scale_expr (va, sb);
}

static void
vga3_yz_zx_xy_dot_x_y_z (const expr_t **c,
						 const expr_t *a, int ga,
						 const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = cross_expr (vb, va);
}

static void
vga3_yz_zx_xy_dot_yz_zx_xy (const expr_t **c,
							const expr_t *a, int ga,
							const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = neg_expr (dot_expr (va, vb));
}

static void
vga3_yz_zx_xy_dot_xyz (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[0] = neg_expr (scale_expr (va, sb));
}

static void
vga3_xyz_dot_x_y_z (const expr_t **c,
					const expr_t *a, int ga,
					const expr_t *b, int gb,
					algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = scale_expr (vb, sa);
}

static void
vga3_xyz_dot_yz_zx_xy (const expr_t **c,
					   const expr_t *a, int ga,
					   const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = neg_expr (scale_expr (vb, sa));
}

static void
vga3_xyz_dot_xyz (const expr_t **c,
				  const expr_t *a, int ga,
				  const expr_t *b, int gb,
				  algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (sb, sa));
}


static pga_func vga3_dot_funcs[4][4] = {
	[0] = {
		[0] = vga3_x_y_z_dot_x_y_z,
		[1] = vga3_x_y_z_dot_xyz,
		[2] = vga3_x_y_z_dot_yz_zx_xy,
		[3] = scale_component,
	},
	[1] = {
		[0] = vga3_xyz_dot_x_y_z,
		[1] = vga3_xyz_dot_xyz,
		[2] = vga3_xyz_dot_yz_zx_xy,
		[3] = vga3_xyz_scale,
	},
	[2] = {
		[0] = vga3_yz_zx_xy_dot_x_y_z,
		[1] = vga3_yz_zx_xy_dot_xyz,
		[2] = vga3_yz_zx_xy_dot_yz_zx_xy,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = vga3_xyz_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
component_dot (const expr_t **c,
			   const expr_t *a, int ga, const expr_t *b, int gb,
			   algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		if (pga3_dot_funcs[ga][gb]) {
			pga3_dot_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		if (pga2_dot_funcs[ga][gb]) {
			pga2_dot_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 3 && m == 0 && z == 0) {
		if (vga3_dot_funcs[ga][gb]) {
			vga3_dot_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static const expr_t *
inner_product (const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	const expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				const expr_t *w[layout->count] = {};
				component_dot (w, a[i], i, b[j], j, algebra);
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static void
pga3_x_y_z_w_wedge_x_y_z_w (const expr_t **c,
							const expr_t *a, int ga,
							const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[1] = cross_expr (va, vb);
	c[3] = sum_expr (scale_expr (vb, sa), neg_expr (scale_expr (va, sb)));
}

static void
pga3_x_y_z_w_wedge_yz_zx_xy (const expr_t **c,
							 const expr_t *a, int ga,
							 const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vb, sa);
	auto cs = dot_expr (va, vb);

	cv = ext_expr (neg_expr (cv), tvec_type, 0, false);
	cs = ext_expr (cs, tvec_type, 0, true);
	c[5] = sum_expr (cv, cs);
}

static void
pga3_x_y_z_w_wedge_wx_wy_wz (const expr_t **c,
							 const expr_t *a, int ga,
							 const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = cross_expr (va, vb);
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_x_y_z_w_wedge_wzy_wxz_wyx_xyz (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 4);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[4] = dot_expr (va, vb);
}

static void
pga3_yz_zx_xy_wedge_x_y_z_w (const expr_t **c,
							 const expr_t *a, int ga,
							 const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (va, sb);
	auto cs = dot_expr (vb, va);

	cv = ext_expr (neg_expr (cv), tvec_type, 0, false);
	cs = ext_expr (cs, tvec_type, 0, true);
	c[5] = sum_expr (cv, cs);
}

// bivector-bivector wedge is commutative
#define pga3_wx_wy_wz_wedge_yz_zx_xy pga3_yz_zx_xy_wedge_wx_wy_wz
static void
pga3_yz_zx_xy_wedge_wx_wy_wz (const expr_t **c,
							  const expr_t *a, int ga,
							  const expr_t *b, int gb,
							  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[4] = dot_expr (va, vb);
}

static void
pga3_wx_wy_wz_wedge_x_y_z_w (const expr_t **c,
							 const expr_t *a, int ga,
							 const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = cross_expr (vb, va);

	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_wedge_x_y_z_w (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 4);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[4] = neg_expr (dot_expr (va, vb));
}

static pga_func pga3_wedge_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_wedge_x_y_z_w,
		[1] = pga3_x_y_z_w_wedge_yz_zx_xy,
		[2] = pga3_scale_component,
		[3] = pga3_x_y_z_w_wedge_wx_wy_wz,
		[5] = pga3_x_y_z_w_wedge_wzy_wxz_wyx_xyz,
	},
	[1] = {
		[0] = pga3_yz_zx_xy_wedge_x_y_z_w,
		[2] = scale_component,
		[3] = pga3_yz_zx_xy_wedge_wx_wy_wz,
	},
	[2] = {
		[0] = pga3_scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = pga3_scale_wxyz,
		[5] = pga3_scale_component,
	},
	[3] = {
		[0] = pga3_wx_wy_wz_wedge_x_y_z_w,
		[1] = pga3_wx_wy_wz_wedge_yz_zx_xy,
		[2] = scale_component,
	},
	[4] = {
		[2] = pga3_wxyz_scale,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_wedge_x_y_z_w,
		[2] = pga3_scale_component,
	},
};

// vector-bivector wedge is commutative
#define pga2_x_y_w_wedge_yw_wx_xy pga2_yw_wx_xy_wedge_x_y_w
static void
pga2_yw_wx_xy_wedge_x_y_w (const expr_t **c,
						   const expr_t *a, int ga, const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = dot_expr (va, vb);
}

static void
pga2_x_y_w_wedge_x_y_w (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = cross_expr (va, vb);
}

static pga_func pga2_wedge_funcs[4][4] = {
	[0] = {
		[0] = pga2_x_y_w_wedge_x_y_w,
		[2] = pga2_x_y_w_wedge_yw_wx_xy,
		[3] = scale_component,
	},
	[1] = {
		[3] = pga2_wxy_scale,
	},
	[2] = {
		[0] = pga2_yw_wx_xy_wedge_x_y_w,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = pga2_wxy_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
vga3_x_y_z_wedge_x_y_z (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = cross_expr (va, vb);
}

static void
vga3_x_y_z_wedge_yz_zx_xy (const expr_t **c,
						   const expr_t *a, int ga, const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = dot_expr (va, vb);
}

static void
vga3_yz_zx_xy_wedge_x_y_z (const expr_t **c,
						   const expr_t *a, int ga, const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = dot_expr (va, vb);
}

static pga_func vga3_wedge_funcs[4][4] = {
	[0] = {
		[0] = vga3_x_y_z_wedge_x_y_z,
		[2] = vga3_x_y_z_wedge_yz_zx_xy,
		[3] = scale_component,
	},
	[1] = {
		[3] = vga3_xyz_scale,
	},
	[2] = {
		[0] = vga3_yz_zx_xy_wedge_x_y_z,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = vga3_xyz_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
component_wedge (const expr_t **c,
				 const expr_t *a, int ga, const expr_t *b, int gb,
				 algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		if (pga3_wedge_funcs[ga][gb]) {
			pga3_wedge_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		if (pga2_wedge_funcs[ga][gb]) {
			pga2_wedge_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 3 && m == 0 && z == 0) {
		if (vga3_wedge_funcs[ga][gb]) {
			vga3_wedge_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static const expr_t *
outer_product (const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	const expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				const expr_t *w[layout->count] = {};
				component_wedge (w, a[i], i, b[j], j, algebra);
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static void
pga3_x_y_z_w_geom_x_y_z_w (const expr_t **c,
						   const expr_t *a, int ga, const expr_t *b, int gb,
						   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[2] = dot_expr (va, vb);
	c[1] = cross_expr (va, vb);
	c[3] = sum_expr (scale_expr (vb, sa), neg_expr (scale_expr (va, sb)));
}

static void
pga3_x_y_z_w_geom_yz_zx_xy (const expr_t **c,
							const expr_t *a, int ga, const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vb, sa);
	auto cs = dot_expr (va, vb);
	c[0] = ext_expr (cross_expr (vb, va), vec_type, 0, false);
	cv = ext_expr (neg_expr (cv), tvec_type, 0, false);
	cs = ext_expr (cs, tvec_type, 0, true);
	c[5] = sum_expr (cv, cs);
}

static void
pga3_x_y_z_w_geom_wx_wy_wz (const expr_t **c,
							const expr_t *a, int ga, const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = neg_expr (dot_expr (va, vb));
	c[5] = ext_expr (cross_expr (va, vb), tvec_type, 0, false);
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_x_y_z_w_geom_wxyz (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	auto cv = scale_expr (va, sb);
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_x_y_z_w_geom_wzy_wxz_wyx_xyz (const expr_t **c,
								   const expr_t *a, int ga,
								   const expr_t *b, int gb,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[1] = scale_expr (va, sb);
	c[3] = cross_expr (vb, va);
	c[4] = sum_expr (dot_expr (va, vb), scale_expr (sa, sb));
}

static void
pga3_yz_zx_xy_geom_x_y_z_w (const expr_t **c,
							const expr_t *a, int ga, const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (va, sb);
	auto cs = dot_expr (vb, va);
	c[0] = ext_expr (cross_expr (vb, va), vec_type, 0, false);
	cv = ext_expr (neg_expr (cv), tvec_type, 0, false);
	cs = ext_expr (cs, tvec_type, 0, true);
	c[5] = sum_expr (cv, cs);
}

static void
pga3_yz_zx_xy_geom_yz_zx_xy (const expr_t **c,
							 const expr_t *a, int ga, const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = cross_expr (vb, va);
	c[2] = neg_expr (dot_expr (vb, va));
}

static void
pga3_yz_zx_xy_geom_wx_wy_wz (const expr_t **c,
							 const expr_t *a, int ga, const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = cross_expr (vb, va);
	c[4] = dot_expr (vb, va);
}

static void
pga3_yz_zx_xy_geom_wxyz (const expr_t **c,
						 const expr_t *a, int ga, const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (va, sb));
}

static void
pga3_yz_zx_xy_geom_wzy_wxz_wyx_xyz (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (va, sb);
	auto cs = dot_expr (vb, va);
	cv = ext_expr (neg_expr (cv), vec_type, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cv, cs);
	c[5] = ext_expr (cross_expr (vb, va), tvec_type, 0, false);
}

static void
pga3_wx_wy_wz_geom_x_y_z_w (const expr_t **c,
							const expr_t *a, int ga, const expr_t *b, int gb,
							algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (vb, va);
	c[5] = ext_expr (cross_expr (vb, va), tvec_type, 0, false);
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_wx_wy_wz_geom_yz_zx_xy (const expr_t **c,
							 const expr_t *a, int ga, const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = cross_expr (vb, va);
	c[4] = dot_expr (va, vb);
}

static void
pga3_wx_wy_wz_geom_wzy_wxz_wyx_xyz (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto vs = offset_cast (stype, b, 3);
	auto cv = neg_expr (scale_expr (va, vs));
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_wxyz_geom_x_y_z_w (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = neg_expr (scale_expr (vb, sa));
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_wxyz_geom_yz_zx_xy (const expr_t **c,
						 const expr_t *a, int ga, const expr_t *b, int gb,
						 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = neg_expr (scale_expr (vb, sa));
}

static void
pga3_wxyz_geom_wzy_wxz_wyx_xyz (const expr_t **c,
								const expr_t *a, int ga,
								const expr_t *b, int gb,
								algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto cs = scale_expr (sa, sb);
	c[0] = ext_expr (neg_expr (cs), vec_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_x_y_z_w (const expr_t **c,
								   const expr_t *a, int ga,
								   const expr_t *b, int gb,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[1] = scale_expr (vb, sa);
	c[3] = cross_expr (va, vb);
	c[4] = neg_expr (sum_expr (dot_expr (va, vb), scale_expr (sa, sb)));
}

static void
pga3_wzy_wxz_wyx_xyz_geom_yz_zx_xy (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vb, sa);
	auto cs = dot_expr (va, vb);
	cv = ext_expr (neg_expr (cv), vec_type, 0, false);
	cs = ext_expr (cs, vec_type, 0, true);
	c[0] = sum_expr (cv, cs);
	c[5] = ext_expr (cross_expr (vb, va), tvec_type, 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wx_wy_wz (const expr_t **c,
									const expr_t *a, int ga,
									const expr_t *b, int gb,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto tvec_type = algebra_float_type (alg, 0x20);
	auto sa = offset_cast (stype, a, 3);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = scale_expr (vb, sa);
	c[5] = ext_expr (cv, tvec_type, 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wxyz (const expr_t **c,
								const expr_t *a, int ga,
								const expr_t *b, int gb,
								algebra_t *alg)
{
	auto stype = alg->type;
	auto vec_type = algebra_float_type (alg, 0x01);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 0);
	auto cs = scale_expr (sa, sb);
	c[0] = ext_expr (cs, vec_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wzy_wxz_wyx_xyz (const expr_t **c,
										   const expr_t *a, int ga,
										   const expr_t *b, int gb,
										   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[2] = neg_expr (scale_expr (sa, sb));
	c[3] = sum_expr (scale_expr (va, sb), neg_expr (scale_expr (vb, sa)));
}

static pga_func pga3_geometric_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_geom_x_y_z_w,
		[1] = pga3_x_y_z_w_geom_yz_zx_xy,
		[2] = pga3_scale_component,
		[3] = pga3_x_y_z_w_geom_wx_wy_wz,
		[4] = pga3_x_y_z_w_geom_wxyz,
		[5] = pga3_x_y_z_w_geom_wzy_wxz_wyx_xyz,
	},
	[1] = {
		[0] = pga3_yz_zx_xy_geom_x_y_z_w,
		[1] = pga3_yz_zx_xy_geom_yz_zx_xy,
		[2] = scale_component,
		[3] = pga3_yz_zx_xy_geom_wx_wy_wz,
		[4] = pga3_yz_zx_xy_geom_wxyz,
		[5] = pga3_yz_zx_xy_geom_wzy_wxz_wyx_xyz,
	},
	[2] = {
		[0] = pga3_scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = pga3_scale_wxyz,
		[5] = pga3_scale_component,
	},
	[3] = {
		[0] = pga3_wx_wy_wz_geom_x_y_z_w,
		[1] = pga3_wx_wy_wz_geom_yz_zx_xy,
		[2] = scale_component,
		[5] = pga3_wx_wy_wz_geom_wzy_wxz_wyx_xyz,
	},
	[4] = {
		[0] = pga3_wxyz_geom_x_y_z_w,
		[1] = pga3_wxyz_geom_yz_zx_xy,
		[2] = pga3_wxyz_scale,
		[5] = pga3_wxyz_geom_wzy_wxz_wyx_xyz,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_geom_x_y_z_w,
		[1] = pga3_wzy_wxz_wyx_xyz_geom_yz_zx_xy,
		[2] = pga3_scale_component,
		[3] = pga3_wzy_wxz_wyx_xyz_geom_wx_wy_wz,
		[4] = pga3_wzy_wxz_wyx_xyz_geom_wxyz,
		[5] = pga3_wzy_wxz_wyx_xyz_geom_wzy_wxz_wyx_xyz,
	},
};

static void
pga2_yw_wx_xy_geom_yw_wx_xy (const expr_t **c,
							 const expr_t *a, int ga, const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto ctype = vector_type (stype, 2);
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 2);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = cross_expr (vb, va);

	if (cv) {
		c[2] = ext_expr (offset_cast (ctype, cv, 0), bvec_type, 0, false);
	}
	c[3] = neg_expr (scale_expr (sa, sb));
}

static void
pga2_yw_wx_xy_geom_x_y_w (const expr_t **c,
						  const expr_t *a, int ga, const expr_t *b, int gb,
						  algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto wa = offset_cast (wtype, a, 0);
	auto wb = offset_cast (wtype, b, 0);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 2);
	auto cv = edag_add_expr (new_swizzle_expr (wb, "y-x"));
	auto cs = wedge_expr (stype, wb, wa);
	cs = ext_expr (cs, vtype, 0, true);
	cv = ext_expr (scale_expr (cv, sa), vtype, 0, false);
	c[0] = sum_expr (cv, cs);
	c[1] = dot_expr (va, vb);
}

static void
pga2_yw_wx_xy_geom_wxy (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 0);
	auto cs = neg_expr (scale_expr (sa, sb));
	c[2] = ext_expr (cs, bvec_type, 0, true);
}

static void
pga2_x_y_w_geom_yw_wx_xy (const expr_t **c,
						  const expr_t *a, int ga, const expr_t *b, int gb,
						  algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto vec_type = algebra_float_type (alg, 0x01);
	auto wa = offset_cast (wtype, a, 0);
	auto wb = offset_cast (wtype, b, 0);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cv = edag_add_expr (new_swizzle_expr (wa, "-yx"));
	auto cs = wedge_expr (stype, wb, wa);
	cs = ext_expr (cs, vec_type, 0, true);
	cv = ext_expr (scale_expr (cv, sb), vec_type, 0, false);
	c[0] = sum_expr (cv, cs);
	c[1] = dot_expr (va, vb);
}

static void
pga2_x_y_w_geom_x_y_w (const expr_t **c,
					   const expr_t *a, int ga, const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto ctype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto ca = offset_cast (ctype, a, 0);
	auto cb = offset_cast (ctype, b, 0);
	c[3] = dot_expr (va, vb);
	c[2] = cross_expr (ca, cb);
}

static void
pga2_x_y_w_geom_wxy (const expr_t **c,
					 const expr_t *a, int ga, const expr_t *b, int gb,
					 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	auto cv = scale_expr (va, sb);

	c[2] = ext_expr (cv, bvec_type, 0, false);
}

static void
pga2_wxy_geom_yw_wx_xy (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cs = neg_expr (scale_expr (sa, sb));
	c[2] = ext_expr (cs, bvec_type, 0, true);
}

static void
pga2_wxy_geom_x_y_w (const expr_t **c,
					 const expr_t *a, int ga, const expr_t *b, int gb,
					 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto bvec_type = algebra_float_type (alg, 0x04);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = scale_expr (vb, sa);

	c[2] = ext_expr (cv, bvec_type, 0, false);
}

static pga_func pga2_geometric_funcs[6][6] = {
	[0] = {
		[0] = pga2_x_y_w_geom_x_y_w,
		[1] = pga2_x_y_w_geom_wxy,
		[2] = pga2_x_y_w_geom_yw_wx_xy,
		[3] = scale_component,
	},
	[1] = {
		[0] = pga2_wxy_geom_x_y_w,
		[2] = pga2_wxy_geom_yw_wx_xy,
		[3] = pga2_wxy_scale,
	},
	[2] = {
		[0] = pga2_yw_wx_xy_geom_x_y_w,
		[1] = pga2_yw_wx_xy_geom_wxy,
		[2] = pga2_yw_wx_xy_geom_yw_wx_xy,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = pga2_wxy_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
vga3_x_y_z_geom_x_y_z (const expr_t **c,
					   const expr_t *a, int ga, const expr_t *b, int gb,
					   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = dot_expr (va, vb);
	c[2] = cross_expr (va, vb);
}

static void
vga3_x_y_z_geom_yz_zx_xy (const expr_t **c,
						  const expr_t *a, int ga, const expr_t *b, int gb,
						  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = cross_expr (vb, va);
	c[1] = dot_expr (va, vb);
}

static void
vga3_x_y_z_geom_xyz (const expr_t **c,
					 const expr_t *a, int ga, const expr_t *b, int gb,
					 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[2] = scale_expr (va, sb);
}

static void
vga3_yz_zx_xy_geom_x_y_z (const expr_t **c,
						  const expr_t *a, int ga, const expr_t *b, int gb,
						  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = cross_expr (vb, va);
	c[1] = dot_expr (va, vb);
}

static void
vga3_yz_zx_xy_geom_yz_zx_xy (const expr_t **c,
							 const expr_t *a, int ga, const expr_t *b, int gb,
							 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = neg_expr (dot_expr (va, vb));
}

static void
vga3_yz_zx_xy_geom_xyz (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[0] = neg_expr (scale_expr (va, sb));
}

static void
vga3_xyz_geom_x_y_z (const expr_t **c,
					 const expr_t *a, int ga, const expr_t *b, int gb,
					 algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = scale_expr (vb, sa);
}

static void
vga3_xyz_geom_yz_zx_xy (const expr_t **c,
						const expr_t *a, int ga, const expr_t *b, int gb,
						algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = neg_expr (scale_expr (vb, sa));
}

static void
vga3_xyz_geom_xyz (const expr_t **c,
				   const expr_t *a, int ga, const expr_t *b, int gb,
				   algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (sb, sa));
}

static pga_func vga3_geometric_funcs[6][6] = {
	[0] = {
		[0] = vga3_x_y_z_geom_x_y_z,
		[1] = vga3_x_y_z_geom_xyz,
		[2] = vga3_x_y_z_geom_yz_zx_xy,
		[3] = scale_component,
	},
	[1] = {
		[0] = vga3_xyz_geom_x_y_z,
		[1] = vga3_xyz_geom_xyz,
		[2] = vga3_xyz_geom_yz_zx_xy,
		[3] = vga3_xyz_scale,
	},
	[2] = {
		[0] = vga3_yz_zx_xy_geom_x_y_z,
		[1] = vga3_yz_zx_xy_geom_xyz,
		[2] = vga3_yz_zx_xy_geom_yz_zx_xy,
		[3] = scale_component,
	},
	[3] = {
		[0] = scale_component,
		[1] = vga3_xyz_scale,
		[2] = scale_component,
		[3] = scale_component,
	},
};

static void
component_geometric (const expr_t **c,
					 const expr_t *a, int ga, const expr_t *b, int gb,
					 algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		if (pga3_geometric_funcs[ga][gb]) {
			pga3_geometric_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		if (pga2_geometric_funcs[ga][gb]) {
			pga2_geometric_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else if (p == 3 && m == 0 && z == 0) {
		if (vga3_geometric_funcs[ga][gb]) {
			vga3_geometric_funcs[ga][gb] (c, a, ga, b, gb, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static const expr_t *
geometric_product (const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	const expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				const expr_t *w[layout->count] = {};
				component_geometric (w, a[i], i, b[j], j, algebra);
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	auto mvec = mvec_gather (c, algebra);
	return mvec;
}

static const expr_t *
regressive_product (const expr_t *e1, const expr_t *e2)
{
	auto a = algebra_dual (e1);
	if (is_error (a)) {
		return a;
	}
	auto b = algebra_dual (e2);
	if (is_error (a)) {
		return b;
	}
	auto c = outer_product (a, b);
	return algebra_undual (c);
}

static const expr_t *
multivector_sum (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	const expr_t *c[layout->count];
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);
	component_sum (op, c, a, b, algebra);
	return mvec_gather (c, algebra);
}

static const expr_t *
commutator_product (const expr_t *e1, const expr_t *e2)
{
	auto ab = geometric_product (e1, e2);
	auto ba = geometric_product (e2, e1);
	auto sub = multivector_sum ('-', ab, ba);
	sub = algebra_optimize (sub);
	return binary_expr ('/', sub, new_int_expr (2, false));
}

static bool
is_two (const expr_t *e)
{
	if (is_integral_val (e)) {
		return expr_integral (e) == 2;
	}
	if (is_floating_val (e)) {
		return expr_floating (e) == 2;
	}
	return false;
}

static const expr_t *
multivector_divide (const expr_t *e1, const expr_t *e2)
{
	if (is_algebra (get_type (e2))) {
		return error (e2, "Division is left-right ambiguous (and works only"
					  " for versors anyway). Use explicit reversion and divide"
					  " by square magnitude instead.");
	}
	if (!is_algebra (get_type (e1))) {
		internal_error (e1, "wtw?");
	}
	auto t1 = get_type (e1);
	auto algebra = algebra_get (t1);
	auto layout = &algebra->layout;
	auto stype = algebra->type;
	const expr_t *a[layout->count] = {};
	bool is_half = is_two (e2);
	e1 = mvec_expr (e1, algebra);
	e2 = promote_scalar (algebra->type, e2);
	mvec_scatter (a, e1, algebra);

	bool neg = is_neg (e2);
	if (neg) {
		e2 = neg_expr (e2);
	}
	for (int i = 0; i < layout->count; i++) {
		if (!a[i]) {
			continue;
		}
		auto den = e2;
		bool sign = neg;
		if (is_neg (a[i])) {
			sign = !sign;
			a[i] = neg_expr (a[i]);
		}
		auto ext = a[i];
		if (is_ext (a[i])) {
			a[i] = a[i]->extend.src;
		} else {
			ext = nullptr;
		}
		if (is_half && a[i]->type == ex_expr && a[i]->expr.op == '+'
			&& a[i]->expr.e1 == a[i]->expr.e2) {
			a[i] = a[i]->expr.e1;
		} else {
			auto ct = get_type (a[i]);
			int width = type_width (ct);
			if (width > 1) {
				den = ext_expr (den, vector_type (stype, width), 2, false);
			}
			a[i] = typed_binary_expr (ct, '/', a[i], den);
			a[i] = edag_add_expr (a[i]);
		}
		if (ext) {
			auto e = new_expr_copy (ext);
			e->extend.src = a[i];
			a[i] = e;
		}
		if (sign) {
			a[i] = neg_expr (a[i]);
		}
	}
	return mvec_gather (a, algebra);
}

static const expr_t *
component_compare (int op, const expr_t *e1, const expr_t *e2, algebra_t *alg)
{
	auto t = get_type (e1 ? e1 : e2);
	auto stype = alg->type;
	auto vtype = vector_type (stype, type_width (t));
	pr_type_t zero[type_size (vtype)] = {};
	if (!e1) {
		e1 = new_value_expr (new_type_value (vtype, zero), false);
	} else {
		e1 = offset_cast (vtype, e1, 0);
	}
	if (!e2) {
		e2 = new_value_expr (new_type_value (vtype, zero), false);
	} else {
		e2 = offset_cast (vtype, e2, 0);
	}
	return binary_expr (op, e1, e2);
}

static const expr_t *
algebra_compare (int op, const expr_t *e1, const expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	const expr_t *cmp = nullptr;
	expr_t *bool_label = nullptr;
	for (int i = 0; i < layout->count; i++) {
		if (a[i] || b[i]) {
			auto c = component_compare (op, a[i], b[i], algebra);
			if (cmp) {
				bool_label = new_label_expr ();
				cmp = bool_expr (QC_AND, bool_label, cmp, c);
			} else {
				cmp = c;
			}
		}
	}
	return cmp;
}

const expr_t *
algebra_binary_expr (int op, const expr_t *e1, const expr_t *e2)
{
	switch (op) {
		case QC_EQ:
		case QC_NE:
		case QC_LE:
		case QC_GE:
		case QC_LT:
		case QC_GT:
			return algebra_compare (op, e1, e2);
		case QC_DOT:
			return inner_product (e1, e2);
		case QC_WEDGE:
			return outer_product (e1, e2);
		case QC_REGRESSIVE:
			return regressive_product (e1, e2);
		case QC_CROSS:
			return commutator_product (e1, e2);
		case '+':
		case '-':
			return multivector_sum (op, e1, e2);
		case '/':
			return multivector_divide (e1, e2);
		case '*':
		case QC_GEOMETRIC:
			return geometric_product (e1, e2);
	}
	return error (e1, "invalid operator");
}

const expr_t *
algebra_negate (const expr_t *e)
{
	auto t = get_type (e);
	auto algebra = algebra_get (t);
	auto layout = &algebra->layout;
	const expr_t *n[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (n, e, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!n[i]) {
			continue;
		}
		n[i] = neg_expr (n[i]);
	}
	return mvec_gather (n, algebra);
}

static const expr_t *
hodge_dual (const expr_t *e, bool undual)
{
	auto algebra = algebra_context (get_type (e));
	if (!algebra) {
		return error (e, "cannot take the %s of a scalar without context",
					  undual ? "undual" : "dual");
	}
	e = algebra_optimize (e);
	auto layout = &algebra->layout;

	const expr_t *a[layout->count] = {};
	const expr_t *b[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (a, e, algebra);

	int dim = algebra->dimension - algebra->zero;
	pr_uint_t I_mask = (1u << algebra->dimension) - 1;
	for (int i = 0; i < layout->count; i++) {
		if (!a[i]) {
			continue;
		}
		auto group = &layout->groups[i];
		//FIXME assumes groups are mono-grade (either come up with something
		//or reject mixed-grade groups)
		auto blade = group->blades[0];
		pr_uint_t d_mask = I_mask ^ blade.mask;
		int dual_ind = layout->group_map[layout->mask_map[d_mask]].num;
		auto dual_group = &layout->groups[dual_ind];
		auto dual_type = algebra_float_type (algebra, dual_group->group_mask);
		auto dual = cast_expr (dual_type, a[i]);
		pr_uint_t flips = algebra_count_flips (algebra, blade.mask, d_mask);
		if (flips & 1) {
			dual = neg_expr (dual);
		}
		auto ct = algebra->mvec_types[1 << i];
		if (undual && ((dim * algebra_get_grade (ct)) & 1)) {
			dual = neg_expr (dual);
		}
		b[dual_ind] = dual;
	}

	return mvec_gather (b, algebra);
}

const expr_t *
algebra_dual (const expr_t *e)
{
	return hodge_dual (e, false);
}

const expr_t *
algebra_undual (const expr_t *e)
{
	return hodge_dual (e, true);
}

static void
set_sign (pr_type_t *val, int sign, const type_t *type)
{
	if (is_float (type)) {
		(*(float *) val) = sign;
	} else {
		(*(double *) val) = sign;
	}
}

const expr_t *
algebra_reverse (const expr_t *e)
{
	auto t = get_type (e);
	auto algebra = algebra_get (t);
	auto layout = &algebra->layout;
	const expr_t *r[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (r, e, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!r[i]) {
			continue;
		}
		auto ct = algebra->mvec_types[1 << i];
		if (is_mono_grade (ct)) {
			int grade = algebra_get_grade (ct);
			if (grade & 2) {
				r[i] = neg_expr (r[i]);
			}
		} else {
			auto group = &layout->groups[i];
			pr_type_t ones[group->count * type_size (algebra->type)];
			bool neg = false;
			for (int j = 0; j < group->count; j++) {
				int grade = algebra_blade_grade (group->blades[i]);
				int sign = grade & 2 ? -1 : 1;
				if (sign < 0) {
					neg = true;
				}
				set_sign (&ones[j * type_size (algebra->type)], sign, ct);
			}
			if (neg) {
				auto rev = new_value_expr (new_type_value (ct, ones), false);
				rev = edag_add_expr (rev);
				r[i] = typed_binary_expr (ct, QC_HADAMARD, r[i], rev);
				r[i] = edag_add_expr (rev);
			}
		}
	}
	return mvec_gather (r, algebra);
}

const expr_t *
algebra_cast_expr (const type_t *dstType, const expr_t *e)
{
	auto srcType = get_type (e);
	if (!(is_scalar (dstType) || is_nonscalar (dstType)
		  || is_algebra (dstType))
		|| !(is_scalar (srcType) || is_nonscalar (srcType)
			 || is_algebra (srcType))) {
		return cast_error (e, srcType, dstType);
	}

	auto algebra = algebra_get (dstType);
	auto srcAlgebra = algebra_get (srcType);
	if (!algebra && srcAlgebra) {
		e = algebra_optimize (e);
		srcType = get_type (e);
		srcAlgebra = algebra_get (srcType);
	}
	if (type_width (dstType) == type_width (srcType)) {
		if (type_same (base_type (dstType), base_type (srcType))) {
			auto alias = new_alias_expr (dstType, e);
			return edag_add_expr (fold_constants (alias));
		}
		if (is_algebra (srcType)) {
			algebra = algebra_get (srcType);
			auto alias = edag_add_expr (new_alias_expr (algebra->type, e));
			return cast_expr (dstType, alias);
		} else {
			auto type = vector_type (algebra->type, type_width (dstType));
			auto cast = cast_expr (type, e);
			cast = fold_constants (new_alias_expr (dstType, cast));
			return edag_add_expr (cast);
		}
	}

	if (!algebra || !srcAlgebra) {
		return cast_error (e, srcType, dstType);
	}
	if (algebra != srcAlgebra) {
		internal_error (e, "casts between different algebras not implemented");
	}
	pr_uint_t dst_mask = get_group_mask (dstType, algebra);
	pr_uint_t src_mask = get_group_mask (srcType, srcAlgebra);

	if (!(src_mask & ~dst_mask)) {
		// src is a strict subset of dst, so the cast is a no-op
		return e;
	}
	if (!(src_mask & dst_mask)) {
		// src and dst have no groups in common, so the result is 0
		warning (e, "cast is always 0");
		return edag_add_expr (new_zero_expr (algebra->type));
	}

	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	auto src = mvec_expr (e, algebra);
	mvec_scatter (a, src, algebra);

	pr_uint_t group_mask = dst_mask & src_mask;
	for (int i = 0; i < layout->count; i++) {
		if (!(group_mask & (1u << i))) {
			a[i] = 0;
		}
	}
	return mvec_gather (a, algebra);
}

static int __attribute__((const))
find_offset (const type_t *t1, const type_t *t2)
{
	return type_width (t1) - type_width (t2);
}

static bool __attribute__((const))
summed_extend (const expr_t *e)
{
	if (!(is_sum (e) && is_ext (e->expr.e1) && is_ext (e->expr.e2))) {
		return false;
	}
	auto ext1 = e->expr.e1->extend;
	auto ext2 = e->expr.e2->extend;
	pr_uint_t   bits1 = (1 << type_width (get_type (ext1.src))) - 1;
	pr_uint_t   bits2 = (1 << type_width (get_type (ext2.src))) - 1;
	if (ext1.reverse) {
		bits1 <<= type_width (ext1.type) - type_width (get_type (ext1.src));
	}
	if (ext2.reverse) {
		bits2 <<= type_width (ext2.type) - type_width (get_type (ext2.src));
	}
	return !(bits1 & bits2);
}

static const expr_t *
assign_extend (const expr_t *src)
{
	auto type = get_type (src);
	auto ext1 = src->expr.e1->extend;
	auto ext2 = src->expr.e2->extend;
	auto type1 = get_type (ext1.src);
	auto type2 = get_type (ext2.src);
	int offs1 = ext1.reverse ? find_offset (ext1.type, type1) : 0;
	int offs2 = ext2.reverse ? find_offset (ext2.type, type2) : 0;
	if (offs2 < offs1) {
		ext1 = src->expr.e2->extend;
		ext2 = src->expr.e1->extend;
		if (src->expr.op == '-') {
			ext1.src = neg_expr (ext1.src);
		}
	} else {
		if (src->expr.op == '-') {
			ext2.src = neg_expr (ext2.src);
		}
	}

	auto params = new_list_expr (nullptr);
	expr_prepend_expr (params, ext1.src);
	expr_prepend_expr (params, ext2.src);
	auto extend = constructor_expr (new_type_expr (type), params);
	return edag_add_expr (extend);
}

const expr_t *
algebra_compound_expr (const type_t *dstType, const expr_t *src)
{
	src = algebra_optimize (src);
	auto srcType = get_type (src);

	if (dstType->meta != ty_algebra && dstType != srcType) {
		return src;
	}
	auto algebra = algebra_get (dstType);

	pr_uint_t dstMask = get_group_mask (dstType, algebra);
	pr_uint_t srcMask = get_group_mask (srcType, algebra);
	//if ((~dstMask) & srcMask) {
	//	// dstType is smaller than srcType
	//	return type_mismatch (dst, src, '=');
	//}

	if (!(dstMask & ~srcMask) && !(srcMask & (srcMask - 1))) {
		return src;
	}

	auto layout = &algebra->layout;
	const expr_t *c[layout->count] = {};
	src = mvec_expr (src, algebra);
	mvec_scatter (c, src, algebra);

	auto comp = new_compound_init ();
	comp ->compound.type_expr = new_type_expr (dstType);
	for (auto sym = get_mvec_sym (dstType); sym; sym = sym->next) {
		pr_uint_t mask = get_group_mask (sym->type, algebra);
		int group = BITOP_LOG2 (mask);
		auto val = c[group];
		if (val) {
			if (summed_extend (val)) {
				val = assign_extend (val);
			}
			val = new_alias_expr (sym->type, val);
			auto des = new_designator (sym, nullptr);
			auto ele = new_element (val, des);
			append_element (comp, ele);
		}
	}
	return comp;
}

const expr_t *
algebra_field_expr (const expr_t *mvec, const expr_t *field_name)
{
	auto mvec_type = get_type (mvec);
	if (is_reference (mvec_type)) {
		mvec_type = dereference_type (mvec_type);
	}
	auto algebra = algebra_get (mvec_type);

	auto field_sym = get_name (field_name);
	if (!field_sym) {
		return error (field_name, "multi-vector reference is not a name");
	}
	auto mvec_struct = get_mvec_struct (mvec_type);
	auto field = mvec_struct ? symtab_lookup (mvec_struct, field_sym->name) : 0;
	if (!field) {
		mvec_struct = algebra->mvec_sym->type->symtab;
		field = symtab_lookup (mvec_struct, field_sym->name);
		if (field) {
			debug (field_name, "'%s' not in sub-type '%s' of '%s', "
				   "returning zero of type '%s'", field_sym->name,
				   mvec_type->name, algebra->mvec_sym->type->name,
				   mvec_type->name);
			return edag_add_expr (new_zero_expr (field->type));
		}
		return error (field_name, "'%s' has no member named '%s'",
					  mvec_type->name, field_sym->name);
	}
	auto layout = &algebra->layout;
	const expr_t *a[layout->count] = {};
	mvec_scatter (a, mvec_expr (mvec, algebra), algebra);
	pr_uint_t group_mask = get_group_mask (field->type, algebra);
	for (int i = 0; i < layout->count; i++) {
		if (!(group_mask & (1u << i))) {
			a[i] = 0;
		}
	}
	return mvec_gather (a, algebra);
}
