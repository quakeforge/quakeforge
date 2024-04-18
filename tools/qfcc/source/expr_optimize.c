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

#include "QF/fbsearch.h"
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

static void
clean_skips (const expr_t **expr_list)
{
	auto dst = expr_list;
	for (auto src = dst; *src; src++) {
		if (*src != &skip) {
			*dst++ = *src;
		}
	}
	*dst = 0;
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
	auto type = get_type (expr);
	auto scale = expr->expr.e2;
	return scale_expr (type, rescale (expr->expr.e1, target, remove), scale);
}

static const expr_t *
remult (const expr_t *expr, const expr_t *remove)
{
	if (!is_mult (expr)) {
		internal_error (expr, "not a mult expression");
	}
	auto type = get_type (expr);
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
	auto new = gather_factors (type, expr->expr.op, factors, count - 1);
	return new;
}

static const expr_t *
remult_scale (const expr_t *expr, const expr_t *remove)
{
	auto mult = remult (expr->expr.e2, remove);
	auto scalee = expr->expr.e1;
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
			const expr_t *scale = 0;
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
			const expr_t *scale = 0;
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

static bool __attribute__((pure))
mult_has_factor (const expr_t *mult, const expr_t *factor)
{
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

	const expr_t *common = 0;
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
	col = optimize_core (col);

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
	if (!total) {
		return expr;
	}

	const expr_t *common = 0;
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

	auto type = get_type (expr);
	auto col = gather_terms (type, com_adds, com_subs);
	col = optimize_core (col);

	auto mult = typed_binary_expr (type, expr->expr.op, col, common);
	mult = edag_add_expr (mult);
	return mult;
}

static void
optimize_extends (const expr_t **expr_list)
{
	for (auto scan = expr_list; *scan; scan++) {
		if ((*scan)->type == ex_extend) {
			auto ext = (*scan)->extend;
			ext.src = optimize_core (ext.src);
			*scan = ext_expr (ext.src, ext.type, ext.extend, ext.reverse);
		}
	}
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
optimize_scale_products (const expr_t **adds, const expr_t **subs)
{
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
		if (is_mult (*scan) && has_const_factor (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_mult (e, adds, subs);
		}
	}
	for (auto scan = subs; *scan; scan++) {
		if (is_mult (*scan) && has_const_factor (*scan)) {
			auto e = *scan;
			*scan = &skip;
			*scan = optimize_mult (e, subs, adds);
		}
	}
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
			*scan = scale_expr (type, *scan, mult);
		}
	}
	clean_skips (expr_list);
}

static const expr_t *
optimize_core (const expr_t *expr)
{
	if (is_sum (expr)) {
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

		optimize_cross_products (adds, subs);
		optimize_scale_products (adds, subs);
		optimize_mult_products (adds, subs);

		expr = gather_terms (type, adds, subs);
	}
	return expr;
}

const expr_t *
algebra_optimize (const expr_t *expr)
{
	if (expr->type != ex_multivec) {
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
