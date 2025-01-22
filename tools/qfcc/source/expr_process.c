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
#include <math.h>

#include "QF/fbsearch.h"
#include "QF/heapsort.h"
#include "QF/math/bitop.h"

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

typedef const expr_t *(*process_f) (const expr_t *expr, rua_ctx_t *ctx);

static const type_t *
proc_decl_type (const expr_t *decl, rua_ctx_t *ctx)
{
	if (decl->type != ex_decl) {
		internal_error (decl, "not a decl expression");
	}
	if (decl->decl.list.head) {
		internal_error (decl, "non-empty cast decl");
	}
	auto spec = spec_process (decl->decl.spec, ctx);
	return spec.type;
}

static const expr_t *
proc_expr (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	if (expr->expr.op == 'C') {
		auto type = proc_decl_type (expr->expr.e1, ctx);
		auto e = expr_process (expr->expr.e2, ctx);
		return cast_expr (type, e);
	}
	auto e1 = expr_process (expr->expr.e1, ctx);
	auto e2 = expr_process (expr->expr.e2, ctx);
	if (is_error (e1)) {
		return e1;
	}
	if (is_error (e2)) {
		return e2;
	}

	if (options.code.short_circuit
		&& ctx->language->short_circuit) {
		if (expr->expr.op == QC_AND || expr->expr.op == QC_OR) {
			auto label = new_label_expr ();
			return bool_expr (expr->expr.op, label, e1, e2);
		}
	}

	auto e = binary_expr (expr->expr.op, e1, e2);
	if (expr->paren) {
		e = paren_expr (e);
	}
	return e;
}

static const expr_t *
proc_uexpr (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	if (expr->expr.op == '&') {
		return current_target.proc_address (expr, ctx);
	}
	auto e1 = expr_process (expr->expr.e1, ctx);
	if (is_error (e1)) {
		return e1;
	}
	if (expr->expr.op == 'S') {
		const type_t *type;
		if (e1->type == ex_type) {
			type = e1->typ.type;
		} else {
			type = get_type (e1);
		}
		return sizeof_expr (nullptr, type);
	}

	return unary_expr (expr->expr.op, e1);
}

static const expr_t *
proc_field (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto object = expr_process (expr->field.object, ctx);
	auto member = expr->field.member;
	if (is_error (object)) {
		return object;
	}
	if (object->type == ex_symbol && object->symbol->sy_type == sy_namespace) {
		if (member->type != ex_symbol) {
			return error (member, "symbol required for namespace access");
		}
		auto namespace = object->symbol->namespace;
		auto sym = symtab_lookup (namespace, member->symbol->name);
		if (!sym) {
			return error (member, "%s not in %s namespace",
						  member->symbol->name, object->symbol->name);
		}
		return new_symbol_expr (sym);
	}

	auto obj_type = get_type (object);
	if (!obj_type) {
		return new_error_expr ();
	}

	if (is_reference (obj_type)) {
		obj_type = dereference_type (obj_type);
	}

	if (is_class (obj_type)) {
		//Class instance variables aren't allowed and thus declaring one
		//is treated as an error, so this is a follow-on error.
		return new_error_expr ();
	}
	if (is_pointer (obj_type)) {
		auto ref_type = dereference_type (obj_type);
		if (!(is_struct (ref_type) || is_union (ref_type)
			  || is_class (ref_type))) {
			return type_mismatch (object, member, '.');
		}
		obj_type = ref_type;
	}
	if (is_algebra (obj_type)) {
		return algebra_field_expr (object, member);
	}
	if (is_entity (obj_type)) {
		symbol_t *field = nullptr;
		if (member->type == ex_symbol) {
			field = get_struct_field (&type_entity, object, member);
		}
		if (field) {
			member = new_deffield_expr (0, field->type, field->def);
			member = edag_add_expr (member);
			return typed_binary_expr (field->type, '.', object, member);
		} else {
			member = expr_process (member, ctx);
			if (is_error (member)) {
				return member;
			}
			auto mem_type = get_type (member);
			if (is_field (mem_type)) {
				mem_type = dereference_type (mem_type);
				return typed_binary_expr (mem_type, '.', object, member);
			}
		}
		return type_mismatch (object, member, '.');
	} else if (is_nonscalar (obj_type)) {
		auto field = get_struct_field (obj_type, object, member);
		if (!field) {
			if (member->type != ex_symbol) {
				return error (member, "invalid swizzle");
			}
			return new_swizzle_expr (object, member->symbol->name);
		}
		member = new_symbol_expr (field);
	} else if (is_struct (obj_type) || is_union (obj_type)) {
		auto field = get_struct_field (obj_type, object, member);
		if (!field) {
			return new_error_expr ();
		}
		member = new_symbol_expr (field);
	} else if (is_class (obj_type)) {
		if (member->type != ex_symbol) {
			return error (member, "invalid class member access");
		}
		auto class = obj_type->class;
		auto sym = member->symbol;
		int  protected = class_access (current_class, class);
		auto ivar = class_find_ivar (class, protected, sym->name);
		if (!ivar) {
			return new_error_expr ();
		}
		member = new_symbol_expr (ivar);
	}
	member = edag_add_expr (member);
	auto e = new_field_expr (object, member);
	e->field.type = member->symbol->type;
	return e;
}

