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

#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

static int __attribute__((pure))
get_group (type_t *type, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	if (is_scalar (type) && !is_algebra (type)) {
		return layout->group_map[layout->mask_map[0]][0];
	}
	if (!is_algebra (type)) {
		internal_error (0, "non-algebra type");
	}
	pr_uint_t group_mask = (1u << (layout->count + 1)) - 1;
	if (type->type != ev_invalid) {
		group_mask = type->t.multivec->group_mask;
	}
	if (group_mask & (group_mask - 1)) {
		internal_error (0, "multi-group mult-vector");
	}
	return BITOP_LOG2 (group_mask);
}

static bool __attribute__((const))
is_neg (const expr_t *e)
{
	return e->type == ex_uexpr && e->e.expr.op == '-';
}

static expr_t *
ext_expr (expr_t *src, type_t *type, int extend, bool reverse)
{
	if (!src) {
		return 0;
	}
	return new_extend_expr (src, type, extend, reverse);
}

static bool __attribute__((const))
anti_com (const expr_t *e)
{
	return (e->type == ex_expr
			&& (e->e.expr.op == CROSS || e->e.expr.op == WEDGE
				|| e->e.expr.op == '-'));
}

static expr_t *
neg_expr (expr_t *e)
{
	if (!e) {
		// propagated zero
		return 0;
	}
	if (is_neg (e)) {
		return e->e.expr.e1;
	}
	auto type = get_type (e);
	if (anti_com (e)) {
		e = new_binary_expr (e->e.expr.op, e->e.expr.e2, e->e.expr.e1);
	} else {
		e = new_unary_expr ('-', e);
	}
	e->e.expr.type = type;
	return fold_constants (e);
}

static expr_t *
alias_expr (type_t *type, expr_t *e, int offset)
{
	if (type == get_type (e)) {
		if (offset) {
			internal_error (e, "offset alias to same type");
		}
		return e;
	}
	bool neg = false;
	if (is_neg (e)) {
		neg = true;
		e = neg_expr (e);
	}
	e = new_offset_alias_expr (type, e, offset);
	if (neg) {
		e = neg_expr (e);
	}
	return e;
}

static expr_t *
offset_cast (type_t *type, expr_t *expr, int offset)
{
	if (type->meta != ty_basic) {
		internal_error (expr, "offset cast to non-basic type");
	}
	if (expr->type == ex_expr && expr->e.expr.op == '+') {
		auto e1 = expr->e.expr.e1;
		auto e2 = expr->e.expr.e2;
		if (e1->type == ex_extend) {
			auto ext = e1->e.extend;
			if (type_width (get_type (ext.src)) == type_width (type)) {
				return alias_expr (type, ext.src, 0);
			}
		}
		if (e2->type == ex_extend) {
			auto ext = e2->e.extend;
			if (type_width (get_type (ext.src)) == type_width (type)) {
				return alias_expr (type, ext.src, 0);
			}
			if (offset >= type_width (get_type (ext.src))) {
				return 0;
			}
		}
	}
	if (expr->type == ex_extend) {
		auto ext = expr->e.extend;
		if (type_width (get_type (ext.src)) == type_width (type)) {
			return alias_expr (type, ext.src, 0);
		}
		if (offset >= type_width (get_type (ext.src))) {
			return 0;
		}
	}
	offset *= type_size (base_type (get_type (expr)));
	return alias_expr (type, expr, offset);
}

static symbol_t *
get_mvec_sym (type_t *type)
{
	symbol_t   *sym = 0;
	if (type->type == ev_invalid) {
		sym = type->t.algebra->mvec_sym;
	} else {
		sym = type->t.multivec->mvec_sym;
	}
	return sym->type->t.symtab->symbols;
}

