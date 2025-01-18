/*
	expr_call.c

	Function call expression construction and manipulations

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

static const expr_t *
check_too_few (const expr_t *fexpr, int arg_count, int param_count)
{
	if (arg_count < param_count) {
		if (!options.traditional) {
			return error (fexpr, "too few arguments");
		}
		// qcc was slack about missing parameters
		if (options.warnings.traditional) {
			warning (fexpr, "too few arguments");
		}
	}
	return nullptr;
}

static const expr_t *
check_arg_count (const expr_t *fexpr, const type_t *ftype,
				 int arg_count, int *param_count)
{
	if (ftype->func.num_params < 0) {
		*param_count = -ftype->func.num_params - 1;
		if (options.code.max_params >= 0
			&& arg_count > options.code.max_params) {
			return error (fexpr, "more than %d arguments",
						  options.code.max_params);
		}
	} else {
		*param_count = ftype->func.num_params;
		if (arg_count > ftype->func.num_params) {
			return error (fexpr, "too many arguments");
		}
	}
	return check_too_few (fexpr, arg_count, *param_count);
}

static const expr_t *
optimize_arguments (const expr_t **arguments, int arg_count)
{
	const expr_t *err = 0;

	for (int i = 0; i < arg_count; i++) {
		if (is_error (arguments[i])) {
			err = arguments[i];
		} else if (arguments[i]->type != ex_compound) {
			arguments[i] = algebra_optimize (arguments[i]);
		}
	}

	return err;
}

static const expr_t *
check_arg_types (const expr_t **arguments, const type_t **arg_types,
				 int arg_count, int param_count,
				 const expr_t *fexpr, const type_t *ftype)
{
	const expr_t *err = 0;
	for (int i = 0; i < arg_count; i++) {
		auto e = arguments[i];
		const type_t *t;

		if (e->type == ex_compound) {
			if (i < param_count) {
				t = ftype->func.param_types[i];
			} else {
				return error (e, "cannot pass compound initializer "
							  "through ...");
			}
		} else {
			t = get_type (e);
		}
		if (!t) {
			return e;
		}

		if (!type_size (t)) {
			err = error (e, "type of formal parameter %d is incomplete",
						 i + 1);
		}
		if (!is_array (t) && value_too_large (t)) {
			err = error (e, "formal parameter %d is too large to be passed by"
						 " value", i + 1);
		}
		if (i < param_count) {
			auto param_type = ftype->func.param_types[i];
			auto param_qual = ftype->func.param_quals[i];

			if (is_reference (param_type) && param_qual != pq_in) {
				internal_error (e, "qualified reference param (not yet)");
			}
			if (is_reference (param_type) && !is_reference (t)) {
				if (!is_lvalue (e)) {
					err = error (e, "cannot pass non-lvalue by reference");
				}
				if (!err && dereference_type (param_type) != t) {
					err = reference_error (e, param_type, t);
				}
			} else if (!is_reference (param_type) && is_reference (t)) {
				t = dereference_type (t);
			}
			if (e->type == ex_nil) {
				t = param_type;
			}
			if (param_qual == pq_out || param_qual == pq_inout) {
				//FIXME should be able to use something like *foo() as
				//an out or inout arg
				if (!is_lvalue (e) || has_function_call (e)) {
					error (e, "lvalue required for %s parameter",
						   param_qual & pq_in ? "inout" : "out");
				}
				if (param_qual == pq_inout
					&& !type_assignable (param_type, t)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  param_type, t);
				}
				// check assignment FROM parameter is ok, but only if
				// there wasn't an earlier error so param mismatch doesn't
				// get double-reported for inout params.
				if (!err && !type_assignable (t, param_type)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  t, param_type);
				}
			} else {
				if (!type_assignable (param_type, t)) {
					err = param_mismatch (e, i + 1, fexpr->symbol->name,
										  param_type, t);
				}
			}
			t = param_type;
		} else {
			if (e->type == ex_nil) {
				t = type_nil;
			}
			if (options.code.promote_float) {
				if (is_scalar (get_type (e)) && is_float (get_type (e))) {
					t = &type_double;
				}
			} else {
				if (is_scalar (get_type (e)) && is_double (get_type (e))) {
					if (!e->implicit) {
						warning (e, "passing double into ... function");
					}
					if (is_constant (e)) {
						// don't auto-demote non-constant doubles
						t = &type_float;
					}
				}
			}
			if (current_target.vararg_int) {
				current_target.vararg_int (e);
			}
		}
		arg_types[i] = t;
	}
	return err;
}

static void
build_call_scope (symbol_t *fsym, const expr_t **arguments)
{
	auto metafunc = fsym->metafunc;
	auto func = metafunc->func;
	auto params = func->parameters;

	int i = 0;
	for (auto p = fsym->params; p; p = p->next, i++) {
		if (!p->selector && !p->type && !p->name) {
			internal_error (0, "inline variadic not implemented");
		}
		if (!p->type) {
			continue;						// non-param selector
		}
		if (is_void (p->type)) {
			if (p->name) {
				error (0, "invalid parameter type for %s", p->name);
			} else if (p != fsym->params || p->next) {
				error (0, "void must be the only parameter");
				continue;
			} else {
				continue;
			}
		}
		if (!p->name) {
			notice (0, "parameter name omitted");
			continue;
		}
		auto psym = new_symbol (p->name);
		psym->sy_type = sy_expr;
		psym->expr = arguments[i];
		symtab_addsymbol (params, psym);
	}
}

static expr_t *
build_intrinsic_call (const expr_t *expr, symbol_t *fsym, const type_t *ftype,
					  const expr_t **arguments, int arg_count, rua_ctx_t *ctx)
{
	auto call = new_intrinsic_expr (nullptr);
	call->intrinsic.opcode = expr->intrinsic.opcode;
	call->intrinsic.res_type = ftype->func.ret_type;
	list_append_list (&call->intrinsic.operands,
					  &expr->intrinsic.operands);
	if (expr->intrinsic.extra) {
		if (expr->intrinsic.extra->type != ex_list) {
			internal_error (expr->intrinsic.extra, "not a list");
		}

		build_call_scope (fsym, arguments);
		if (current_target.setup_intrinsic_symtab) {
			auto metafunc = fsym->metafunc;
			auto func = metafunc->func;
			current_target.setup_intrinsic_symtab (func->locals);
		}

		auto extra = &expr->intrinsic.extra->list;
		int extra_count = list_count (extra);
		const expr_t *extra_args[extra_count + 1] = {};
		list_scatter (extra, extra_args);

		auto metafunc = fsym->metafunc;
		auto func = metafunc->func;
		auto scope = current_symtab;
		current_symtab = func->locals;
		for (int i = 0; i < extra_count; i++) {
			extra_args[i] = expr_process (extra_args[i], ctx);
		}
		current_symtab = scope;
		list_gather (&call->intrinsic.operands, extra_args, extra_count);
	} else {
		for (int i = 0; i < arg_count; i++) {
			if (is_reference (get_type (arguments[i]))) {
				arguments[i] = pointer_deref (arguments[i]);
			}
		}
		list_gather (&call->intrinsic.operands, arguments, arg_count);
	}
	return call;
}

static const expr_t *
inline_return_expr (function_t *func, const expr_t *val)
{
	if (!func->return_val && val) {
		return error (val, "returning a value for a void function");
	}
	if (func->return_val && !val) {
		return error (val, "return from non-void function without a value");
	}
	auto ret = new_block_expr (nullptr);
	if (val) {
		append_expr (ret, assign_expr (func->return_val, val));
	}
	if (func->return_label) {
		append_expr (ret, goto_expr (func->return_label));
	}
	return ret;
}

static expr_t *
build_inline_call (symbol_t *fsym, const type_t *ftype,
				   const expr_t **arguments, int arg_count)
{
	auto metafunc = fsym->metafunc;
	auto func = metafunc->func;

	build_call_scope (fsym, arguments);

	auto locals = func->locals;
	auto call = new_block_expr (nullptr);
	call->block.scope = locals;

	if (!is_void (ftype->func.ret_type)) {
		auto spec = (specifier_t) {
			.type = ftype->func.ret_type,
			.storage = sc_local,
		};
		auto decl = new_decl_expr (spec, locals);
		auto ret = new_symbol (".ret");
		append_decl (decl, ret, nullptr);
		append_expr (call, decl);
		call->block.result = new_symbol_expr (ret);
	}
	func->return_val = call->block.result;

	auto expr = metafunc->expr;
	if (expr->type == ex_block) {
		expr->block.scope->parent = locals;
	}
	append_expr (call, expr);

	func->return_label = new_label_expr ();
	append_expr (call, func->return_label);

	func->return_imp = inline_return_expr;

	auto proc = new_process_expr (call);
	proc->process.function = func;

	return proc;
}

static expr_t *
build_args (const expr_t *(*arg_exprs)[2], int *arg_expr_count,
			const expr_t **arguments, const type_t **arg_types,
			int arg_count, int param_count, const type_t *ftype)
{
	bool emit_args = ftype->func.num_params < 0 && !ftype->func.no_va_list;
	expr_t     *args = new_list_expr (0);
	// args is built in reverse order so it matches params
	for (int i = 0; i < arg_count; i++) {
		auto param_qual = i < param_count ? ftype->func.param_quals[i] : pq_in;
		if (emit_args && i == param_count) {
			expr_prepend_expr (args, new_args_expr ());
			emit_args = false;
		}

		auto e = arguments[i];
		if (e->type == ex_compound || e->type == ex_multivec) {
			scoped_src_loc (e);
			e = initialized_temp_expr (arg_types[i], e);
		}
		// FIXME this is target-specific info and should not be in the
		// expression tree
		// That, or always use a temp, since it should get optimized out
		if (has_function_call (e)) {
			scoped_src_loc (e);
			auto cast = cast_expr (arg_types[i], e);
			auto tmp = new_temp_def_expr (arg_types[i]);
			arg_exprs[*arg_expr_count][0] = cast;
			arg_exprs[*arg_expr_count][1] = tmp;
			++*arg_expr_count;
			e = tmp;
		} else {
			if (param_qual != pq_out) {
				// out parameters do not need to be sent so no need to cast
			}
			if (param_qual & pq_out) {
				auto inout = new_expr ();
				inout->type = ex_inout;
				if (is_reference(get_type (e))) {
					e = pointer_deref (e);
				}
				if (param_qual == pq_inout) {
					inout->inout.in = cast_expr (arg_types[i], e);
				}
				inout->inout.out = e;
				e = inout;
			} else {
				if (is_reference (arg_types[i])) {
					if (is_reference (get_type (e))) {
						// just copy the param, so no op
					} else {
						e = address_expr (e, nullptr);
					}
				} else {
					if (is_reference (get_type (e))) {
						e = pointer_deref (e);
					}
					e = cast_expr (arg_types[i], e);
				}
			}
		}
		expr_prepend_expr (args, e);
	}
	// if no args are provided for ..., the args expression wont have
	// been emitted
	if (emit_args) {
		expr_prepend_expr (args, new_args_expr ());
	}
	return args;
}

const expr_t *
build_function_call (const expr_t *fexpr, const type_t *ftype,
					 const expr_t *args, rua_ctx_t *ctx)
{
	int         param_count = 0;
	const expr_t *err = 0;

	int arg_count = args ? list_count (&args->list) : 0;
	const expr_t *arguments[arg_count + 1] = {};
	const type_t *arg_types[arg_count + 1] = {};

	if (args) {
		// args is reversed (a, b, c) -> c, b, a
		list_scatter_rev (&args->list, arguments);
	}

	if ((err = optimize_arguments (arguments, arg_count))) {
		return err;
	}
	if ((err = check_arg_count (fexpr, ftype, arg_count, &param_count))) {
		return err;
	}
	if ((err = check_arg_types (arguments, arg_types, arg_count, param_count,
								fexpr, ftype))) {
		return err;
	}

	scoped_src_loc (fexpr);
	if (fexpr->type == ex_symbol && fexpr->symbol->sy_type == sy_func
		&& fexpr->symbol->metafunc->expr) {
		auto fsym = fexpr->symbol;
		auto metafunc = fsym->metafunc;
		auto expr = metafunc->expr;
		if (expr->type == ex_intrinsic) {
			return build_intrinsic_call (expr, fsym, ftype,
										 arguments, arg_count, ctx);
		}
		if (metafunc->can_inline) {
			return build_inline_call (fsym, ftype, arguments, arg_count);
		}
		internal_error (fexpr, "calls to inline functions that cannot be "
						"inlined not implemented");
	} else {
		auto call = new_block_expr (nullptr);
		call->block.scope = current_symtab;
		call->block.is_call = 1;
		int         num_args = 0;
		const expr_t *arg_exprs[arg_count + 1][2];
		auto arg_list = build_args (arg_exprs, &num_args, arguments, arg_types,
									arg_count, param_count, ftype);
		for (int i = 0; i < num_args; i++) {
			scoped_src_loc (arg_exprs[i][0]);
			auto assign = assign_expr (arg_exprs[i][1], arg_exprs[i][0]);
			append_expr (call, assign);
		}
		auto ret_type = ftype->func.ret_type;
		call->block.result = new_call_expr (fexpr, arg_list, ret_type);
		return call;
	}
}

const expr_t *
function_expr (const expr_t *fexpr, const expr_t *args, rua_ctx_t *ctx)
{
	if (fexpr->type == ex_type) {
		return constructor_expr (fexpr, args);
	}

	fexpr = find_function (fexpr, args, ctx);
	if (is_error (fexpr)) {
		return fexpr;
	}

	auto ftype = get_type (fexpr);
	if (is_ptr (ftype) && is_func (dereference_type (ftype))) {
		ftype = dereference_type (ftype);
	}
	if (!is_func (ftype)) {
		if (fexpr->type == ex_symbol)
			return error (fexpr, "Called object \"%s\" is not a function",
						  fexpr->symbol->name);
		else
			return error (fexpr, "Called object is not a function");
	}

	if (fexpr->type == ex_symbol && args
		&& args->list.head && is_string_val (args->list.head->expr)) {
		auto arg = args->list.head->expr;
		// FIXME eww, I hate this, but it's needed :(
		// FIXME make a qc hook? :)
		if (strncmp (fexpr->symbol->name, "precache_sound", 14) == 0)
			PrecacheSound (expr_string (arg), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_model", 14) == 0)
			PrecacheModel (expr_string (arg), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_file", 13) == 0)
			PrecacheFile (expr_string (arg), fexpr->symbol->name[13]);
	}

	return build_function_call (fexpr, ftype, args, ctx);
}

const expr_t *
return_expr (function_t *f, const expr_t *e)
{
	const type_t *t;
	const type_t *ret_type = unalias_type (f->type->func.ret_type);

	if (!e) {
		if (!is_void(ret_type)) {
			if (options.traditional) {
				if (options.warnings.traditional)
					warning (e,
							 "return from non-void function without a value");
				// force a nil return value in case qf code is being generated
				e = new_nil_expr ();
			} else {
				e = error (e, "return from non-void function without a value");
				return e;
			}
		}
		// the traditional check above may have set e
		if (!e) {
			return new_return_expr (0);
		}
	}

	if (e->type == ex_compound || e->type == ex_multivec) {
		scoped_src_loc (e);
		e = initialized_temp_expr (ret_type, e);
	} else {
		e = algebra_optimize (e);
	}

	t = get_type (e);

	if (!t) {
		return e;
	}
	if (is_void(ret_type)) {
		if (!options.traditional)
			return error (e, "returning a value for a void function");
		if (options.warnings.traditional)
			warning (e, "returning a value for a void function");
	}
	if (e->type == ex_bool) {
		e = convert_from_bool (e, ret_type);
	}
	if (is_reference (t) && !is_reference (ret_type)) {
		e = pointer_deref (e);
		t = dereference_type (t);
	}
	if (is_float(ret_type) && is_int_val (e)) {
		e = cast_expr (&type_float, e);
		t = &type_float;
	}
	if (is_void(t)) {
		if (e->type == ex_nil) {
			t = ret_type;
			e = convert_nil (e, t);
		} else {
			if (!options.traditional)
				return error (e, "void value not ignored as it ought to be");
			if (options.warnings.traditional)
				warning (e, "void value not ignored as it ought to be");
			//FIXME does anything need to be done here?
		}
	}
	if (!type_assignable (ret_type, t)) {
		if (!options.traditional)
			return error (e, "type mismatch for return value of %s: %s -> %s",
						  f->sym->name, get_type_string (t),
						  get_type_string (ret_type));
		if (options.warnings.traditional)
			warning (e, "type mismatch for return value of %s",
					 f->sym->name);
	} else {
		if (ret_type != t) {
			e = cast_expr (ret_type, e);
			t = f->sym->type->func.ret_type;
		}
	}
	if (e->type == ex_vector) {
		e = assign_expr (new_temp_def_expr (t), e);
	}
	return new_return_expr (e);
}

const expr_t *
at_return_expr (function_t *f, const expr_t *e)
{
	const type_t *ret_type = unalias_type (f->type->func.ret_type);

	if (!is_void(ret_type)) {
		return error (e, "use of @return in non-void function");
	}
	if (is_nil (e)) {
		// int or pointer 0 seems reasonable
		return new_return_expr (new_int_expr (0, false));
	} else if (!is_function_call (e)) {
		return error (e, "@return value not a function");
	}
	const expr_t *call_expr = e->block.result->branch.target;
	const auto call_type = get_type (call_expr);
	if (!is_func (call_type) && !call_type->func.void_return) {
		return error (e, "@return function not void_return");
	}
	expr_t     *ret_expr = new_return_expr (e);
	ret_expr->retrn.at_return = true;
	return ret_expr;
}
