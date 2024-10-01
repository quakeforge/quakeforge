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

const expr_t *
expr_process (const expr_t *expr)
{
	static process_f funcs[ex_count] = {
		[ex_expr] = proc_expr,
		[ex_uexpr] = proc_uexpr,
		[ex_value] = proc_value,
		[ex_compound] = proc_compound,
		[ex_symbol] = proc_symbol,
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