static bool
check_types (expr_t **e, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	for (int i = 0; i < layout->count; i++) {
		if (!e[i]) {
			continue;
		}
		auto type = algebra_mvec_type (algebra, 1u << i);
		if (get_type (e[i]) != type) {
			return false;
		}
	}
	return true;
}

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
	for (auto sym = get_mvec_sym (mvtype); sym; sym = sym->next) {
		*c = new_offset_alias_expr (sym->type, expr, sym->s.offset);
		c = &(*c)->next;
		mvec->e.multivec.count++;
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
		if (!is_algebra (ct)) {
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
	if (!count) {
		return new_zero_expr (algebra->type);
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

static bool __attribute__((const))
ext_compat (const expr_t *a, const expr_t *b)
{
	return (a->e.extend.extend == b->e.extend.extend
			&& a->e.extend.reverse == b->e.extend.reverse
			&& a->e.extend.type == b->e.extend.type);
}

static expr_t *
sum_expr (type_t *type, expr_t *a, expr_t *b)
{
	if (!a) {
		return b;
	}
	if (!b) {
		return a;
	}
	if (a->type != ex_extend && b->type == ex_extend) {
		// ensure always ext + something
		return sum_expr (type, b, a);
	}
	if (a->type == ex_extend) {
		if (b->type == ex_extend) {
			if (ext_compat (a, b)) {
				auto ext = a->e.extend;
				a = a->e.extend.src;
				b = b->e.extend.src;
				auto sum = sum_expr (get_type (a), a, b);
				return ext_expr (sum, type, ext.extend, ext.reverse);
			}
		} else if (b->type == ex_expr && b->e.expr.op == '+') {
			auto c = b->e.expr.e1;
			auto d = b->e.expr.e2;
			if (ext_compat (a, c)) {
				// d should not be compatible with a because it should have
				// already been merged
				auto ext = a->e.extend;
				a = a->e.extend.src;
				c = c->e.extend.src;
				auto sum = sum_expr (get_type (a), a, c);
				sum = ext_expr (sum, type, ext.extend, ext.reverse);
				return sum_expr (type, sum, d);
			}
			if (ext_compat (a, d)) {
				// c should not be compatible with a because it should have
				// already been merged
				auto ext = a->e.extend;
				a = a->e.extend.src;
				d = d->e.extend.src;
				auto sum = sum_expr (get_type (a), a, d);
				sum = ext_expr (sum, type, ext.extend, ext.reverse);
				return sum_expr (type, sum, c);
			}
		}
	}
	bool neg = false;
	if ((is_neg (a) && (is_neg (b) || anti_com (b)))
		|| (anti_com (a) && is_neg (b))) {
		neg = true;
		a = neg_expr (a);
		b = neg_expr (b);
	}
	int op = '+';
	if (is_neg (a) && b->type != ex_extend) {
		auto t = a;
		a = b;
		b = t;
		op = '-';
		b = neg_expr (b);
	} else if (a->type != ex_extend && is_neg (b)) {
		op = '-';
		b = neg_expr (b);
	}
	auto sum = new_binary_expr (op, a, b);
	sum->e.expr.type = type;
	if (neg) {
		sum = neg_expr (sum);
	}
	return sum;
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
			auto sum_type = get_type (a[i]);
			if (op == '+') {
				c[i] = sum_expr (sum_type, a[i], b[i]);
			} else {
				c[i] = sum_expr (sum_type, a[i], neg_expr (b[i]));
			}
			c[i] = fold_constants (c[i]);
		} else if (a[i]) {
			c[i] = a[i];
		} else if (b[i]) {
			if (op == '+') {
				c[i] = b[i];
			} else {
				c[i] = neg_expr (b[i]);
				c[i] = fold_constants (c[i]);
			}
		} else {
			c[i] = 0;
		}
	}
}

static expr_t *
scale_expr (type_t *type, expr_t *a, expr_t *b)
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
	int  op = is_scalar (get_type (a)) ? '*' : SCALE;

	bool neg = false;
	if (is_neg (a)) {
		neg = !neg;
		a = neg_expr (a);
	}
	if (is_neg (b)) {
		neg = !neg;
		b = neg_expr (b);
	}

	auto scale = new_binary_expr (op, a, b);
	scale->e.expr.type = type;
	scale = fold_constants (scale);
	if (neg) {
		scale = neg_expr (scale);
		scale->e.expr.type = type;
		scale = fold_constants (scale);
	}
	return scale;
}

static expr_t *
dot_expr (type_t *type, expr_t *a, expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}
	bool neg = false;
	if (is_neg (a)) {
		neg = !neg;
		a = neg_expr (a);
	}
	if (is_neg (b)) {
		neg = !neg;
		b = neg_expr (b);
	}

	auto dot = new_binary_expr (DOT, a, b);
	dot->e.expr.type = type;
	if (neg) {
		dot = neg_expr (dot);
		dot->e.expr.type = type;
	}
	return dot;
}

static expr_t *
cross_expr (type_t *type, expr_t *a, expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}
	bool neg = false;
	if (is_neg (a)) {
		neg = !neg;
		a = neg_expr (a);
	}
	if (is_neg (b)) {
		neg = !neg;
		b = neg_expr (b);
	}
	if (neg) {
		auto t = a;
		a = b;
		b = t;
	}
	auto cross = new_binary_expr (CROSS, a, b);
	cross->e.expr.type = type;
	return cross;
}

static expr_t *
wedge_expr (type_t *type, expr_t *a, expr_t *b)
{
	if (!a || !b) {
		// propagated zero
		return 0;
	}
	bool neg = false;
	if (is_neg (a)) {
		neg = !neg;
		a = neg_expr (a);
	}
	if (is_neg (b)) {
		neg = !neg;
		b = neg_expr (b);
	}
	if (neg) {
		auto t = a;
		a = b;
		b = t;
	}
	auto cross = new_binary_expr (WEDGE, a, b);
	cross->e.expr.type = type;
	return cross;
}

