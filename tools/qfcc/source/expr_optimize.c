/*
	expr_optimize.c

	algebraic expression optimization

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

#include "QF/heapsort.h"
#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static const expr_t *optimize_core (const expr_t *expr);
static const expr_t skip;

static int
clean_skips (const expr_t **expr_list)
{
	auto dst = expr_list;
	int count = 0;
	for (auto src = dst; *src; src++) {
		if (*src != &skip) {
			*dst++ = *src;
			count++;
		}
	}
	*dst = nullptr;
	return count;
}

static const expr_t *
rescale (const expr_t *expr, const expr_t *target, const expr_t *remove)
{
	if (expr == target) {
		if (target->expr.e1 == remove) {
			return target->expr.e2;
		}
		return target->expr.e1;
	}
	if (!is_scale (expr)) {
		internal_error (expr, "not a scale expression");
	}
	auto scale = expr->expr.e2;
	return scale_expr (rescale (expr->expr.e1, target, remove), scale);
}

static const expr_t *
remult (const expr_t *expr, const expr_t *remove)
{
	if (expr == remove) {
		return nullptr;
	}
	if (!is_mult (expr)) {
		internal_error (expr, "not a mult expression");
	}
	int count = count_factors (expr);
	const expr_t *factors[count + 1] = {};
	scatter_factors (expr, factors);
	for (auto f = factors; *f; f++) {
		if (*f == remove) {
			*f = &skip;
			break;
		}
	}
	clean_skips (factors);
	auto type = get_type (expr);
	auto new = gather_factors (expr->expr.op, type, factors, count - 1);
	return new;
}

static const expr_t *
remult_scale (const expr_t *expr, const expr_t *remove)
{
	auto mult = remult (expr->expr.e2, remove);
	auto scalee = expr->expr.e1;
	if (!mult) {
		return scalee;
	}
	auto type = get_type (expr);
	auto new = typed_binary_expr (type, QC_SCALE, scalee, mult);
	return edag_add_expr (new);
}

static const expr_t *
optimize_cross (const expr_t *expr, const expr_t **adds, const expr_t **subs)
{
	auto l = traverse_scale (expr)->expr.e1;
	auto r = traverse_scale (expr)->expr.e2;
	int l_count = 0;
	int r_count = 0;
	for (auto search = adds; *search; search++) {
		if (*search != &skip) {
			auto c = traverse_scale (*search);
			if (is_cross (c)) {
				l_count += c->expr.e1 == l;
				l_count += c->expr.e2 == l;
				r_count += c->expr.e1 == r;
				r_count += c->expr.e2 == r;
			}
		}
	}
	for (auto search = subs; *search; search++) {
		if (*search != &skip) {
			auto c = traverse_scale (*search);
			if (is_cross (c)) {
				l_count += c->expr.e1 == l;
				l_count += c->expr.e2 == l;
				r_count += c->expr.e1 == r;
				r_count += c->expr.e2 == r;
			}
		}
	}
	if (!(l_count + r_count)) {
		return expr;
	}
	bool right = r_count > l_count;
	int count = right ? r_count : l_count;
	auto com = right ? r : l;
	const expr_t *com_adds[count + 2] = {};
	const expr_t *com_subs[count + 2] = {};
	auto adst = com_adds;
	auto sdst = com_subs;

	*adst++ = rescale (expr, traverse_scale (expr), com);

	for (auto search = adds; *search; search++) {
		if (*search != &skip) {
			auto c = traverse_scale (*search);
			const expr_t *scale = nullptr;
			bool neg = false;
			if (is_cross (c)) {
				if (c->expr.e1 == com) {
					neg = right;
					scale = rescale (*search, c, com);
				} else if (c->expr.e2 == com) {
					neg = !right;
					scale = rescale (*search, c, com);
				}
			}
			if (scale) {
				*search = &skip;
				if (neg) {
					*sdst++ = scale;
				} else {
					*adst++ = scale;
				}
			}
		}
	}
	for (auto search = subs; *search; search++) {
		if (*search != &skip) {
			auto c = traverse_scale (*search);
			const expr_t *scale = nullptr;
			bool neg = false;
			if (is_cross (c)) {
				if (c->expr.e1 == com) {
					neg = right;
					scale = rescale (*search, c, com);
				} else if (c->expr.e2 == com) {
					neg = !right;
					scale = rescale (*search, c, com);
				}
			}
			if (scale) {
				*search = &skip;
				if (neg) {
					*adst++ = scale;
				} else {
					*sdst++ = scale;
				}
			}
		}
	}

	auto type = get_type (com);
	auto col = gather_terms (type, com_adds, com_subs);
	if (!col || !(col = optimize_core (col))) {
		return nullptr;
	}
	if (is_neg (col)) {
		col = neg_expr (col);
		right = !right;
	}
	col = optimize_core (col);
	const expr_t *cross;
	if (right) {
		cross = typed_binary_expr (type, QC_CROSS, col, com);
	} else {
		cross = typed_binary_expr (type, QC_CROSS, com, col);
	}
	cross = edag_add_expr (cross);
	return cross;
}

static bool
perm_swapped (const expr_t **a, const expr_t **b)
{
	// try to rotate b such that a[0] == b[0]
	// if a[0] is not in b, then b will be in its original state and the
	// next test will fail as desired
	for (int i = 0; i < 3; i++) {
		if (a[0] == b[0]) {
			break;
		}
		auto t = b[0];
		b[0] = b[1];
		b[1] = b[2];
		b[2] = t;
	}
	if (a[0] == b[0] && a[1] == b[2] && a[2] == b[1]) {
		return true;
	}
	return false;
}

static bool
perm_ordered (const expr_t **a, const expr_t **b)
{
	// try to rotate b such that a[0] == b[0]
	// if a[0] is not in b, then b will be in its original state and the
	// next test will fail as desired
	for (int i = 0; i < 3; i++) {
		if (a[0] == b[0]) {
			break;
		}
		auto t = b[0];
		b[0] = b[1];
		b[1] = b[2];
		b[2] = t;
	}
	if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
		return true;
	}
	return false;
}

static bool
check_dot_anticom (const expr_t *a, const expr_t *b)
{
	if (!is_anticommute (a) || !is_anticommute (b)) {
		return false;
	}
	if (a->expr.e1 != b->expr.e2) {
		return false;
	}
	if (a->expr.e2 != b->expr.e1) {
		return false;
	}
	return true;
}

static const expr_t *
optimize_dot (const expr_t *expr, const expr_t **adds, const expr_t **subs)
{
	auto l = expr->expr.e1;
	auto r = expr->expr.e2;
	if (l == r) {
		l = optimize_core (l);
		auto type = get_type (expr);
		auto e = typed_binary_expr (type, QC_DOT, l, l);
		e = fold_constants (e);
		return edag_add_expr (e);
	} else {
		l = optimize_core (l);
		r = optimize_core (r);
		if (!l || !r || reject_dot (l, r)) {
			return &skip;
		}
	}
	int factor = 1;
	const expr_t *a[3] = {
		is_cross (l) ? l->expr.e1 : l,
		is_cross (l) ? l->expr.e2 : is_cross (r) ? r->expr.e1 : nullptr,
		is_cross (r) ? r->expr.e2 : r,
	};
	for (auto search = adds; *search; search++) {
		if (*search != &skip) {
			auto c = *search;
			if (is_dot (c)) {
				auto c_l = c->expr.e1;
				auto c_r = c->expr.e2;
				const expr_t *b[3] = {
					is_cross (c_l) ? c_l->expr.e1 : c_l,
					is_cross (c_l) ? c_l->expr.e2 : is_cross (c_r)
								   ? c_r->expr.e1 : nullptr,
					is_cross (c_r) ? c_r->expr.e2 : c_r,
				};
				// a.bxc + c.bxa etc
				if (a[1] && b[1] && perm_swapped (a, b)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
				if (a[1] && b[1] && perm_ordered (a, b)) {
					*search = &skip;
					factor += 1;
					continue;
				}
				// check for a.boc + a.cob or boc.a + cob.a (o = anti-com op)
				// also (axb).(cxd) + (axb).(dxc) and similar
				if (c_l == l && check_dot_anticom (c_r, r)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
				if (c_r == l && check_dot_anticom (c_l, r)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
				if (c_l == r && check_dot_anticom (c_r, l)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
				if (c_r == r && check_dot_anticom (c_l, l)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
			}
		}
	}
	for (auto search = subs; *search; search++) {
		if (*search != &skip) {
			auto c = *search;
			if (is_dot (c)) {
				auto c_l = c->expr.e1;
				auto c_r = c->expr.e2;
				const expr_t *b[3] = {
					is_cross (c_l) ? c_l->expr.e1 : c_l,
					is_cross (c_l) ? c_l->expr.e2 : is_cross (c_r)
								   ? c_r->expr.e1 : nullptr,
					is_cross (c_r) ? c_r->expr.e2 : c_r,
				};
				// a.bxc + c.bxa etc
				if (a[1] && b[1] && perm_swapped (a, b)) {
					*search = &skip;
					factor += 1;
					continue;
				}
				if (a[1] && b[1] && perm_ordered (a, b)) {
					*search = &skip;
					factor -= 1;
					continue;
				}
				// check for a.boc + a.cob or boc.a + cob.a (o = anti-com op)
				// also (axb).(cxd) + (axb).(dxc) and similar
				if (c_l == l && check_dot_anticom (c_r, r)) {
					*search = &skip;
					factor += 1;
					continue;
				}
				if (c_r == l && check_dot_anticom (c_l, r)) {
					*search = &skip;
					factor += 1;
					continue;
				}
				if (c_l == r && check_dot_anticom (c_r, l)) {
					*search = &skip;
					factor += 1;
					continue;
				}
				if (c_r == r && check_dot_anticom (c_l, l)) {
					*search = &skip;
					factor += 1;
					continue;
				}
			}
		}
	}
	if (!factor) {
		return &skip;
	}
	bool neg = factor < 0;
	if (neg) {
		factor = -factor;
	}

	if (factor > 1) {
		auto type = get_type (expr);
		auto fact = cast_expr (base_type (type),
							   new_int_expr (factor, false));
		expr = typed_binary_expr (type, '*', expr, fact);
		expr = fold_constants (expr);
		expr = edag_add_expr (expr);
	} else {
		auto type = get_type (expr);
		expr = typed_binary_expr (type, QC_DOT, l, r);
		expr = fold_constants (expr);
		expr = edag_add_expr (expr);
	}
	if (neg) {
		expr = neg_expr (expr);
	}
	return expr;
}

static bool __attribute__((pure))
mult_has_factor (const expr_t *mult, const expr_t *factor)
{
	if (mult == factor) {
		return true;
	}
	if (!is_mult (mult)) {
		return false;
	}
	if (mult->expr.e1 == factor || mult->expr.e2 == factor) {
		return true;
	}
	bool has_factor = mult_has_factor (mult->expr.e1, factor);
	if (!has_factor) {
		has_factor = mult_has_factor (mult->expr.e2, factor);
	}
	return has_factor;
}

static bool __attribute__((pure))
has_const_factor (const expr_t *mult)
{
	if (is_constant (mult)) {
		return true;
	}
	if (!is_mult (mult)) {
		return false;
	}
	if (is_constant (mult->expr.e1) || is_constant (mult->expr.e2)) {
		return true;
	}
	bool has_const = has_const_factor (mult->expr.e1);
	if (!has_const) {
		has_const = has_const_factor (mult->expr.e2);
	}
	return has_const;
}

static const expr_t *
optimize_scale (const expr_t *expr, const expr_t **adds, const expr_t **subs)
{
	auto mult = expr->expr.e2;
	int num_factors = count_factors (mult);
	int total = 0;
	int fac_counts[num_factors + 1] = {};
	const expr_t *factors[num_factors + 2] = {};
	if (is_mult (mult)) {
		scatter_factors (mult, factors);
	} else {
		factors[0] = mult;
	}
	for (auto f = factors; *f; f++) {
		*f = optimize_core (*f);
	}

	for (auto search = adds; *search; search++) {
		if (is_scale (*search)) {
			for (auto f = factors; *f; f++) {
				if (mult_has_factor ((*search)->expr.e2, *f)) {
					fac_counts[f - factors]++;
					total++;
				}
			}
		}
	}
	for (auto search = subs; *search; search++) {
		if (is_scale (*search)) {
			for (auto f = factors; *f; f++) {
				if (mult_has_factor ((*search)->expr.e2, *f)) {
					fac_counts[f - factors]++;
					total++;
				}
			}
		}
	}
	if (!total) {
		return expr;
	}

	const expr_t *common = nullptr;
	int count = 0;
	for (auto f = factors; *f; f++) {
		if (fac_counts[f - factors] > count
			|| (fac_counts[f - factors] == count && is_constant (*f))) {
			common = *f;
			count = fac_counts[f - factors];
		}
	}

	const expr_t *com_adds[count + 2] = {};
	const expr_t *com_subs[count + 2] = {};
	auto dst = com_adds;
	*dst++ = remult_scale (expr, common);
	for (auto src = adds; *src; src++) {
		if (is_scale (*src) && mult_has_factor ((*src)->expr.e2, common)) {
			*dst++ = remult_scale (*src, common);
			*src = &skip;
		}
	}
	dst = com_subs;
	for (auto src = subs; *src; src++) {
		if (is_scale (*src) && mult_has_factor ((*src)->expr.e2, common)) {
			*dst++ = remult_scale (*src, common);
			*src = &skip;
		}
	}

	auto type = get_type (expr);
	auto scale = expr->expr.e1;
	auto col = gather_terms (type, com_adds, com_subs);
	if (!col || !(col = optimize_core (col))) {
		return nullptr;
	}

	scale = typed_binary_expr (type, QC_SCALE, col, common);
	scale = edag_add_expr (scale);
	return scale;
}

static const expr_t *
optimize_mult (const expr_t *expr, const expr_t **adds, const expr_t **subs)
{
	int num_factors = count_factors (expr);
	int total = 0;
	int fac_counts[num_factors + 1] = {};
	const expr_t *factors[num_factors + 2] = {};
	if (is_mult (expr)) {
		scatter_factors (expr, factors);
	} else {
		factors[0] = expr;
	}
	for (auto f = factors; *f; f++) {
		*f = optimize_core (*f);
	}

	for (auto search = adds; *search; search++) {
		if (is_mult (*search)) {
			for (auto f = factors; *f; f++) {
				if (mult_has_factor (*search, *f)) {
					fac_counts[f - factors]++;
					total++;
				}
			}
		}
	}
	for (auto search = subs; *search; search++) {
		if (is_mult (*search)) {
			for (auto f = factors; *f; f++) {
				if (mult_has_factor (*search, *f)) {
					fac_counts[f - factors]++;
					total++;
				}
			}
		}
	}
	auto type = get_type (expr);
	if (!total) {
		expr = gather_factors ('*', type, factors, num_factors);
		return expr;
	}

	const expr_t *common = nullptr;
	int count = 0;
	for (auto f = factors; *f; f++) {
		if (fac_counts[f - factors] > count
			|| (fac_counts[f - factors] == count && is_constant (*f))) {
			common = *f;
			count = fac_counts[f - factors];
		}
	}

	const expr_t *com_adds[count + 2] = {};
	const expr_t *com_subs[count + 2] = {};
	auto dst = com_adds;
	*dst++ = remult (expr, common);
	for (auto src = adds; *src; src++) {
		if (is_mult (*src) && mult_has_factor (*src, common)) {
			*dst++ = remult (*src, common);
			*src = &skip;
		}
	}
	dst = com_subs;
	for (auto src = subs; *src; src++) {
		if (is_mult (*src) && mult_has_factor (*src, common)) {
			*dst++ = remult (*src, common);
			*src = &skip;
		}
	}

	auto col = gather_terms (type, com_adds, com_subs);
	if (!col || !(col = optimize_core (col))) {
		return &skip;
	}

	auto mult = typed_binary_expr (type, expr->expr.op, col, common);
	mult = edag_add_expr (mult);
	return mult;
}

static void
optimize_extends (const expr_t **expr_list)
{
	for (auto scan = expr_list; *scan; scan++) {
		if (is_ext (*scan)) {
			auto ext = (*scan)->extend;
			ext.src = optimize_core (ext.src);
			*scan = ext_expr (ext.src, ext.type, ext.extend, ext.reverse);
		}
	}
}

void
merge_extends (const expr_t **adds, const expr_t **subs)
{
	for (auto scan = adds; *scan; scan++) {
		if (!is_ext (*scan)) {
			continue;
		}
		auto extend = (*scan)->extend;
		auto dst = scan + 1;
		for (auto src = dst; *src; src++) {
			if (is_ext (*src) && ext_compat (&extend, &(*src)->extend)) {
				extend.src = sum_expr (extend.src, (*src)->extend.src);
			} else {
				*dst++ = *src;
			}
		}
		*dst = 0;
		dst = subs;
		for (auto src = dst; *src; src++) {
			if (is_ext (*src) && ext_compat (&extend, &(*src)->extend)) {
				extend.src = sum_expr (extend.src,
									   neg_expr ((*src)->extend.src));
			} else {
				*dst++ = *src;
			}
		}
		if (extend.src) {
			*scan = ext_expr (extend.src, extend.type,
							  extend.extend, extend.reverse);
		} else {
			*scan = &skip;
		}
		*dst = 0;
	}

	clean_skips (adds);
	clean_skips (subs);
}

static void
optimize_cross_products (const expr_t **adds, const expr_t **subs)
{
	for (auto scan = adds; *scan; scan++) {
		if (is_cross (traverse_scale (*scan))) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_cross (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_cross (traverse_scale (*scan))) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_cross (e, adds, subs);
		}
	}

	clean_skips (adds);
	clean_skips (subs);
}

static void
optimize_dot_products (const expr_t **adds, const expr_t **subs)
{
	for (auto scan = adds; *scan; scan++) {
		if (is_dot (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_dot (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_dot (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_dot (e, subs, adds);
		}
	}

	clean_skips (adds);
	clean_skips (subs);
}

static const expr_t *
raise_scale (const expr_t *expr)
{
	int op = 0;
	if (is_cross (expr)) {
		op = QC_CROSS;
	}
	if (is_dot (expr)) {
		op = QC_DOT;
	}
	if (!op) {
		return expr;
	}

	auto type = get_type (expr);
	const expr_t *a_mult = nullptr;
	const expr_t *b_mult = nullptr;
	auto a = raise_scale (expr->expr.e1);
	auto b = raise_scale (expr->expr.e2);
	if (is_scale (a)) {
		a_mult = a->expr.e2;
		a = a->expr.e1;
	}
	if (is_scale (b)) {
		b_mult = b->expr.e2;
		b = b->expr.e1;
	}
	expr = typed_binary_expr (type, op, a, b);
	expr = edag_add_expr (expr);
	if (a_mult || b_mult) {
		if (op == QC_CROSS) {
			const expr_t *mult;
			if (a_mult && b_mult) {
				mult = binary_expr ('*', a_mult, b_mult);
			} else {
				mult = a_mult ? a_mult : b_mult;
			}
			expr = typed_binary_expr (type, QC_SCALE, expr, mult);
			expr = fold_constants (expr);
			expr = edag_add_expr (expr);
		} else {
			if (a_mult) {
				expr = typed_binary_expr (type, '*', expr, a_mult);
			}
			if (b_mult) {
				expr = typed_binary_expr (type, '*', expr, b_mult);
			}
			expr = edag_add_expr (expr);
		}
	}
	return expr;
}

static void
optimize_scale_products (const expr_t **adds, const expr_t **subs)
{
	for (auto scan = adds; *scan; scan++) {
		if (!is_scale (*scan)) {
			*scan = raise_scale (*scan);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (!is_scale (*scan)) {
			*scan = raise_scale (*scan);
		}
	}
	for (auto scan = adds; *scan; scan++) {
		if (is_scale (*scan) && has_const_factor ((*scan)->expr.e2)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_scale (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_scale (*scan) && has_const_factor ((*scan)->expr.e2)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_scale (e, subs, adds);
		}
	}
	for (auto scan = adds; *scan; scan++) {
		if (is_scale (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_scale (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_scale (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_scale (e, subs, adds);
		}
	}

	clean_skips (adds);
	clean_skips (subs);
}

static void
optimize_mult_products (const expr_t **adds, const expr_t **subs)
{
	for (auto scan = adds; *scan; scan++) {
		if (is_mult (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_mult (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_mult (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_mult (e, subs, adds);
		}
	}

	clean_skips (adds);
	clean_skips (subs);
}

static int
expr_ptr_cmp (const void *_a, const void *_b)
{
	auto a = *(const expr_t **) _a;
	auto b = *(const expr_t **) _b;
	return a - b;
}

static void
optimize_adds (const expr_t **expr_list)
{
	int count = 0;
	for (auto scan = expr_list; *scan; scan++, count++) continue;
	heapsort (expr_list, count, sizeof (expr_list[0]), expr_ptr_cmp);

	for (auto scan = expr_list; *scan; scan++) {
		if (*scan == &skip) {
			continue;
		}
		int same = 0;
		for (auto expr = scan + 1; *expr; expr++) {
			if (*expr == *scan) {
				same++;
				*expr = &skip;
			}
		}
		if (same++) {
			auto type = get_type (*scan);
			auto mult = cast_expr (base_type (type),
								   new_int_expr (same, false));
			mult = edag_add_expr (mult);
			*scan = scale_expr (*scan, mult);
		}
	}
	clean_skips (expr_list);
}

static bool
check_anticom (const expr_t *a, const expr_t *b)
{
	if (a->expr.op == b->expr.op
		&& a->expr.e1 == b->expr.e2
		&& a->expr.e2 == b->expr.e1) {
		return true;
	}
	return false;
}

static bool
check_dot_pair (const expr_t *dot, const expr_t *a, const expr_t *b)
{
	if (dot->expr.e1 == a && dot->expr.e2 == b) {
		return true;
	}
	if (dot->expr.e2 == a && dot->expr.e1 == b) {
		return true;
	}
	return false;
}

// check for (a×b)×c - a•c b, replace with -b•c a
// simpler, fewer opportunities for rounding issues (maybe), and very likely
// to cancel with a related permutation
static bool
simp_lcross_sub_dot_scale (const expr_t **a, const expr_t **subs)
{
	if (is_cross (*a)) {
		if (is_cross ((*a)->expr.e1)) {
			auto aa = (*a)->expr.e1->expr.e1;
			auto ab = (*a)->expr.e1->expr.e2;
			auto ac = (*a)->expr.e2;
			for (auto b = subs; *b; b++) {
				if (b != a && is_scale (*b) && is_dot ((*b)->expr.e2)) {
					if (check_dot_pair ((*b)->expr.e2, aa, ac)
						&& (*b)->expr.e1 == ab) {
						*b = scale_expr (aa, dot_expr (ab, ac));
						*a = &skip;
						return true;
					}
				}
			}
		}
	}
	return false;
}

// check for (a×b)×c + b•c a, replace with a•c b
// simpler, fewer opportunities for rounding issues (maybe), and very likely
// to cancel with a related permutation
static bool
simp_lcross_add_dot_scale (const expr_t **a, const expr_t **adds)
{
	if (is_cross (*a)) {
		if (is_cross ((*a)->expr.e2)) {
			auto aa = (*a)->expr.e1->expr.e1;
			auto ab = (*a)->expr.e1->expr.e2;
			auto ac = (*a)->expr.e2;
			for (auto b = adds; *b; b++) {
				if (b != a && is_scale (*b) && is_dot ((*b)->expr.e2)) {
					if (check_dot_pair ((*b)->expr.e2, ab, ac)
						&& (*b)->expr.e1 == aa) {
						*a = scale_expr (ab, dot_expr (aa, ac));
						*b = &skip;
						return true;
					}
				}
			}
		}
	}
	return false;
}

// check for a×(b×c) - a•c b, replace with -a•b c
// simpler, fewer opportunities for rounding issues (maybe), and very likely
// to cancel with a related permutation
static bool
simp_rcross_sub_dot_scale (const expr_t **a, const expr_t **subs)
{
	if (is_cross (*a)) {
		if (is_cross ((*a)->expr.e2)) {
			auto aa = (*a)->expr.e1;
			auto ab = (*a)->expr.e2->expr.e1;
			auto ac = (*a)->expr.e2->expr.e2;
			for (auto b = subs; *b; b++) {
				if (b != a && is_scale (*b) && is_dot ((*b)->expr.e2)) {
					if (check_dot_pair ((*b)->expr.e2, aa, ac)
						&& (*b)->expr.e1 == ab) {
						*b = scale_expr (ac, dot_expr (aa, ab));
						*a = &skip;
						return true;
					}
				}
			}
		}
	}
	return false;
}

// check for a×(b×c) + a•b c, replace with a•c b
// simpler, fewer opportunities for rounding issues (maybe), and very likely
// to cancel with a related permutation
static bool
simp_rcross_add_dot_scale (const expr_t **a, const expr_t **adds)
{
	if (is_cross (*a)) {
		if (is_cross ((*a)->expr.e2)) {
			auto aa = (*a)->expr.e1;
			auto ab = (*a)->expr.e2->expr.e1;
			auto ac = (*a)->expr.e2->expr.e2;
			for (auto b = adds; *b; b++) {
				if (b != a && is_scale (*b) && is_dot ((*b)->expr.e2)) {
					if (check_dot_pair ((*b)->expr.e2, aa, ab)
						&& (*b)->expr.e1 == ac) {
						*a = scale_expr (ab, dot_expr (aa, ac));
						*b = &skip;
						return true;
					}
				}
			}
		}
	}
	return false;
}

static void
cancel_terms (const expr_t **adds, const expr_t **subs)
{
	bool did_something;
	do {
		did_something = false;
		for (auto a = adds; *a; a++) {
			did_something |= simp_rcross_sub_dot_scale (a, subs);
		}
		for (auto a = subs; *a; a++) {
			did_something |= simp_rcross_sub_dot_scale (a, adds);
		}
		for (auto a = adds; *a; a++) {
			did_something |= simp_rcross_add_dot_scale (a, adds);
		}
		for (auto a = subs; *a; a++) {
			did_something |= simp_rcross_add_dot_scale (a, subs);
		}
		for (auto a = adds; *a; a++) {
			did_something |= simp_lcross_sub_dot_scale (a, subs);
		}
		for (auto a = subs; *a; a++) {
			did_something |= simp_lcross_sub_dot_scale (a, adds);
		}
		for (auto a = adds; *a; a++) {
			did_something |= simp_lcross_add_dot_scale (a, adds);
		}
		for (auto a = subs; *a; a++) {
			did_something |= simp_lcross_add_dot_scale (a, subs);
		}
	} while (did_something);
	for (auto a = adds; *a; a++) {
		if (is_anticommute (*a)) {
			for (auto b = a + 1; *b; b++) {
				if (is_anticommute (*b) && check_anticom (*a, *b)) {
					*b = &skip;
					*a = &skip;
					break;
				}
			}
		}
		if (*a == &skip) {
			continue;
		}
		for (auto s = subs; *s; s++) {
			if (*s == *a) {
				*a = &skip;
				*s = &skip;
				break;
			}
		}
	}
	clean_skips (adds);
	clean_skips (subs);
}

static bool
cmp_scalar_vector (const expr_t *scalar, const expr_t *vector)
{
	if (!is_constant (scalar) || !is_constant (vector)) {
		return false;
	}
	auto scalar_type = get_type (scalar);
	auto vector_type = get_type (vector);
	int scalar_size = type_size (scalar_type);
	int vector_size = type_size (vector_type);
	pr_type_t scalar_val[scalar_size];
	pr_type_t vector_val[vector_size];
	value_store (scalar_val, scalar_type, scalar);
	value_store (vector_val, vector_type, vector);
	size_t size = scalar_size * sizeof (pr_type_t);
	for (int i = 0, j = 0; i < vector_size / scalar_size;
		 i++, j += scalar_size) {
		if (memcmp (scalar_val, &vector_val[j], size) != 0) {
			return false;
		}
	}
	return true;
}

static const expr_t *
optimize_core (const expr_t *expr)
{
	if (is_neg (expr)) {
		auto neg = optimize_core (expr->expr.e1);
		if (neg && has_const_factor (neg)) {
			auto mone = new_int_expr (-1, true);
			neg = binary_expr ('*', neg, mone);
			neg = optimize_core (neg);
			return neg;
		} else {
			return neg_expr (neg);
		}
	} else if (expr->type == ex_swizzle) {
		auto src = optimize_core (expr->swizzle.src);
		auto new = new_expr_copy (expr);
		new->swizzle.src = src;
		auto swiz = edag_add_expr (new);
		if (is_ext (src)) {
			swiz = ext_swizzle (src, swiz);
		}
		return swiz;
	} else if (is_ext (expr)) {
		auto new = optimize_core (expr->extend.src);
		if (new) {
			auto ext = new_expr_copy (expr);
			ext->extend.src = new;
			new = ext;
		}
		return new;
	} else if (is_sum (expr)) {
		auto type = get_type (expr);
		int count = count_terms (expr);
		const expr_t *adds[count + 1] = {};
		const expr_t *subs[count + 1] = {};
		scatter_terms (expr, adds, subs);

		optimize_extends (adds);
		optimize_extends (subs);

		if (expr->expr.commutative) {
			optimize_adds (adds);
			optimize_adds (subs);
		}
		cancel_terms (adds, subs);

		optimize_cross_products (adds, subs);
		optimize_dot_products (adds, subs);
		optimize_scale_products (adds, subs);
		optimize_mult_products (adds, subs);

		expr = gather_terms (type, adds, subs);
	} else if (is_quot (expr)) {
		auto type = get_type (expr);
		auto num = optimize_core (expr->expr.e1);
		auto den = optimize_core (expr->expr.e2);
		if (!num) {
			if (!den) {
				return binary_expr ('/', num, den);
			}
			return nullptr;
		}
		if (!den) {
			return binary_expr ('/', num, den);
		}
		auto num_type = get_type (num);
		int num_count = count_factors (num) + 1;
		int den_count = count_factors (den) + 1;
		const expr_t *num_fac[num_count + 1] = {};
		const expr_t *den_fac[den_count + 1] = {};
		scatter_factors (num, num_fac);
		scatter_factors (den, den_fac);

		for (auto n = num_fac; *n; n++) {
			for (auto d = den_fac; *d; d++) {
				if (*d == &skip) {
					continue;
				}
				if (*n == *d) {
					*n = &skip;
					*d = &skip;
					break;
				}
				if (is_scalar (get_type (*n)) && is_constant (*n)
					&& is_scalar (get_type (*d)) && is_constant (*d)) {
					auto gt_expr = binary_expr (QC_GT, *n, *d);
					const expr_t *div;
					bool gt = expr_integral (gt_expr);
					if (gt) {
						div = binary_expr ('/', *n, *d);
					} else {
						div = binary_expr ('/', *d, *n);
					}
					auto mul = binary_expr ('*', div, gt ? *d : *n);
					auto eq = binary_expr (QC_EQ, mul, gt ? *n : *d);
					if (expr_integral (eq)) {
						*n = gt ?  div  : &skip;
						*d = gt ? &skip :  div ;
						break;
					}
				}
				if (is_scale (*n)) {
					auto n_scale = (*n)->expr.e2;
					if (is_scale (*d) && n_scale == (*d)->expr.e2) {
						*n = (*n)->expr.e1;
						*d = (*d)->expr.e1;
						break;
					}
					if (cmp_scalar_vector (n_scale, *d)) {
						*n = (*n)->expr.e1;
						*d = &skip;
						break;
					}
				}
				if (is_scale (*d)) {
					auto d_scale = (*d)->expr.e2;
					if (cmp_scalar_vector (d_scale, *n)) {
						*n = &skip;
						*d = (*d)->expr.e1;
						break;
					}
				}
			}
		}
		num_count = clean_skips (num_fac);
		den_count = clean_skips (den_fac);

		if (!num_count && !den_count) {
			expr = edag_add_expr (new_int_expr (1, true));
			return cast_expr (type, expr);
		}

		if (num_count) {
			if (den_count) {
				num = gather_factors ('*', type, num_fac, num_count);
			} else {
				expr = gather_factors ('*', type, num_fac, num_count);
			}
		} else {
			num = edag_add_expr (new_int_expr (1, true));
			num = cast_expr (num_type, num);
		}
		if (den_count) {
			den = gather_factors ('*', type, den_fac, den_count);
			expr = typed_binary_expr (type, '/', num, den);
		}
		expr = fold_constants (expr);
		expr = edag_add_expr (expr);
	} else if (expr->type == ex_expr || expr->type == ex_uexpr) {
		auto type = get_type (expr);
		const expr_t *pos[2] = { expr };
		const expr_t *neg[1] = {};

		optimize_cross_products (pos, neg);
		optimize_dot_products (pos, neg);
		optimize_scale_products (pos, neg);
		optimize_mult_products (pos, neg);

		expr = gather_terms (type, pos, neg);
	}
	return expr;
}

const expr_t *
algebra_optimize (const expr_t *expr)
{
	if (expr->type == ex_alias) {
		auto alias = expr->alias;
		alias.expr = optimize_core (alias.expr);
		if (!alias.expr) {
			return nullptr;
		}
		auto a = new_expr ();
		a->type = ex_alias;
		a->alias = alias;
		return edag_add_expr (a);
	} else if (expr->type != ex_multivec) {
		return optimize_core (expr);
	} else {
		auto algebra = algebra_get (get_type (expr));
		auto layout = &algebra->layout;
		const expr_t *groups[layout->count] = {};
		expr = mvec_expr (expr, algebra);
		mvec_scatter (groups, expr, algebra);

		for (int i = 0; i < layout->count; i++) {
			if (groups[i]) {
				groups[i] = optimize_core (groups[i]);
			}
		}
		expr = mvec_gather (groups, algebra);
	}

	return expr;
}

const expr_t *
expr_optimize (const expr_t *expr)
{
	if (is_error (expr)) {
		return expr;
	}
	auto type = get_type (expr);
	expr = algebra_optimize (expr);
	if (!expr) {
		expr = new_zero_expr (type);
	}
	return expr;
}