static const expr_t *
proc_array (const expr_t *expr, rua_ctx_t *ctx)
{
	auto array = expr_process (expr->array.base, ctx);
	auto index = expr_process (expr->array.index, ctx);
	if (is_error (array)) {
		return array;
	}
	if (is_error (index)) {
		return index;
	}
	auto array_type = get_type (array);
	auto index_type = get_type (index);
	if (is_reference (array_type)) {
		array_type = dereference_type (array_type);
	}
	if (is_reference (index_type)) {
		index_type = dereference_type (index_type);
	}
	if (!is_pointer (array_type) && !is_array (array_type)
		&& !is_nonscalar (array_type) && !is_matrix (array_type)) {
		return error (array, "not an array");
	}
	if (!is_integral (index_type)) {
		return error (index, "invalid array index type");
	}
	scoped_src_loc (expr);
	auto e = new_array_expr (array, index);
	const type_t *type;
	if (is_matrix (array_type)) {
		type = column_type (array_type);
	} else if (is_nonscalar (array_type)) {
		type = base_type (array_type);
	} else {
		type = dereference_type (array_type);
	}
	e->array.type = type;
	return e;
}

static const expr_t *
proc_label (const expr_t *expr, rua_ctx_t *ctx)
{
	return expr;
}

static const expr_t *
proc_block (const expr_t *expr, rua_ctx_t *ctx)
{
	auto old_scope = current_symtab;
	current_symtab = expr->block.scope;
	int count = list_count (&expr->block.list);
	int num_out = 0;
	bool flush = !expr->block.no_flush;
	const expr_t *result = nullptr;
	const expr_t *in[count + 1];
	const expr_t *out[count + 1];
	const expr_t *err = nullptr;
	list_scatter (&expr->block.list, in);
	for (int i = 0; i < count; i++) {
		if (flush) {
			edag_flush ();
		}
		auto e = expr_process (in[i], ctx);
		if (is_error (e)) {
			err = e;
			continue;
		}
		if (e) {
			out[num_out++] = e;
			if (expr->block.result == in[i]) {
				result = e;
			}
		}
	}
	if (flush) {
		edag_flush ();
	}
	if (err) {
		current_symtab = old_scope;
		return err;
	}
	if (!result && expr->block.result) {
		result = expr_process (expr->block.result, ctx);
		if (is_error (result)) {
			return result;
		}
	}

	scoped_src_loc (expr);
	auto block = new_block_expr (nullptr);
	list_gather (&block->block.list, out, num_out);
	block->block.scope = expr->block.scope;
	block->block.result = result;
	block->block.is_call = expr->block.is_call;

	current_symtab = old_scope;
	return block;
}

static const expr_t *
ps_pretty_function (const expr_t *e)
{
	return new_string_expr (current_func->name);
}

static const expr_t *
ps_function (const expr_t *e)
{
	return new_string_expr (current_func->def->name);
}

static const expr_t *
ps_line (const expr_t *e)
{
	return new_int_expr (e->loc.line, false);
}

static const expr_t *
ps_infinity (const expr_t *e)
{
	return new_float_expr (INFINITY, false);
}

static const expr_t *
ps_file (const expr_t *e)
{
	return new_string_expr (GETSTR (e->loc.file));
}

static const expr_t *
ps_super (const expr_t *e)
{
	if (current_class && strcmp (e->symbol->name, "super") == 0) {
		// FIXME check for in message receiver expr?
		return e;
	}
	return nullptr;
}