typedef void (*pga_func) (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg);
static void
scale_component (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	if (is_algebra (get_type (b))) {
		auto t = a;
		a = b;
		b = t;
	}
	if (!is_algebra (get_type (a))) {
		a = promote_scalar (alg->type, a);
	}
	auto scale_type = get_type (a);
	b = promote_scalar (alg->type, b);
	auto scale = scale_expr (scale_type, a, b);
	int  group = get_group (scale_type, alg);
	c[group] = scale;
}

static void
pga3_x_y_z_w_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x04);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[2] = dot_expr (dot_type, va, vb);
}

static void
pga3_x_y_z_w_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	c[0] = ext_expr (cross_expr (vtype, b, va), dot_type, 0, false);
}

static void
pga3_x_y_z_w_dot_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto cs = neg_expr (dot_expr (stype, b, va));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_x_y_z_w_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	auto cv = scale_expr (vtype, va, sb);
	c[5] = ext_expr (cv, algebra_mvec_type (alg, 0x20), 0, false);
}

static void
pga3_x_y_z_w_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto bvtype = algebra_mvec_type (alg, 0x02);
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	c[1] = scale_expr (bvtype, va, sb);
	c[3] = cross_expr (bmtype, vb, va);
}

static void
pga3_yz_zx_xy_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto vb = offset_cast (vtype, b, 0);
	c[0] = ext_expr (cross_expr (vtype, vb, a), dot_type, 0, false);
}

static void
pga3_yz_zx_xy_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto dot_type = algebra_mvec_type (alg, 0x04);
	c[2] = neg_expr (dot_expr (dot_type, a, b));
}

static void
pga3_yz_zx_xy_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (bmtype, va, sb));
}

static void
pga3_yz_zx_xy_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);

	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);

	auto cv = scale_expr (vtype, a, sb);
	auto cs = dot_expr (stype, a, vb);

	cv = ext_expr (neg_expr (cv), dot_type, 0, false);
	cs = ext_expr (cs, dot_type, 0, true);
	c[0] = sum_expr (dot_type, cv, cs);
}

static void
pga3_wx_wy_wz_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (stype, a, vb);
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_wxyz_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto dot_type = algebra_mvec_type (alg, 0x20);
	auto vb = new_swizzle_expr (b, "-x-y-z0");
	auto sa = offset_cast (stype, a, 0);
	c[5] = scale_expr (dot_type, vb, sa);
}

static void
pga3_wxyz_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[3] = neg_expr (scale_expr (bmtype, vb, sa));
}

static void
pga3_wxyz_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cs = neg_expr (scale_expr (stype, sa, sb));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b,
								  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto bvtype = algebra_mvec_type (alg, 0x02);
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto sa = offset_cast (stype, a, 3);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = scale_expr (bvtype, vb, sa);
	c[3] = cross_expr (bmtype, va, vb);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);

	auto va = offset_cast (vtype, a, 0);
	auto sa = offset_cast (stype, a, 3);

	auto cv = scale_expr (vtype, b, sa);
	auto cs = dot_expr (stype, b, va);

	cv = ext_expr (neg_expr (cv), dot_type, 0, false);
	cs = ext_expr (cs, dot_type, 0, true);
	c[0] = sum_expr (dot_type, cv, cs);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 0);
	auto cs = scale_expr (stype, sa, sb);
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
										  algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[0] = neg_expr (scale_expr (stype, sa, sb));
}

static pga_func pga3_dot_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_dot_x_y_z_w,
		[1] = pga3_x_y_z_w_dot_yz_zx_xy,
		[2] = scale_component,
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
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = scale_component,
		[5] = scale_component,
	},
	[3] = {
		[0] = pga3_wx_wy_wz_dot_x_y_z_w,
		[2] = scale_component,
	},
	[4] = {
		[0] = pga3_wxyz_dot_x_y_z_w,
		[1] = pga3_wxyz_dot_yz_zx_xy,
		[2] = scale_component,
		[5] = pga3_wxyz_dot_wzy_wxz_wyx_xyz,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_dot_x_y_z_w,
		[1] = pga3_wzy_wxz_wyx_xyz_dot_yz_zx_xy,
		[2] = scale_component,
		[4] = pga3_wzy_wxz_wyx_xyz_dot_wxyz,
		[5] = pga3_wzy_wxz_wyx_xyz_dot_wzy_wxz_wyx_xyz,
	},
};

static void
pga2_yw_wx_xy_dot_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 2);

	c[1] = neg_expr (scale_expr (stype, sa, sb));
}

