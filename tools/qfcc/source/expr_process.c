/*
	expr_process.c

	expression processing

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

typedef const expr_t *(*process_f) (const expr_t *expr);

static const expr_t *
proc_expr (const expr_t *expr)
{
	auto e1 = expr_process (expr->expr.e1);
	auto e2 = expr_process (expr->expr.e2);
	if (is_error (e1)) {
		return e1;
	}
	if (is_error (e2)) {
		return e2;
	}

	return binary_expr (expr->expr.op, e1, e2);
}

static const expr_t *
proc_uexpr (const expr_t *expr)
{
	auto e1 = expr_process (expr->expr.e1);
	if (is_error (e1)) {
		return e1;
	}

	return unary_expr (expr->expr.op, e1);
}

static const expr_t *
proc_symbol (const expr_t *expr)
{
	return expr;
}

static void
proc_do_list (ex_list_t *out, const ex_list_t *in)
{
	int count = list_count (in);
	const expr_t *exprs[count + 1];
	list_scatter (in, exprs);
	for (int i = 0; i < count; i++) {
		exprs[i] = expr_process (exprs[i]);
	}
	list_gather (out, exprs, count);
}

static const expr_t *
proc_vector (const expr_t *expr)
{
	auto vec = new_expr ();
	vec->type = ex_vector;
	vec->vector.type = expr->vector.type;
	proc_do_list (&vec->vector.list, &expr->vector.list);
	return vec;
}

static const expr_t *
proc_value (const expr_t *expr)
{
	return expr;
}

static const expr_t *
proc_compound (const expr_t *expr)
{
	auto comp = new_compound_init ();
	for (auto ele = expr->compound.head; ele; ele = ele->next) {
		append_element (comp, new_element (expr_process (ele->expr),
										   ele->designator));
	}
	return comp;
}

static const expr_t *
proc_assign (const expr_t *expr)
{
	auto dst = expr_process (expr->assign.dst);
	auto src = expr_process (expr->assign.src);
	if (is_error (src)) {
		return src;
	}
	if (is_error (src)) {
		return src;
	}
	return assign_expr (dst, src);
}

static const expr_t *
proc_branch (const expr_t *expr)
{
	if (expr->branch.type == pr_branch_call) {
		auto args = new_list_expr (nullptr);
		proc_do_list (&args->list, &expr->branch.args->list);
		return function_expr (expr->branch.target, args);
	} else {
		auto branch = new_expr ();
		branch->type = ex_branch;
		branch->branch = expr->branch;
		branch->branch.target = expr_process (expr->branch.target);
		branch->branch.index = expr_process (expr->branch.index);
		branch->branch.test = expr_process (expr->branch.test);
		if (is_error (branch->branch.target)) {
			return branch->branch.target;
		}
		if (is_error (branch->branch.index)) {
			return branch->branch.index;
		}
		if (is_error (branch->branch.test)) {
			return branch->branch.test;
		}
		return branch;
	}
}

const expr_t *
expr_process (const expr_t *expr)
{
	static process_f funcs[ex_count] = {
		[ex_expr] = proc_expr,
		[ex_uexpr] = proc_uexpr,
		[ex_symbol] = proc_symbol,
		[ex_vector] = proc_vector,
		[ex_value] = proc_value,
		[ex_compound] = proc_compound,
		[ex_assign] = proc_assign,
		[ex_branch] = proc_branch,
	};

	if (expr->type >= ex_count) {
		internal_error (expr, "bad sub-expression type: %d", expr->type);
	}
	if (!funcs[expr->type]) {
		internal_error (expr, "unexpected sub-expression type: %s",
						expr_names[expr->type]);
	}

	return funcs[expr->type] (expr);
}