static struct {
	const char *name;
	const expr_t *(*expr) (const expr_t *e);
} builtin_names[] = {
	{ .name = "__PRETTY_FUNCTION__", .expr = ps_pretty_function },
	{ .name = "__FUNCTION__",        .expr = ps_function        },
	{ .name = "__LINE__",            .expr = ps_line            },
	{ .name = "__INFINITY__",        .expr = ps_infinity        },
	{ .name = "__FILE__",            .expr = ps_file            },
	{ .name = "super",               .expr = ps_super           },

	{}
};

static const expr_t *
proc_symbol (const expr_t *expr, rua_ctx_t *ctx)
{
	auto sym = expr->symbol;
	for (auto bi = builtin_names; bi->name; bi++) {
		if (strcmp (bi->name, sym->name) == 0) {
			scoped_src_loc (expr);
			auto e = bi->expr (expr);
			if (e) {
				return bi->expr (expr);
			}
			break;
		}
	}
	sym = symtab_lookup (current_symtab, sym->name);
	if (sym) {
		if (sym->sy_type == sy_expr || sym->sy_type == sy_type_param) {
			return sym->expr;
		}
		if (sym->sy_type == sy_convert) {
			return sym->convert.conv (sym, sym->convert.data);
		}
		scoped_src_loc (expr);
		return new_symbol_expr (sym);
	}
	return error (expr, "undefined symbol `%s`", expr->symbol->name);
}

bool
proc_do_list (ex_list_t *out, const ex_list_t *in, rua_ctx_t *ctx)
{
	int count = list_count (in);
	const expr_t *exprs[count + 1] = {};
	list_scatter (in, exprs);
	bool ok = true;
	int new_count = 0;
	for (int i = 0; i < count; i++) {
		exprs[new_count] = expr_process (exprs[i], ctx);
		// keep null expressions out of the list (non-local declarations
		// or local declarations without initializers return null)
		new_count += !!exprs[new_count];
		if (is_error (exprs[i])) {
			ok = false;
		}
	}
	list_gather (out, exprs, new_count);
	return ok;
}

static const expr_t *
proc_vector (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto list = new_expr ();
	list->type = ex_list;
	if (!proc_do_list (&list->list, &expr->vector.list, ctx)) {
		return new_error_expr ();
	}
	return new_vector_list (list);
}

static const expr_t *
proc_selector (const expr_t *expr, rua_ctx_t *ctx)
{
	return expr;
}

static const expr_t *
proc_message (const expr_t *expr, rua_ctx_t *ctx)
{
	auto receiver = expr_process (expr->message.receiver, ctx);
	auto message = expr->message.message;
	scoped_src_loc (receiver);
	for (auto k = message; k; k = k->next) {
		if (k->expr) {
			k->expr = (expr_t *) expr_process (k->expr, ctx);
		}
	}
	return message_expr (receiver, message, ctx);
}

static const expr_t *
proc_nil (const expr_t *expr, rua_ctx_t *ctx)
{
	return expr;
}

static const expr_t *
proc_value (const expr_t *expr, rua_ctx_t *ctx)
{
	return expr;
}

static const expr_t *
proc_compound (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto comp = new_compound_init ();
	if (expr->compound.type_expr) {
		comp->compound.type = proc_decl_type (expr->compound.type_expr, ctx);
	}
	for (auto ele = expr->compound.head; ele; ele = ele->next) {
		append_element (comp, new_element (expr_process (ele->expr, ctx),
										   ele->designator));
	}
	return comp;
}

static const expr_t *
proc_assign (const expr_t *expr, rua_ctx_t *ctx)
{
	auto dst = expr_process (expr->assign.dst, ctx);
	auto src = expr_process (expr->assign.src, ctx);
	if (is_error (dst)) {
		return dst;
	}
	if (is_error (src)) {
		return src;
	}
	scoped_src_loc (expr);
	auto assign = assign_expr (dst, src);
	if (expr->paren) {
		assign = paren_expr (assign);
	}
	return assign;
}