static void
pga2_yw_wx_xy_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x04);
	auto va = offset_cast (wtype, a, 0);
	auto sa = offset_cast (stype, a, 2);
	auto vb = offset_cast (wtype, b, 0);
	auto cv = new_swizzle_expr (vb, "y-x");
	auto cs = wedge_expr (stype, vb, va);
	cv = ext_expr (scale_expr (wtype, cv, sa), vtype, 0, false);
	cs = ext_expr (cs, dot_type, 0, true);
	c[2] = sum_expr (dot_type, cv, cs);
}

static void
pga2_yw_wx_xy_dot_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 0);
	auto cs = neg_expr (scale_expr (stype, sa, sb));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga2_x_y_w_dot_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto dot_type = algebra_mvec_type (alg, 0x04);
	auto va = offset_cast (wtype, a, 0);
	auto vb = offset_cast (wtype, b, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cv = scale_expr (wtype, new_swizzle_expr (va, "-yx"), sb);
	auto cs = wedge_expr (stype, vb, va);
	cv = ext_expr (cv, dot_type, 0, false);
	cs = ext_expr (cs, dot_type, 0, true);
	c[2] = sum_expr (dot_type, cs, cv);
}

static void
pga2_x_y_w_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (stype, va, vb);
	c[1] = cs;
}

static void
pga2_x_y_w_dot_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto cv = scale_expr (vtype, va, b);

	c[0] = ext_expr (cv, dot_type, 0, false);
}

static void
pga2_wxy_dot_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cs = neg_expr (scale_expr (stype, sa, sb));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga2_wxy_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = scale_expr (vtype, vb, sa);
	c[0] = ext_expr (cv, dot_type, 0, false);
}

static pga_func pga2_dot_funcs[4][4] = {
	[0] = {
		[0] = pga2_yw_wx_xy_dot_yw_wx_xy,
		[2] = pga2_yw_wx_xy_dot_x_y_w,
		[3] = pga2_yw_wx_xy_dot_wxy,
	},
	[1] = {
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
	},
	[2] = {
		[0] = pga2_x_y_w_dot_yw_wx_xy,
		[2] = pga2_x_y_w_dot_x_y_w,
		[3] = pga2_x_y_w_dot_wxy,
	},
	[3] = {
		[0] = pga2_wxy_dot_yw_wx_xy,
		[2] = pga2_wxy_dot_x_y_w,
	},
};

static void
component_dot (expr_t **c, expr_t *a, expr_t *b, algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga3_dot_funcs[ga][gb]) {
			pga3_dot_funcs[ga][gb] (c, a, b, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga2_dot_funcs[ga][gb]) {
			pga2_dot_funcs[ga][gb] (c, a, b, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static expr_t *
inner_product (expr_t *e1, expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	expr_t *a[layout->count] = {};
	expr_t *b[layout->count] = {};
	expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				expr_t *w[layout->count] = {};
				component_dot (w, a[i], b[j], algebra);
				if (!check_types (w, algebra)) {
					internal_error (a[i], "wrong types in dot product");
				}
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static void
pga3_x_y_z_w_wedge_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x08);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[1] = cross_expr (algebra_mvec_type (alg, 0x02), va, vb);
	c[3] = sum_expr (wedge_type,
					 scale_expr (wedge_type, vb, sa),
					 neg_expr (scale_expr (wedge_type, va, sb)));
}

static void
pga3_x_y_z_w_wedge_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vtype, b, sa);
	auto cs = dot_expr (stype, va, b);

	cv = ext_expr (neg_expr (cv), wedge_type, 0, false);
	cs = ext_expr (cs, wedge_type, 0, true);
	c[5] = sum_expr (wedge_type, cv, cs);
}

static void
pga3_x_y_z_w_wedge_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto cv = cross_expr (vtype, va, b);
	c[5] = ext_expr (cv, wedge_type, 0, false);
}

static void
pga3_x_y_z_w_wedge_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), a, b);
}

static void
pga3_yz_zx_xy_wedge_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (vtype, a, sb);
	auto cs = dot_expr (stype, vb, a);

	cv = ext_expr (neg_expr (cv), wedge_type, 0, false);
	cs = ext_expr (cs, wedge_type, 0, true);
	c[5] = sum_expr (wedge_type, cv, cs);
}

// bivector-bivector wedge is commutative
#define pga3_wx_wy_wz_wedge_yz_zx_xy pga3_yz_zx_xy_wedge_wx_wy_wz
static void
pga3_yz_zx_xy_wedge_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), a, b);
}

