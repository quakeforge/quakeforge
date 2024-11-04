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
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

const expr_t *
build_function_call (const expr_t *fexpr, const type_t *ftype,
					 const expr_t *params)
{
	int         param_count = 0;
	expr_t     *call;
	const expr_t *err = 0;

	int arg_count = params ? list_count (&params->list) : 0;
	const expr_t *arguments[arg_count + 1];
	if (params) {
		list_scatter_rev (&params->list, arguments);
	}

	for (int i = 0; i < arg_count; i++) {
		auto e = arguments[i];
		if (e->type == ex_error) {
			return e;
		}
		if (e->type != ex_compound) {
			arguments[i] = algebra_optimize (e);
		}
	}

	if (ftype->func.num_params < -1) {
		if (options.code.max_params >= 0
			&& arg_count > options.code.max_params) {
			return error (fexpr, "more than %d parameters",
						  options.code.max_params);
		}
		if (-arg_count > ftype->func.num_params + 1) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = -ftype->func.num_params - 1;
	} else if (ftype->func.num_params >= 0) {
		if (arg_count > ftype->func.num_params) {
			return error (fexpr, "too many arguments");
		} else if (arg_count < ftype->func.num_params) {
			if (!options.traditional)
				return error (fexpr, "too few arguments");
			if (options.warnings.traditional)
				warning (fexpr, "too few arguments");
		}
		param_count = ftype->func.num_params;
	}

	const type_t *arg_types[arg_count + 1];
	// params is reversed (a, b, c) -> c, b, a
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
	if (err) {
		return err;
	}

	bool        emit_args = false;
	if (ftype->func.num_params < 0) {
		emit_args = !ftype->func.no_va_list;
	}
	scoped_src_loc (fexpr);
	call = new_block_expr (0);
	call->block.is_call = 1;
	int         arg_expr_count = 0;
	const expr_t *arg_exprs[arg_count + 1][2];
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
			expr_prepend_expr (args, tmp);

			arg_exprs[arg_expr_count][0] = cast;
			arg_exprs[arg_expr_count][1] = tmp;
			arg_expr_count++;
		} else {
			if (param_qual != pq_out) {
				// out parameters do not need to be sent so no need to cast
			}
			if (param_qual & pq_out) {
				auto inout = new_expr ();
				inout->type = ex_inout;
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
			expr_prepend_expr (args, e);
		}
	}
	if (emit_args) {
		emit_args = false;
		expr_prepend_expr (args, new_args_expr ());
	}
	for (int i = 0; i < arg_expr_count; i++) {
		scoped_src_loc (arg_exprs[i][0]);
		auto assign = assign_expr (arg_exprs[i][1], arg_exprs[i][0]);
		append_expr (call, assign);
	}
	auto ret_type = ftype->func.ret_type;
	call->block.result = new_call_expr (fexpr, args, ret_type);
	return call;
}

const expr_t *
function_expr (const expr_t *fexpr, const expr_t *params)
{
	if (params) {
		for (auto p = params->list.head; p; p = p->next) {
			p->expr = convert_name (p->expr);
		}
	}

	if (fexpr->type == ex_symbol && fexpr->symbol->sy_type == sy_type) {
		return constructor_expr (fexpr, params);
	}

	fexpr = find_function (fexpr, params);
	fexpr = convert_name (fexpr);
	if (is_error (fexpr)) {
		return fexpr;
	}

	auto ftype = get_type (fexpr);
	if (ftype->type != ev_func) {
		if (fexpr->type == ex_symbol)
			return error (fexpr, "Called object \"%s\" is not a function",
						  fexpr->symbol->name);
		else
			return error (fexpr, "Called object is not a function");
	}

	if (fexpr->type == ex_symbol && params && is_string_val (params)) {
		// FIXME eww, I hate this, but it's needed :(
		// FIXME make a qc hook? :)
		if (strncmp (fexpr->symbol->name, "precache_sound", 14) == 0)
			PrecacheSound (expr_string (params), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_model", 14) == 0)
			PrecacheModel (expr_string (params), fexpr->symbol->name[14]);
		else if (strncmp (fexpr->symbol->name, "precache_file", 13) == 0)
			PrecacheFile (expr_string (params), fexpr->symbol->name[13]);
	}

	return build_function_call (fexpr, ftype, params);
}

const expr_t *
return_expr (function_t *f, const expr_t *e)
{
	const type_t *t;
	const type_t *ret_type = unalias_type (f->type->func.ret_type);

	e = convert_name (e);
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
	ret_expr->retrn.at_return = 1;
	return ret_expr;
}