static const expr_t *
proc_branch (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	if (expr->branch.type == pr_branch_call) {
		auto target = expr_process (expr->branch.target, ctx);
		if (is_error (target)) {
			return target;
		}
		auto args = (expr_t *) expr->branch.args;
		if (expr->branch.args) {
			args = new_list_expr (nullptr);
			if (!proc_do_list (&args->list, &expr->branch.args->list, ctx)) {
				return new_error_expr ();
			}
		}
		return function_expr (target, args, ctx);
	} else {
		auto branch = new_expr ();
		branch->type = ex_branch;
		branch->branch = expr->branch;
		branch->branch.target = expr_process (expr->branch.target, ctx);
		branch->branch.index = expr_process (expr->branch.index, ctx);
		branch->branch.test = expr_process (expr->branch.test, ctx);
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
proc_return (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto ret_val = expr->retrn.ret_val;
	if (ret_val) {
		ret_val = expr_process (ret_val, ctx);
	}
	if (current_func->return_imp) {
		return current_func->return_imp (current_func, ret_val);
	}
	if (expr->retrn.at_return) {
		return at_return_expr (current_func, ret_val);
	} else {
		return return_expr (current_func, ret_val);
	}
}

static const expr_t *
proc_swizzle (const expr_t *expr, rua_ctx_t *ctx)
{
	// To get here, a field expression was processed and then passed to an
	// inline function.
	// FIXME this is probably a bug in argument setup for inline functions
	if (!expr->swizzle.type) {
		internal_error (expr, "unprocessed swizzle");
	}
	return expr;
}

static const expr_t *
proc_list (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto list = new_list_expr (nullptr);
	if (!proc_do_list (&list->list, &expr->list, ctx)) {
		return new_error_expr ();
	}
	return list;
}

static const expr_t *
proc_type (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	return process_type (expr, ctx);
}

static const expr_t *
proc_incop (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto e = expr_process (expr->incop.expr, ctx);
	if (is_error (e)) {
		return e;
	}
	if (!is_lvalue (e)) {
		return error (expr, "invalid lvalue for %c%c",
					  expr->incop.op, expr->incop.op);
	}
	return new_incop_expr (expr->incop.op, e, expr->incop.postop);
}

static const expr_t *
proc_cond (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	auto test = expr_process (expr->cond.test, ctx);
	auto true_expr = expr_process (expr->cond.true_expr, ctx);
	auto false_expr = expr_process (expr->cond.false_expr, ctx);
	if (is_error (test)) {
		return test;
	}
	if (is_error (true_expr)) {
		return true_expr;
	}
	if (is_error (false_expr)) {
		return false_expr;
	}
	auto true_type = get_type (true_expr);
	auto false_type = get_type (false_expr);
	if (!type_same (true_type, false_type)) {
		if (type_promotes (true_type, false_type)) {
			false_expr = cast_expr (true_type, false_expr);
		} else if (type_promotes (false_type, true_type)) {
			true_expr = cast_expr (false_type, true_expr);
		} else {
			return error (expr, "incompatible types in conditional expression");
		}
	}
	return new_cond_expr (test, true_expr, false_expr);
}

static const type_t *
decl_function (const type_t *type, const expr_t *t, const symbol_t *sym,
			   rua_ctx_t *ctx)
{
	scoped_src_loc (t);
	if (type && is_func (type)) {
		error (t, "'%s' declared as a function returning a function",
			   sym->name);
	} else if (type && is_array (type)) {
		error (t, "'%s' declared as function returning an array", sym->name);
	}
	// FIXME not sure I like this setup for @function
	auto params = t->typ.params;
	auto ftype = params->typ.type;
	if (type) {
		ftype = append_type (ftype, type);
		ftype = find_type (ftype);
	}
	return ftype;
}

static const type_t *
decl_array (const type_t *type, const expr_t *t, const symbol_t *sym,
			rua_ctx_t *ctx)
{
	scoped_src_loc (t);
	if (type && is_func (type)) {
		error (t, "declaration of '%s' as array of functions", sym->name);
	}
	auto params = new_list_expr (nullptr);
	if (!proc_do_list (&params->list, &t->typ.params->list, ctx)) {
		return type;
	}
	expr_prepend_expr (params, new_type_expr (type));
	auto type_expr = type_function (QC_AT_ARRAY, params);
	return resolve_type (type_expr, ctx);
}

static const type_t *
decl_pointer (const type_t *type, const expr_t *t, const symbol_t *sym,
			  rua_ctx_t *ctx)
{
	scoped_src_loc (t);
	auto params = new_list_expr (new_type_expr (type));
	auto type_expr = type_function (QC_AT_POINTER, params);
	return resolve_type (type_expr, ctx);
}

static const type_t *
decl_field (const type_t *type, const expr_t *t, const symbol_t *sym,
			rua_ctx_t *ctx)
{
	scoped_src_loc (t);
	auto params = new_list_expr (new_type_expr (type));
	auto type_expr = type_function (QC_AT_FIELD, params);
	return resolve_type (type_expr, ctx);
}

specifier_t
spec_process (specifier_t spec, rua_ctx_t *ctx)
{
	if (spec.type && spec.type_expr) {
		internal_error (0, "both type and type_expr set");
	}
	if (spec.storage == sc_local || spec.storage == sc_param) {
		spec.is_function = false;
	}
	if (!spec.type_expr) {
		spec = default_type (spec, spec.sym);
	}
	if (!spec.type_list) {
		if (spec.type_expr) {
			spec.type = resolve_type (spec.type_expr, ctx);
			spec.type_list = nullptr;
		}
		return spec;
	}
	if (spec.type_list->type != ex_list) {
		internal_error (spec.type_list, "not a list");
	}
	int num_types = list_count (&spec.type_list->list);
	const expr_t *type_list[num_types];
	auto type = spec.type; // core type (int etc)
	if (spec.type_expr) {
		type = resolve_type (spec.type_expr, ctx);
	}
	//const type_t *type = nullptr;
	// other than fields, the type list is built up by appending types
	// to the list, but it's the final type in the list that takes the
	// core type, so extract them in reverse for a forward loop
	list_scatter_rev (&spec.type_list->list, type_list);
	for (int i = 0; i < num_types; i++) {
		auto t = type_list[i];
		if (t->type != ex_type) {
			internal_error (t, "not a type expr");
		}
		switch (t->typ.op) {
			case QC_AT_FUNCTION:
				type = decl_function (type, t, spec.sym, ctx);
				break;
			case QC_AT_ARRAY:
				type = decl_array (type, t, spec.sym, ctx);
				break;
			case QC_AT_POINTER:
				type = decl_pointer (type, t, spec.sym, ctx);
				break;
			case QC_AT_FIELD:
				type = decl_field (type, t, spec.sym, ctx);
				break;
			default:
				internal_error (t, "unexpected type op: %d", t->typ.op);
		}
	}
	spec.type_list = nullptr;
	spec.type = type;
	return spec;
}

static const expr_t *
proc_decl (const expr_t *expr, rua_ctx_t *ctx)
{
	scoped_src_loc (expr);
	expr_t *block = nullptr;
	auto decl_spec = expr->decl.spec;
	if (decl_spec.type && decl_spec.type_expr) {
		internal_error (0, "both type and type_expr set");
	}
	int count = list_count (&expr->decl.list);
	if (!count) {
		return block;
	}
	if (decl_spec.storage == sc_local) {
		scoped_src_loc (expr);
		block = new_block_expr (nullptr);
	}
	const expr_t *decls[count];
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
			init = expr_process (init, ctx);
			if (is_error (init)) {
				return init;
			}
			pr.loc = decl->assign.dst->loc;
			sym = decl->assign.dst->symbol;
		} else if (decl->type == ex_symbol) {
			sym = decl->symbol;
		} else {
			internal_error (decl, "not a symbol");
		}
		auto spec = decl_spec;
		spec.sym = sym;
		if (!ctx->extdecl && spec.type_list) {
			// to get here, a concrete declaration is being made
			spec.is_generic = false;
			spec = spec_process (spec, ctx);
		}
		if (sym && !spec.type_expr) {
			spec.type = append_type (sym->type, spec.type);
			spec.type = find_type (spec.type);
			sym->type = nullptr;
		}
		auto symtab = expr->decl.symtab;
		ctx->language->parse_declaration (spec, sym, init, symtab, block, ctx);
	}
	edag_flush ();
	return block;
}