static void
pga3_wx_wy_wz_wedge_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = cross_expr (vtype, vb, a);
	c[5] = ext_expr (cv, wedge_type, 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_wedge_x_y_z_w (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	c[4] = neg_expr (dot_expr (algebra_mvec_type (alg, 0x10), a, b));
}

static pga_func pga3_wedge_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_wedge_x_y_z_w,
		[1] = pga3_x_y_z_w_wedge_yz_zx_xy,
		[2] = scale_component,
		[3] = pga3_x_y_z_w_wedge_wx_wy_wz,
		[5] = pga3_x_y_z_w_wedge_wzy_wxz_wyx_xyz,
	},
	[1] = {
		[0] = pga3_yz_zx_xy_wedge_x_y_z_w,
		[2] = scale_component,
		[3] = pga3_yz_zx_xy_wedge_wx_wy_wz,
	},
	[2] = {
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = scale_component,
		[5] = scale_component,
	},
	[3] = {
		[0] = pga3_wx_wy_wz_wedge_x_y_z_w,
		[1] = pga3_wx_wy_wz_wedge_yz_zx_xy,
		[2] = scale_component,
	},
	[4] = {
		[2] = scale_component,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_wedge_x_y_z_w,
		[2] = scale_component,
	},
};

// vector-bivector wedge is commutative
#define pga2_x_y_w_wedge_yw_wx_xy pga2_yw_wx_xy_wedge_x_y_w
static void
pga2_yw_wx_xy_wedge_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[3] = dot_expr (algebra_mvec_type (alg, 0x08), a, b);
}

static void
pga2_x_y_w_wedge_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto wedge_type = algebra_mvec_type (alg, 0x01);
	c[0] = cross_expr (wedge_type, a, b);
}

static pga_func pga2_wedge_funcs[4][4] = {
	[0] = {
		[1] = scale_component,
		[2] = pga2_yw_wx_xy_wedge_x_y_w,
	},
	[1] = {
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
	},
	[2] = {
		[0] = pga2_x_y_w_wedge_yw_wx_xy,
		[1] = scale_component,
		[2] = pga2_x_y_w_wedge_x_y_w,
	},
	[3] = {
		[1] = scale_component,
	},
};

static void
component_wedge (expr_t **c, expr_t *a, expr_t *b, algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga3_wedge_funcs[ga][gb]) {
			pga3_wedge_funcs[ga][gb] (c, a, b, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga2_wedge_funcs[ga][gb]) {
			pga2_wedge_funcs[ga][gb] (c, a, b, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static expr_t *
outer_product (expr_t *e1, expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	expr_t *a[layout->count] = {};
	expr_t *b[layout->count] = {};
	expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				expr_t *w[layout->count] = {};
				component_wedge (w, a[i], b[j], algebra);
				if (!check_types (w, algebra)) {
					internal_error (a[i], "wrong types in wedge product");
				}
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static void
pga3_x_y_z_w_geom_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x08);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[2] = dot_expr (stype, va, vb);
	c[1] = cross_expr (algebra_mvec_type (alg, 0x02), va, vb);
	c[3] = sum_expr (geom_type,
					 scale_expr (geom_type, vb, sa),
					 neg_expr (scale_expr (geom_type, va, sb)));
}

static void
pga3_x_y_z_w_geom_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vtype, b, sa);
	auto cs = dot_expr (stype, va, b);
	c[0] = ext_expr (cross_expr (vtype, b, va),
							algebra_mvec_type (alg, 0x01), 0, false);
	cv = ext_expr (neg_expr (cv), geom_type, 0, false);
	cs = ext_expr (cs, geom_type, 0, true);
	c[5] = sum_expr (geom_type, cv, cs);
}

static void
pga3_x_y_z_w_geom_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto cs = neg_expr (dot_expr (stype, va, b));
	c[5] = ext_expr (cross_expr (vtype, va, b), geom_type, 0, false);
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_x_y_z_w_geom_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto va = offset_cast (vtype, a, 0);
	auto sb = offset_cast (stype, b, 0);
	auto cv = scale_expr (vtype, va, sb);
	c[5] = ext_expr (cv, geom_type, 0, false);
}

static void
pga3_x_y_z_w_geom_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x10);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	c[1] = scale_expr (algebra_mvec_type (alg, 0x02), va, sb);
	c[3] = cross_expr (algebra_mvec_type (alg, 0x08), vb, va);
	c[4] = dot_expr (geom_type, a, b);
}

static void
pga3_yz_zx_xy_geom_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (vtype, a, sb);
	auto cs = dot_expr (stype, vb, a);
	c[0] = ext_expr (cross_expr (vtype, vb, a),
							algebra_mvec_type (alg, 0x01), 0, false);
	cv = ext_expr (neg_expr (cv), geom_type, 0, false);
	cs = ext_expr (cs, geom_type, 0, true);
	c[5] = sum_expr (geom_type, cv, cs);
}

static void
pga3_yz_zx_xy_geom_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[1] = cross_expr (algebra_mvec_type (alg, 0x02), b, a);
	c[2] = neg_expr (dot_expr (algebra_mvec_type (alg, 0x04), b, a));
}

