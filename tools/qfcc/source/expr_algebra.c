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
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

static int __attribute__((pure))
get_group (type_t *type, algebra_t *algebra)
{
	auto layout = &algebra->layout;
	if (is_scalar (type)) {
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
sum_expr (type_t *type, int op, expr_t *a, expr_t *b)
{
	auto sum = new_binary_expr (op, a, b);
	sum->e.expr.type = type;
	return sum;
}

static expr_t *
scale_expr (type_t *type, expr_t *a, expr_t *b)
{
	if (!is_scalar (get_type (b)) || !is_real (get_type (b))) {
		internal_error (b, "not a real scalar type");
	}
	if (!is_real (get_type (a))) {
		internal_error (b, "not a real scalar type");
	}
	int  op = is_scalar (get_type (a)) ? '*' : SCALE;

	auto scale = new_binary_expr (op, a, b);
	scale->e.expr.type = type;
	return fold_constants (scale);
}

static expr_t *
dot_expr (type_t *type, expr_t *a, expr_t *b)
{
	auto vec_type = get_type (a);
	auto dot = new_binary_expr (DOT, a, b);
	dot->e.expr.type = vector_type (vec_type, type_width (vec_type));
	dot = array_expr (dot, new_short_expr (0));
	dot->e.expr.type = type;
	return dot;
}

static expr_t *
cross_expr (type_t *type, expr_t *a, expr_t *b)
{
	auto cross = new_binary_expr (CROSS, a, b);
	cross->e.expr.type = type;
	return cross;
}

typedef void (*pga_func) (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg);
static void
scale_component (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	if (!is_scalar (get_type (b))) {
		auto t = a;
		a = b;
		b = t;
	}
	if (is_scalar (get_type (a))) {
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
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	c[2] = dot_expr (dot_type, va, vb);
}

static void
pga3_x_y_z_w_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto va = new_offset_alias_expr (vtype, a, 0);
	c[2] = cross_expr (dot_type, b, va);
}

static void
pga3_x_y_z_w_dot_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto cs = dot_expr (stype, b, va);
	auto blade = new_value_expr (algebra_blade_value (alg, "e0"));
	c[0] = scale_expr (get_type (blade), blade, unary_expr ('-', cs));
}

static void
pga3_x_y_z_w_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto dot_type = algebra_mvec_type (alg, 0x20);
	auto va = new_offset_alias_expr (dot_type, new_swizzle_expr (a, "xyz0"), 0);
	auto sb = new_offset_alias_expr (stype, b, 0);
	c[5] = scale_expr (dot_type, va, sb);
}

#define pga3_wzy_wxz_wyx_xyz_dot_x_y_z_w pga3_x_y_z_w_dot_wzy_wxz_wyx_xyz
static void
pga3_x_y_z_w_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								  algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto bvtype = algebra_mvec_type (alg, 0x02);
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto va = new_offset_alias_expr (bvtype, a, 0);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto sb = new_offset_alias_expr (stype, b, 3);
	c[1] = scale_expr (bvtype, va, sb);
	c[3] = cross_expr (bmtype, vb, va);
}

static void
pga3_yz_zx_xy_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto vb = new_offset_alias_expr (vtype, a, 0);
	c[2] = cross_expr (dot_type, a, vb);
}

static void
pga3_yz_zx_xy_dot_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto dot_type = algebra_mvec_type (alg, 0x04);
	c[2] = unary_expr ('-', dot_expr (dot_type, a, b));
}

#define pga3_wxyz_dot_yz_zx_xy pga3_yz_zx_xy_dot_wxyz
static void
pga3_yz_zx_xy_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto bmtype = algebra_mvec_type (alg, 0x08);
	auto va = new_offset_alias_expr (bmtype, a, 0);
	auto sb = new_offset_alias_expr (stype, b, 0);
	c[3] = scale_expr (bmtype, va, unary_expr ('-', sb));
}

#define pga3_wzy_wxz_wyx_xyz_dot_yz_zx_xy pga3_yz_zx_xy_dot_wzy_wxz_wyx_xyz
static void
pga3_yz_zx_xy_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
								   algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto dot_type = algebra_mvec_type (alg, 0x01);

	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto sb = new_offset_alias_expr (stype, b, 3);

	auto cv = scale_expr (dot_type, a, sb);
	auto cs = dot_expr (stype, a, vb);

	auto tmp = new_temp_def_expr (dot_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto stmp = new_offset_alias_expr (stype, tmp, 3);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, cv));
	append_expr (block, assign_expr (stmp, cs));
	c[0] = block;
}

static void
pga3_wx_wy_wz_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto cs = dot_expr (stype, b, va);
	auto blade = new_value_expr (algebra_blade_value (alg, "e0"));
	c[0] = scale_expr (get_type (blade), blade, cs);
}