static const expr_t *
proc_loop (const expr_t *expr, rua_ctx_t *ctx)
{
	auto test = expr_process (expr->loop.test, ctx);
	auto body = expr_process (expr->loop.body, ctx);
	auto continue_label = expr->loop.continue_label;
	auto continue_body = expr_process (expr->loop.continue_body, ctx);
	auto break_label = expr->loop.break_label;
	bool do_while = expr->loop.do_while;
	bool not = expr->loop.not;
	scoped_src_loc (expr);
	return new_loop_expr (not, do_while, test, body,
						  continue_label, continue_body, break_label);
}

static const expr_t *
proc_select (const expr_t *expr, rua_ctx_t *ctx)
{
	auto test = expr_process (expr->select.test, ctx);
	test = test_expr (test);
	edag_flush ();
	auto true_body = expr_process (expr->select.true_body, ctx);
	edag_flush ();
	auto false_body = expr_process (expr->select.false_body, ctx);
	edag_flush ();
	scoped_src_loc (expr);
	auto select = new_select_expr (expr->select.not, test, true_body, nullptr,
								   false_body);
	select->select.els = expr->select.els;
	return select;
}

static const expr_t *
proc_intrinsic (const expr_t *expr, rua_ctx_t *ctx)
{
	int count = list_count (&expr->intrinsic.operands);
	const expr_t *operands[count + 1];
	list_scatter (&expr->intrinsic.operands, operands);
	auto opcode = expr_process (expr->intrinsic.opcode, ctx);
	for (int i = 0; i < count; i++) {
		operands[i] = expr_process (operands[i], ctx);
	}
	scoped_src_loc (expr);
	auto e = new_expr ();
	e->type = ex_intrinsic;
	e->intrinsic = (ex_intrinsic_t) {
		.opcode = opcode,
		.res_type = expr->intrinsic.res_type,
	};
	list_gather (&e->intrinsic.operands, operands, count);
	return e;
}