static void
pga3_yz_zx_xy_geom_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[3] = cross_expr (algebra_mvec_type (alg, 0x08), b, a);
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), b, a);
}

static void
pga3_yz_zx_xy_geom_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sb = offset_cast (stype, b, 0);
	c[3] = neg_expr (scale_expr (algebra_mvec_type (alg, 0x08), a, sb));
}

static void
pga3_yz_zx_xy_geom_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto vb = offset_cast (vtype, b, 0);
	auto sb = offset_cast (stype, b, 3);
	auto cv = scale_expr (vtype, a, sb);
	auto cs = dot_expr (stype, vb, a);
	cv = ext_expr (neg_expr (cv), geom_type, 0, false);
	cs = ext_expr (cs, geom_type, 0, true);
	c[0] = sum_expr (geom_type, cv, cs);
	c[5] = ext_expr (cross_expr (vtype, vb, a),
							algebra_mvec_type (alg, 0x20), 0, false);
}

static void
pga3_wx_wy_wz_geom_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto vb = offset_cast (vtype, b, 0);
	auto cs = dot_expr (stype, vb, a);
	c[5] = ext_expr (cross_expr (vtype, vb, a), geom_type, 0, false);
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga3_wx_wy_wz_geom_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[3] = cross_expr (algebra_mvec_type (alg, 0x08), b, a);
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), a, b);
}

static void
pga3_wx_wy_wz_geom_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto vs = offset_cast (stype, b, 3);
	auto cv = neg_expr (scale_expr (vtype, a, vs));
	c[5] = ext_expr (cv, geom_type, 0, false);
}

static void
pga3_wxyz_geom_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto sa = offset_cast (stype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = neg_expr (scale_expr (vtype, vb, sa));
	c[5] = ext_expr (cv, geom_type, 0, false);
}

static void
pga3_wxyz_geom_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	c[3] = neg_expr (scale_expr (algebra_mvec_type (alg, 0x08), b, sa));
}

static void
pga3_wxyz_geom_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 3);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto cs = scale_expr (stype, sa, sb);
	c[0] = ext_expr (neg_expr (cs), geom_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_x_y_z_w (expr_t **c, expr_t *a, expr_t *b,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x10);
	auto va = offset_cast (vtype, a, 0);
	auto sa = offset_cast (stype, a, 3);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = scale_expr (algebra_mvec_type (alg, 0x02), vb, sa);
	c[3] = cross_expr (algebra_mvec_type (alg, 0x08), va, vb);
	c[4] = neg_expr (dot_expr (geom_type, b, a));
}

static void
pga3_wzy_wxz_wyx_xyz_geom_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vtype, b, sa);
	auto cs = dot_expr (stype, va, b);
	cv = ext_expr (neg_expr (cv), geom_type, 0, false);
	cs = ext_expr (cs, geom_type, 0, true);
	c[0] = sum_expr (geom_type, cv, cs);
	c[5] = ext_expr (cross_expr (vtype, b, va),
							algebra_mvec_type (alg, 0x20), 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x20);
	auto sa = offset_cast (stype, a, 3);
	auto cv = scale_expr (vtype, b, sa);
	c[5] = ext_expr (cv, geom_type, 0, false);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wxyz (expr_t **c, expr_t *a, expr_t *b,
								algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 0);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto cs = scale_expr (stype, sa, sb);
	c[0] = ext_expr (cs, geom_type, 0, true);
}

static void
pga3_wzy_wxz_wyx_xyz_geom_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
										   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x08);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	auto sa = offset_cast (stype, a, 3);
	auto sb = offset_cast (stype, b, 3);
	c[2] = neg_expr (scale_expr (stype, sa, sb));
	c[3] = sum_expr (geom_type,
					 scale_expr (geom_type, va, sb),
					 neg_expr (scale_expr (geom_type, vb, sa)));
}

static pga_func pga3_geometric_funcs[6][6] = {
	[0] = {
		[0] = pga3_x_y_z_w_geom_x_y_z_w,
		[1] = pga3_x_y_z_w_geom_yz_zx_xy,
		[2] = scale_component,
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
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = scale_component,
		[5] = scale_component,
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
		[2] = scale_component,
		[5] = pga3_wxyz_geom_wzy_wxz_wyx_xyz,
	},
	[5] = {
		[0] = pga3_wzy_wxz_wyx_xyz_geom_x_y_z_w,
		[1] = pga3_wzy_wxz_wyx_xyz_geom_yz_zx_xy,
		[2] = scale_component,
		[3] = pga3_wzy_wxz_wyx_xyz_geom_wx_wy_wz,
		[4] = pga3_wzy_wxz_wyx_xyz_geom_wxyz,
		[5] = pga3_wzy_wxz_wyx_xyz_geom_wzy_wxz_wyx_xyz,
	},
};

