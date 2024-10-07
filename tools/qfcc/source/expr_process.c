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
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
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

	scoped_src_loc (expr);
	return binary_expr (expr->expr.op, e1, e2);
}

static const expr_t *
proc_uexpr (const expr_t *expr)
{
	auto e1 = expr_process (expr->expr.e1);
	if (is_error (e1)) {
		return e1;
	}

	scoped_src_loc (expr);
	return unary_expr (expr->expr.op, e1);
}

static const expr_t *
proc_block (const expr_t *expr)
{
	if (expr->block.scope) {
		expr->block.scope->parent = current_symtab;
		current_symtab = expr->block.scope;
	}
	int count = list_count (&expr->block.list);
	int num_out = 0;
	const expr_t *result = nullptr;
	const expr_t *in[count + 1];
	const expr_t *out[count + 1];
	list_scatter (&expr->block.list, in);
	for (int i = 0; i < count; i++) {
		auto e = expr_process (in[i]);
		if (e && !is_error (e)) {
			out[num_out++] = e;
			if (expr->block.result == in[i]) {
				result = e;
			}
		}
	}

	scoped_src_loc (expr);
	auto block = new_block_expr (nullptr);
	list_gather (&block->block.list, out, num_out);
	block->block.scope = expr->block.scope;
	block->block.result = result;
	block->block.is_call = expr->block.is_call;
	if (expr->block.scope) {
		current_symtab = current_symtab->parent;
	}
	return block;
}

static const expr_t *
proc_symbol (const expr_t *expr)
{
	auto sym = symtab_lookup (current_symtab, expr->symbol->name);
	if (sym) {
		scoped_src_loc (expr);
		expr = new_symbol_expr (sym);
	}
	return expr;
}

static bool
proc_do_list (ex_list_t *out, const ex_list_t *in)
{
	int count = list_count (in);
	const expr_t *exprs[count + 1];
	list_scatter (in, exprs);
	bool ok = true;
	for (int i = 0; i < count; i++) {
		exprs[i] = expr_process (exprs[i]);
		if (is_error (exprs[i])) {
			ok = false;
		}
	}
	list_gather (out, exprs, count);
	return ok;
}

static const expr_t *
proc_vector (const expr_t *expr)
{
	scoped_src_loc (expr);
	auto vec = new_expr ();
	vec->type = ex_vector;
	vec->vector.type = expr->vector.type;
	if (!proc_do_list (&vec->vector.list, &expr->vector.list)) {
		return new_error_expr ();
	}
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
	scoped_src_loc (expr);
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
	scoped_src_loc (expr);
	return assign_expr (dst, src);
}

static const expr_t *
proc_branch (const expr_t *expr)
{
	scoped_src_loc (expr);
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

static const expr_t *
proc_decl (const expr_t *expr)
{
	if (expr->decl.spec.storage == sc_local) {
		scoped_src_loc (expr);
		local_expr = new_block_expr (nullptr);
	}
	int count = list_count (&expr->decl.list);
	const expr_t *decls[count + 1];
	list_scatter (&expr->decl.list, decls);
	for (int i = 0; i < count; i++) {
		auto decl = decls[i];
		scoped_src_loc (decl);
		const expr_t *init = nullptr;
		symbol_t   *sym;
		if (decl->type == ex_assign) {
			init = decl->assign.src;
			if (decl->assign.dst->type != ex_symbol) {
				internal_error (decl->assign.dst, "not a symbol");
			}
			pr.loc = decl->assign.dst->loc;
			sym = decl->assign.dst->symbol;
		} else if (decl->type == ex_symbol) {
			sym = decl->symbol;
		} else {
			internal_error (decl, "not a symbol");
		}
		auto spec = expr->decl.spec;
		if (sym && sym->type) {
			spec.type = append_type (sym->type, spec.type);
			spec.type = find_type (spec.type);
			sym->type = nullptr;
		}
		current_language.parse_declaration (spec, sym, init, current_symtab);
	}
	auto block = local_expr;
	local_expr = nullptr;
	return block;
}

const expr_t *
expr_process (const expr_t *expr)
{
	static process_f funcs[ex_count] = {
		[ex_expr] = proc_expr,
		[ex_uexpr] = proc_uexpr,
		[ex_block] = proc_block,
		[ex_symbol] = proc_symbol,
		[ex_vector] = proc_vector,
		[ex_value] = proc_value,
		[ex_compound] = proc_compound,
		[ex_assign] = proc_assign,
		[ex_branch] = proc_branch,
		[ex_decl] = proc_decl,
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