static void
pga3_wxyz_dot_x_y_z_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto dot_type = algebra_mvec_type (alg, 0x20);
	auto vb = new_offset_alias_expr (dot_type,
									 new_swizzle_expr (b, "-x-y-z0"), 0);
	auto sa = new_offset_alias_expr (stype, a, 0);
	c[5] = scale_expr (dot_type, vb, sa);
}

static void
pga3_wxyz_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = new_offset_alias_expr (stype, a, 0);
	auto sb = new_offset_alias_expr (stype, b, 3);
	auto blade = new_value_expr (algebra_blade_value (alg, "e0"));
	sb = unary_expr ('-', sb);
	c[0] = scale_expr (get_type (blade), blade, scale_expr (stype, sa, sb));
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wxyz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = new_offset_alias_expr (stype, a, 0);
	auto sb = new_offset_alias_expr (stype, b, 3);
	auto blade = new_value_expr (algebra_blade_value (alg, "e0"));
	c[0] = scale_expr (get_type (blade), blade, scale_expr (stype, sa, sb));
}

static void
pga3_wzy_wxz_wyx_xyz_dot_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
										  algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = new_offset_alias_expr (stype, a, 3);
	auto sb = new_offset_alias_expr (stype, b, 3);
	c[0] = unary_expr ('-', scale_expr (stype, sa, sb));
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
	auto sa = new_offset_alias_expr (stype, a, 2);
	auto sb = new_offset_alias_expr (stype, b, 2);

	c[1] = unary_expr ('-', scale_expr (stype, sa, sb));
}

static void
pga2_yw_wx_xy_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto dot_type = algebra_mvec_type (alg, 0x04);
	auto va = new_offset_alias_expr (dot_type, new_swizzle_expr (a, "y-x0"), 0);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto tmp = cross_expr (dot_type, a, b);
	tmp = new_offset_alias_expr (dot_type, new_swizzle_expr (tmp, "00z"), 0);
	c[2] = sum_expr (dot_type, '+', scale_expr (dot_type, va, sb), tmp);
}

static void
pga2_x_y_w_dot_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto dot_type = algebra_mvec_type (alg, 0x04);
	auto va = new_offset_alias_expr (dot_type, new_swizzle_expr (a, "-yx0"), 0);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto tmp = cross_expr (dot_type, a, b);
	tmp = new_offset_alias_expr (dot_type, new_swizzle_expr (tmp, "00-z"), 0);
	c[2] = sum_expr (dot_type, '+', scale_expr (dot_type, va, sb), tmp);
}

static void
pga2_x_y_w_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto cs = dot_expr (stype, va, vb);
	c[1] = cs;
}

static void
pga2_x_y_w_dot_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto cv = scale_expr (dot_type, va, b);
	auto tmp = new_temp_def_expr (dot_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, cv));
	c[0] = block;
}

static void
pga2_wxy_dot_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto dot_type = algebra_mvec_type (alg, 0x01);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto cv = scale_expr (dot_type, vb, a);
	auto tmp = new_temp_def_expr (dot_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, unary_expr ('-', cv)));
	c[0] = block;
}

static pga_func pga2_dot_funcs[4][4] = {
	[0] = {
		[0] = pga2_yw_wx_xy_dot_yw_wx_xy,
		[2] = pga2_yw_wx_xy_dot_x_y_w,
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
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto sa = new_offset_alias_expr (stype, a, 3);
	auto sb = new_offset_alias_expr (stype, b, 3);
	c[1] = cross_expr (algebra_mvec_type (alg, 0x02), va, vb);
	c[3] = sum_expr (wedge_type, '-', scale_expr (wedge_type, vb, sa),
									  scale_expr (wedge_type, va, sb));
}

#define pga3_yz_zx_xy_wedge_x_y_z_w pga3_x_y_z_w_wedge_yz_zx_xy
static void
pga3_x_y_z_w_wedge_yz_zx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto sa = new_offset_alias_expr (stype, a, 3);
	auto cv = scale_expr (get_type (b), b, sa);
	auto cs = dot_expr (stype, va, b);
	auto tmp = new_temp_def_expr (wedge_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto stmp = new_offset_alias_expr (stype, tmp, 3);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, unary_expr ('-', cv)));
	append_expr (block, assign_expr (stmp, cs));
	c[5] = block;
}

// vector-bivector wedge is commutative
#define pga3_wx_wy_wz_wedge_x_y_z_w pga3_x_y_z_w_wedge_wx_wy_wz
static void
pga3_x_y_z_w_wedge_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 3);
	auto wedge_type = algebra_mvec_type (alg, 0x20);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto cv = cross_expr (vtype, va, b);
	auto cs = new_zero_expr (stype);
	auto tmp = new_temp_def_expr (wedge_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto stmp = new_offset_alias_expr (stype, tmp, 3);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, cv));
	append_expr (block, assign_expr (stmp, cs));
	c[5] = block;
}