static void
pga2_yw_wx_xy_geom_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto ctype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 2);
	auto cv = alias_expr (ctype, cross_expr (vtype, b, a), 0);

	c[0] = ext_expr (cv, geom_type, 0, false);
	c[1] = neg_expr (scale_expr (algebra_mvec_type (alg, 0x02), sa, sb));
}

static void
pga2_yw_wx_xy_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x04);
	auto va = offset_cast (wtype, a, 0);
	auto sa = offset_cast (stype, a, 2);
	auto vb = offset_cast (wtype, b, 0);
	auto cv = new_swizzle_expr (vb, "y-x");
	auto cs = wedge_expr (stype, vb, va);
	cs = ext_expr (cs, geom_type, 0, true);
	cv = ext_expr (scale_expr (wtype, cv, sa), vtype, 0, false);
	c[2] = sum_expr (geom_type, cv, cs);
	c[3] = dot_expr (algebra_mvec_type (alg, 0x08), a, b);
}

static void
pga2_yw_wx_xy_geom_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 2);
	auto sb = offset_cast (stype, b, 0);
	auto cs = neg_expr (scale_expr (stype, sa, sb));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga2_x_y_w_geom_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto wtype = vector_type (stype, 2);
	auto vtype = vector_type (stype, 3);
	auto geom_type = algebra_mvec_type (alg, 0x04);
	auto va = offset_cast (wtype, a, 0);
	auto vb = offset_cast (wtype, b, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cv = new_swizzle_expr (va, "-yx");
	auto cs = wedge_expr (stype, vb, va);
	cs = ext_expr (cs, geom_type, 0, true);
	cv = ext_expr (scale_expr (wtype, cv, sb), vtype, 0, false);
	c[2] = sum_expr (geom_type, cv, cs);
	c[3] = dot_expr (algebra_mvec_type (alg, 0x08), a, b);
}

static void
pga2_x_y_w_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto vb = offset_cast (vtype, b, 0);
	c[1] = dot_expr (stype, va, vb);
	c[0] = cross_expr (geom_type, a, b);
}

static void
pga2_x_y_w_geom_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto va = offset_cast (vtype, a, 0);
	auto cv = scale_expr (vtype, va, b);

	c[0] = ext_expr (cv, geom_type, 0, false);
}

static void
pga2_wxy_geom_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = offset_cast (stype, a, 0);
	auto sb = offset_cast (stype, b, 2);
	auto cs = neg_expr (scale_expr (stype, sa, sb));
	c[0] = ext_expr (cs, algebra_mvec_type (alg, 0x01), 0, true);
}

static void
pga2_wxy_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto vb = offset_cast (vtype, b, 0);
	auto cv = scale_expr (vtype, vb, a);

	c[0] = ext_expr (cv, geom_type, 0, false);
}

static pga_func pga2_geometric_funcs[6][6] = {
	[0] = {
		[0] = pga2_yw_wx_xy_geom_yw_wx_xy,
		[1] = scale_component,
		[2] = pga2_yw_wx_xy_geom_x_y_w,
		[3] = pga2_yw_wx_xy_geom_wxy,
	},
	[1] = {
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
	},
	[2] = {
		[0] = pga2_x_y_w_geom_yw_wx_xy,
		[1] = scale_component,
		[2] = pga2_x_y_w_geom_x_y_w,
		[3] = pga2_x_y_w_geom_wxy,
	},
	[3] = {
		[0] = pga2_wxy_geom_yw_wx_xy,
		[1] = scale_component,
		[2] = pga2_wxy_geom_x_y_w,
	},
};

static void
component_geometric (expr_t **c, expr_t *a, expr_t *b, algebra_t *algebra)
{
	int         p = algebra->plus;
	int         m = algebra->minus;
	int         z = algebra->zero;

	if (p == 3 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga3_geometric_funcs[ga][gb]) {
			pga3_geometric_funcs[ga][gb] (c, a, b, algebra);
		}
	} else if (p == 2 && m == 0 && z == 1) {
		int ga = get_group (get_type (a), algebra);
		int gb = get_group (get_type (b), algebra);
		if (pga2_geometric_funcs[ga][gb]) {
			pga2_geometric_funcs[ga][gb] (c, a, b, algebra);
		}
	} else {
		internal_error (a, "not implemented");
	}
}