static const expr_t *
proc_switch (const expr_t *expr, rua_ctx_t *ctx)
{
	return current_target.proc_switch (expr, ctx);
}

static const expr_t *
proc_caselabel (const expr_t *expr, rua_ctx_t *ctx)
{
	return current_target.proc_caselabel (expr, ctx);
}

static const expr_t *
proc_xvalue (const expr_t *expr, rua_ctx_t *ctx)
{
	auto xvalue = expr->xvalue.expr;
	if (xvalue->type == ex_symbol && xvalue->symbol->sy_type == sy_xvalue) {
		if (expr->xvalue.lvalue) {
			auto xv = xvalue->symbol->xvalue.lvalue;
			if (!xv || !is_lvalue (xv)) {
				return error (xv, "invalid lvalue");
			}
			xvalue = xv;
		} else {
			xvalue = xvalue->symbol->xvalue.rvalue;
		}
	}
	return expr_process (xvalue, ctx);
}

const expr_t *
expr_process (const expr_t *expr, rua_ctx_t *ctx)
{
	if (!expr) {
		return expr;
	}
	static process_f funcs[ex_count] = {
		[ex_label] = proc_label,
		[ex_block] = proc_block,
		[ex_expr] = proc_expr,
		[ex_uexpr] = proc_uexpr,
		[ex_symbol] = proc_symbol,
		[ex_vector] = proc_vector,
		[ex_selector] = proc_selector,
		[ex_message] = proc_message,
		[ex_nil] = proc_nil,
		[ex_value] = proc_value,
		[ex_compound] = proc_compound,
		[ex_assign] = proc_assign,
		[ex_branch] = proc_branch,
		[ex_return] = proc_return,
		[ex_swizzle] = proc_swizzle,
		[ex_list] = proc_list,
		[ex_type] = proc_type,
		[ex_incop] = proc_incop,
		[ex_cond] = proc_cond,
		[ex_field] = proc_field,
		[ex_array] = proc_array,
		[ex_decl] = proc_decl,
		[ex_loop] = proc_loop,
		[ex_select] = proc_select,
		[ex_intrinsic] = proc_intrinsic,
		[ex_switch] = proc_switch,
		[ex_caselabel] = proc_caselabel,
		[ex_xvalue] = proc_xvalue,
	};

	if (expr->type >= ex_count) {
		internal_error (expr, "bad sub-expression type: %d", expr->type);
	}
	if (!funcs[expr->type]) {
		internal_error (expr, "unexpected sub-expression type: %s",
						expr_names[expr->type]);
	}

	bool force_lookup = ctx->force_lookup;
	ctx->force_lookup = true;
	auto proc = funcs[expr->type] (expr, ctx);
	proc = edag_add_expr (proc);
	if (proc && proc->type == ex_process) {
		auto func = current_func;
		if (proc->process.function) {
			current_func = proc->process.function;
		}
		proc = expr_process (proc->process.expr, ctx);
		current_func = func;
	}
	ctx->force_lookup = force_lookup;
	return proc;
}

void
decl_process (const expr_t *expr, rua_ctx_t *ctx)
{
	ctx->extdecl = true;
	expr_process (expr, ctx);
	ctx->extdecl = false;
}