static void
pga3_x_y_z_w_wedge_wzy_wxz_wyx_xyz (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), a, b);
}

// bivector-bivector wedge is commutative
#define pga3_wx_wy_wz_wedge_yz_zx_xy pga3_yz_zx_xy_wedge_wx_wy_wz
static void
pga3_yz_zx_xy_wedge_wx_wy_wz (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	c[4] = dot_expr (algebra_mvec_type (alg, 0x10), a, b);
}

static void
pga3_wzy_wxz_wyx_xyz_wedge_x_y_z_w (expr_t **c, expr_t *a, expr_t *b,
									algebra_t *alg)
{
	c[4] = unary_expr ('-', dot_expr (algebra_mvec_type (alg, 0x10), a, b));
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
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static pga_func pga3_geometric_funcs[6][6] = {
	[0] = { },
	[1] = { },
	[2] = {
		[0] = scale_component,
		[1] = scale_component,
		[2] = scale_component,
		[3] = scale_component,
		[4] = scale_component,
		[5] = scale_component,
	},
	[3] = { },
	[4] = { },
	[5] = { },
};

static void
pga2_yw_wx_xy_geom_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto sa = new_offset_alias_expr (stype, a, 2);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto cv = cross_expr (geom_type, b, a);

	c[0] = new_offset_alias_expr (geom_type, new_swizzle_expr (cv, "xy0"), 0);
	c[1] = unary_expr ('-', scale_expr (algebra_mvec_type (alg, 0x02), sa, sb));
}

static void
pga2_yw_wx_xy_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto geom_type = algebra_mvec_type (alg, 0x04);
	auto va = new_offset_alias_expr (geom_type,
									 new_swizzle_expr (a, "y-x0"), 0);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto tmp = cross_expr (geom_type, a, b);
	tmp = new_offset_alias_expr (geom_type, new_swizzle_expr (tmp, "00z"), 0);
	c[2] = sum_expr (geom_type, '+', scale_expr (geom_type, va, sb), tmp);
	c[3] = dot_expr (algebra_mvec_type (alg, 0x08), a, b);
}

#define pga2_wxy_geom_yw_wx_xy pga2_yw_wx_xy_geom_wxy
static void
pga2_yw_wx_xy_geom_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto sa = new_offset_alias_expr (stype, a, 0);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto blade = new_value_expr (algebra_blade_value (alg, "e0"));
	sb = unary_expr ('-', sb);
	c[0] = scale_expr (get_type (blade), blade, scale_expr (stype, sa, sb));
}

static void
pga2_x_y_w_geom_yw_wx_xy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto geom_type = algebra_mvec_type (alg, 0x04);
	auto va = new_offset_alias_expr (geom_type,
									 new_swizzle_expr (a, "-yx0"), 0);
	auto sb = new_offset_alias_expr (stype, b, 2);
	auto tmp = cross_expr (geom_type, a, b);
	tmp = new_offset_alias_expr (geom_type, new_swizzle_expr (tmp, "00-z"), 0);
	c[2] = sum_expr (geom_type, '+', scale_expr (geom_type, va, sb), tmp);
	c[3] = dot_expr (algebra_mvec_type (alg, 0x08), a, b);
}

static void
pga2_x_y_w_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	c[1] = dot_expr (stype, va, vb);
	c[0] = cross_expr (geom_type, a, b);
}

static void
pga2_x_y_w_geom_wxy (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto va = new_offset_alias_expr (vtype, a, 0);
	auto cv = scale_expr (geom_type, va, b);

	auto tmp = new_temp_def_expr (geom_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, cv));
	c[0] = block;
}

static void
pga2_wxy_geom_x_y_w (expr_t **c, expr_t *a, expr_t *b, algebra_t *alg)
{
	auto stype = alg->type;
	auto vtype = vector_type (stype, 2);
	auto geom_type = algebra_mvec_type (alg, 0x01);
	auto vb = new_offset_alias_expr (vtype, b, 0);
	auto cv = scale_expr (geom_type, vb, a);

	auto tmp = new_temp_def_expr (geom_type);
	auto vtmp = new_offset_alias_expr (vtype, tmp, 0);
	auto block = new_block_expr ();
	block->e.block.result = tmp;
	append_expr (block, assign_expr (vtmp, cv));
	c[0] = block;
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
				component_sum ('+', c, c, w, algebra);
			}
		}
	}
	return mvec_gather (c, algebra);
}

static expr_t *
regressive_product (expr_t *e1, expr_t *e2)
{
	internal_error (e1, "not implemented");
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