static expr_t *
geometric_product (expr_t *e1, expr_t *e2)
{
	auto t1 = get_type (e1);
	auto t2 = get_type (e2);
	auto algebra = is_algebra (t1) ? algebra_get (t1) : algebra_get (t2);
	auto layout = &algebra->layout;
	expr_t *a[layout->count] = {};
	expr_t *b[layout->count] = {};
	expr_t *c[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = mvec_expr (e2, algebra);
	mvec_scatter (a, e1, algebra);
	mvec_scatter (b, e2, algebra);

	for (int i = 0; i < layout->count; i++) {
		for (int j = 0; j < layout->count; j++) {
			if (a[i] && b[j]) {
				expr_t *w[layout->count] = {};
				component_geometric (w, a[i], b[j], algebra);
				if (!check_types (w, algebra)) {
					internal_error (a[i], "wrong types in geometric product");
				}
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static expr_t *
regressive_product (expr_t *e1, expr_t *e2)
{
	auto a = algebra_dual (e1);
	auto b = algebra_dual (e2);
	auto c = outer_product (a, b);
	return algebra_dual (c);
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
commutator_product (expr_t *e1, expr_t *e2)
{
	auto ab = geometric_product (e1, e2);
	auto ba = geometric_product (e2, e1);
	return multivector_sum ('-', ab, ba);
}

static expr_t *
multivector_divide (expr_t *e1, expr_t *e2)
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
	expr_t *a[layout->count] = {};
	e1 = mvec_expr (e1, algebra);
	e2 = promote_scalar (algebra->type, e2);
	mvec_scatter (a, e1, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!a[i]) {
			continue;
		}
		auto den = e2;
		auto ct = get_type (a[i]);
		int width = type_width (ct);
		if (width > 1) {
			den = ext_expr (den, vector_type (stype, width), 2, false);
		}
		a[i] = new_binary_expr ('/', a[i], den);
		a[i]->e.expr.type = ct;
	}
	return mvec_gather (a, algebra);
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
	auto t = get_type (e);
	auto algebra = algebra_get (t);
	auto layout = &algebra->layout;
	expr_t *n[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (n, e, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!n[i]) {
			continue;
		}
		auto ct = get_type (n[i]);
		if (is_algebra (ct)) {
			n[i] = cast_expr (float_type (ct), n[i]);
		}
		n[i] = neg_expr (n[i]);
		n[i]->e.expr.type = ct;
	}
	return mvec_gather (n, algebra);
}

expr_t *
algebra_dual (expr_t *e)
{
	if (!is_algebra (get_type (e))) {
		//FIXME check for being in an @algebra { } block
		return error (e, "cannot take the dual of a scalar without context");
	}
	auto algebra = algebra_get (get_type (e));
	auto layout = &algebra->layout;

	expr_t *a[layout->count] = {};
	expr_t *b[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (a, e, algebra);

	pr_uint_t I_mask = (1u << algebra->dimension) - 1;
	for (int i = 0; i < layout->count; i++) {
		if (!a[i]) {
			continue;
		}
		auto group = &layout->groups[i];
		//FIXME assumes groups are mono-grade (either come up with something
		//or reject mixed-grade groups)
		pr_uint_t mask = I_mask ^ group->blades[0].mask;
		int dual_ind = layout->group_map[layout->mask_map[mask]][0];
		auto dual_group = &layout->groups[dual_ind];
		auto dual_type = algebra_mvec_type (algebra, dual_group->group_mask);
		auto dual = cast_expr (dual_type, a[i]);
		if (algebra_count_flips (algebra, group->blades[0].mask, mask) & 1) {
			b[dual_ind] = neg_expr (dual);
		} else {
			b[dual_ind] = dual;
		}
	}

	return mvec_gather (b, algebra);
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

expr_t *
algebra_reverse (expr_t *e)
{
	auto t = get_type (e);
	auto algebra = algebra_get (t);
	auto layout = &algebra->layout;
	expr_t *r[layout->count] = {};
	e = mvec_expr (e, algebra);
	mvec_scatter (r, e, algebra);

	for (int i = 0; i < layout->count; i++) {
		if (!r[i]) {
			continue;
		}
		auto ct = get_type (r[i]);
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
				auto rev = new_value_expr (new_type_value (ct, ones));
				r[i] = new_binary_expr (HADAMARD, r[i], rev);
				r[i]->e.expr.type = ct;
			}
		}
	}
	return mvec_gather (r, algebra);
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
	auto base = alias_expr (&type_int, dst, memset_base);
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
		if (srcType == dstType) {
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

	auto sym = get_mvec_sym (dstType);
	auto block = new_block_expr ();
	int  memset_base = 0;
	for (int i = 0; i < layout->count; i++) {
		if (!components[i]) {
			continue;
		}
		while (sym->type != get_type (components[i])) {
			sym = sym->next;
		}
		int size = sym->s.offset - memset_base;
		if (size) {
			zero_components (block, dst, memset_base, size);
		}
		auto dst_alias = new_offset_alias_expr (sym->type, dst, sym->s.offset);
		append_expr (block, new_assign_expr (dst_alias, components[i]));
		memset_base = sym->s.offset + type_size (sym->type);
	}
	if (type_size (dstType) - memset_base) {
		zero_components (block, dst, memset_base,
						 type_size (dstType) - memset_base);
	}
	return block;
}
