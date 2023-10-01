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
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

static const expr_t skip;

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
	const expr_t *cross;
	if (right) {
		cross = typed_binary_expr (type, CROSS, col, com);
	} else {
		cross = typed_binary_expr (type, CROSS, com, col);
	}
	cross = edag_add_expr (cross);
	return cross;
}

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

static const expr_t *optimize_core (const expr_t *expr);

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

		optimize_cross_products (adds, subs);

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
