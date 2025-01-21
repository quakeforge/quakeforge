/*
	expr_type.c

	type expressions

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

#include "QF/progs.h"
#include "QF/sys.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/evaluate_type.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

typedef struct {
	gentype_t  *types;
	int         num_types;
	codespace_t *code;
	defspace_t *data;
	strpool_t  *strings;
	sys_jmpbuf  jmpbuf;
	def_t      *args[3];
	def_t      *funcs[tf_num_functions];
	rua_ctx_t  *rua_ctx;
} comp_ctx_t;

typedef struct {
	const char *name;
	const char *(*check_params) (int arg_count, const expr_t **args);
	const type_t *(*resolve) (int arg_count, const expr_t **args,
							  rua_ctx_t *ctx);
	const type_t **(*expand) (int arg_count, const expr_t **args,
							  rua_ctx_t *ctx);
	const expr_t *(*evaluate) (int arg_count, const expr_t **args,
							   rua_ctx_t *ctx);
	def_t *(*compute) (int arg_count, const expr_t **args, comp_ctx_t *ctx);
} type_func_t;

static type_func_t type_funcs[];

static bool
check_type (const expr_t *arg)
{
	if (arg->type == ex_symbol && arg->symbol->sy_type == sy_type_param) {
		return true;
	}
	if (arg->type == ex_symbol && arg->symbol->sy_type == sy_type) {
		return true;
	}
	if (arg->type == ex_type) {
		return true;
	}
	if (arg->type == ex_list) {
		for (auto le = arg->list.head; le; le = le->next) {
			if (!check_type (le->expr)) {
				return false;
			}
		}
		return true;
	}
	return false;
}

static bool
check_property (const expr_t *arg)
{
	if (arg->type != ex_type && !arg->typ.property) {
		return false;
	}
	return true;
}

static bool __attribute__((pure))
check_int (const expr_t *arg)
{
	if (arg->type == ex_expr || arg->type == ex_uexpr) {
		if (!is_int (arg->expr.type)) {
			return false;
		}
		return check_int (arg->expr.e1)
			&& arg->type == ex_expr && check_int (arg->expr.e2);
	}
	if (is_integral_val (arg)) {
		return true;
	}
	if ((arg->type != ex_symbol || arg->symbol->sy_type != sy_type_param)
		&& arg->type != ex_type) {
		return false;
	}
	int op = arg->type ? arg->typ.op : arg->symbol->expr->typ.op;
	if (op != QC_AT_WIDTH && op != QC_AT_ROWS && op != QC_AT_COLS) {
		return false;
	}
	return true;
}

static const char *
single_type (int arg_count, const expr_t **args)
{
	if (arg_count < 1) {
		return "too few arguments";
	}
	if (arg_count > 1) {
		return "too many arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	return nullptr;
}

static const char *
single_type_property (int arg_count, const expr_t **args)
{
	if (arg_count < 1) {
		return "too few arguments";
	}
	if (arg_count > 2) {
		return "too many arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	if (!check_property (args[1])) {
		return "second parameter must be a type property";
	}
	return nullptr;
}

static const char *
single_type_opt_int (int arg_count, const expr_t **args)
{
	if (arg_count < 1) {
		return "too few arguments";
	}
	if (arg_count > 2) {
		return "too many arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	if (arg_count > 1 && !check_int (args[1])) {
		return "second parameter must be int";
	}
	return nullptr;
}

static const char *
single_type_opt_int_pair (int arg_count, const expr_t **args)
{
	if (arg_count < 1) {
		return "too few arguments";
	}
	if (arg_count > 3) {
		return "too many arguments";
	}
	if (arg_count == 2) {
		return "must have one or three arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	if (arg_count > 1 && (!check_int (args[1]) || !check_int (args[2]))) {
		return "second and third parameters must be int";
	}
	return nullptr;
}

static const expr_t *
evaluate_property (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (!type) {
		return error (args[0], "could not resolve type");
	}
	if (!type->property) {
		return error (args[0], "type doesn't support properties");
	} else {
		auto property = args[1]->typ.property;
		if (property->params) {
			scoped_src_loc (property->params);
			auto params = new_list_expr (nullptr);
			if (!proc_do_list (&params->list, &property->params->list,
							   ctx)) {
				return nullptr;
			}
			auto prop = new_type_expr (nullptr);
			property = new_attrfunc (property->name, params);
			args[1] = prop;
		}
		auto e = type->property (type, property);
		if (e->type == ex_type) {
			e = eval_type (e, ctx);
		}
		return e;
	}
}

static const expr_t *
evaluate_int (const expr_t *expr, rua_ctx_t *ctx)
{
	expr = expr_process (expr, ctx);
	if (expr->type == ex_expr || expr->type == ex_uexpr) {
		auto e = new_expr ();
		*e = *expr;
		e->expr.e1 = evaluate_int (expr->expr.e1, ctx);
		if (expr->type == ex_expr) {
			e->expr.e2 = evaluate_int (expr->expr.e2, ctx);
		}
		return fold_constants (e);
	}
	if (is_integral_val (expr)) {
		return expr;
	}
	if (expr->type != ex_type) {
		internal_error (expr, "invalid type op");
	}
	int op = expr->typ.op;
	auto type = expr->typ.type;
	if (!type) {
		return error (expr, "invalid type passed to %s", type_funcs[op].name);
	}
	if (op == QC_AT_WIDTH) {
		return new_int_expr (type_width (type), false);
	}
	if (op == QC_AT_ROWS) {
		return new_int_expr (type_rows (type), false);
	}
	if (op == QC_AT_COLS) {
		return new_int_expr (type_cols (type), false);
	}
	internal_error (expr, "invalid type op");
}

static const expr_t *
evaluate_int_op (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	return evaluate_int (args[0], ctx);
}

static const type_t *
resolve_property (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		if (!type->property) {
			error (args[0], "type doesn't support properties");
		} else {
			auto e = type->property (type, args[1]->typ.property);
			if (is_error (e)) {
				type = nullptr;
			} else {
				type = resolve_type (e, ctx);
			}
		}
	}
	return type;
}

static const type_t *
resolve_function (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	return &type_func;//FIXME
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = field_type (type);//FIXME wrong type
		type = find_type (type);
	}
	return type;
}

static const type_t *
resolve_field (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = field_type (type);
		type = find_type (type);
	}
	return type;
}

static const type_t *
resolve_pointer (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	type = pointer_type (type);
	return type;
}

static const type_t *
resolve_array (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	int count = 0;
	if (arg_count > 1) {
		auto count_expr = evaluate_int (args[1], ctx);
		if (is_error (count_expr)) {
			return nullptr;
		}
		count = expr_integral (count_expr);
	}
	type = array_type (type, count);
	return type;
}

static const type_t *
resolve_base (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = base_type (type);
	}
	return type;
}

static const type_t *
resolve_vector (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		int width = 0;
		if (arg_count > 1) {
			auto width_expr = evaluate_int (args[1], ctx);
			if (is_error (width_expr)) {
				return nullptr;
			}
			width = expr_integral (width_expr);
		} else {
			width = type_width (type);
		}
		type = vector_type (type, width);
		if (!type) {
			error (args[0], "invalid @vector");
		}
	}
	return type;
}

static const type_t *
resolve_matrix (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		int rows = 0;
		int cols = 0;
		if (arg_count > 1) {
			auto rows_expr = evaluate_int (args[2], ctx);
			auto cols_expr = evaluate_int (args[1], ctx);
			if (is_error (rows_expr) || is_error (cols_expr)) {
				return nullptr;
			}
			rows = expr_integral (rows_expr);
			cols = expr_integral (cols_expr);
		} else {
			cols = type_cols (type);
			rows = type_rows (type);
		}
		type = matrix_type (type, cols, rows);
		if (!type) {
			error (args[0], "invalid @matrix");
		}
	}
	return type;
}

static const type_t *
resolve_int (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = int_type (type);
	}
	return type;
}

static const type_t *
resolve_uint (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = uint_type (type);
	}
	return type;
}

static const type_t *
resolve_bool (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = bool_type (type);
	}
	return type;
}

static const type_t *
resolve_float (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	auto type = resolve_type (args[0], ctx);
	if (type) {
		type = float_type (type);
	}
	return type;
}

static const type_t **
expand_vector (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	const type_t *base = resolve_type (args[0], ctx);
	if (!is_bool (base) && !is_scalar (base)) {
		error (args[0], "invalid vector component type");
		return nullptr;
	}

	if (arg_count == 2) {
		auto comps_expr = evaluate_int (args[1], ctx);
		if (is_error (comps_expr)) {
			return nullptr;
		}
		int comps = expr_integral (comps_expr);
		if (comps < 1 || comps > 4) {
			error (args[1], "invalid vector component count");
			return nullptr;
		}

		const type_t **types = malloc (sizeof(type_t[2]));
		types[0] = vector_type (base, comps);
		types[1] = nullptr;
		return types;
	} else {
		const type_t **types = malloc (sizeof(type_t *[5]));
		for (int i = 0; i < 4; i++) {
			types[i] = vector_type (base, i + 1);
		}
		types[4] = nullptr;
		return types;
	}
}

static const type_t **
expand_matrix (int arg_count, const expr_t **args, rua_ctx_t *ctx)
{
	const type_t *base = resolve_type (args[0], ctx);
	if (!is_scalar (base)) {
		error (args[0], "invalid matrix component type");
		return nullptr;
	}

	// @matrix doesn't include vectors for generic parameters
	if (arg_count == 3) {
		auto rows_expr = evaluate_int (args[2], ctx);
		auto cols_expr = evaluate_int (args[1], ctx);
		if (is_error (rows_expr) || is_error (cols_expr)) {
			return nullptr;
		}
		int rows = expr_integral (rows_expr);
		int cols = expr_integral (cols_expr);

		const type_t **types = malloc (sizeof(type_t *[2]));
		types[0] = matrix_type (base, cols, rows);
		types[1] = nullptr;
		return types;
	} else {
		const type_t **types = malloc (sizeof(type_t *[3*3 + 1]));
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				types[i*3 + j] = matrix_type (base, i + 2, j + 2);
			}
		}
		types[3*3] = nullptr;
		return types;
	}
}

static def_t *compute_tmp (comp_ctx_t *ctx);
static def_t *compute_sized_tmp (int size, comp_ctx_t *ctx);
static pr_string_t compute_str (const char *, comp_ctx_t *ctx);
static def_t *compute_type (const expr_t *arg, comp_ctx_t *ctx);
static def_t *compute_val (const expr_t *arg, comp_ctx_t *ctx);

#define BASE(b, base) (((base) & 3) << OP_##b##_SHIFT)
#define DBASE(b, d) ((d) ? BASE(b, ((def_t*)(d))->reg) : 0)
#define DOFFSET(d) ((d) ? ((def_t*)(d))->offset : 0)
#define OP(a, b, c, op) ((op) | DBASE(A, a) | DBASE(B, b) | DBASE(C, c))
#define I(_op, _a, _b, _c) &(dstatement_t){ \
	.op = OP(_a,_b,_c,_op),                 \
	.a = DOFFSET(_a),                       \
	.b = DOFFSET(_b),                       \
	.c = DOFFSET(_c),                       \
}
#define C(op, a, b, c) codespace_addcode (ctx->code, I(op, a, b, c), 1)

static def_t *
compute_property (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	auto property = args[1]->typ.property;
	int count = property->params ? list_count (&property->params->list) : 0;
	// FIXME assumes simple 1-component types
	auto attr_params = compute_sized_tmp (2 + 2 * count, ctx);
	def_t ndef = { .offset = attr_params->offset + 0, .space = ctx->data };
	def_t cdef = { .offset = attr_params->offset + 1, .space = ctx->data };
	D_STRING (&ndef) = compute_str (property->name, ctx);
	D_INT (&cdef) = count;
	if (count) {
		const expr_t *params[count];
		list_scatter (&property->params->list, params);
		for (int i = 0; i < count; i++) {
			auto p = compute_type (params[i], ctx);
			auto s = codespace_newstatement (ctx->code);
			*s = (dstatement_t) {
				.op = OP_STORE_A_1,
				.a = attr_params->offset + 2 + i,
				.b = 0,
				.c = DOFFSET(p),
			};
		}
	}
	C (OP_STORE_A_1, ctx->args[0],             nullptr, type);
	C (OP_LEA_A,     attr_params,              nullptr, ctx->args[1]);
	C (OP_CALL_B,    ctx->funcs[tf_property], nullptr, res);
	return res;
}

static def_t *
compute_function (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],            nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_function], nullptr, res);
	return res;
}

static def_t *
compute_field (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],         nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_field], nullptr, res);
	return res;
}

static def_t *
compute_pointer (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],           nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_pointer], nullptr, res);
	return res;
}

static def_t *
compute_base (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_base], nullptr, res);
	return res;
}

static def_t *
compute_int (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],       nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_int], nullptr, res);
	return res;
}

static def_t *
compute_uint (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_uint], nullptr, res);
	return res;
}

static def_t *
compute_bool (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_bool], nullptr, res);
	return res;
}

static def_t *
compute_float (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_float], nullptr, res);
	return res;
}

static def_t *
compute_type_array (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto count = compute_val (args[1], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],         nullptr, type);
	C (OP_STORE_A_1, ctx->args[1],         nullptr, count);
	C (OP_CALL_B,    ctx->funcs[tf_array], nullptr, res);
	return res;
}

static def_t *
compute_type_vector (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto width = compute_val (args[1], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],          nullptr, type);
	C (OP_STORE_A_1, ctx->args[1],          nullptr, width);
	C (OP_CALL_B,    ctx->funcs[tf_vector], nullptr, res);
	return res;
}

static def_t *
compute_matrix (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto cols = compute_val (args[1], ctx);
	auto rows = compute_val (args[2], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],          nullptr, type);
	C (OP_STORE_A_1, ctx->args[1],          nullptr, cols);
	C (OP_STORE_A_1, ctx->args[2],          nullptr, rows);
	C (OP_CALL_B,    ctx->funcs[tf_matrix], nullptr, res);
	return res;
}

static def_t *
compute_width (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],         nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_width], nullptr, res);
	return res;
}

static def_t *
compute_rows (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_rows], nullptr, res);
	return res;
}

static def_t *
compute_cols (int arg_count, const expr_t **args, comp_ctx_t *ctx)
{
	auto type = compute_type (args[0], ctx);
	auto res = compute_tmp (ctx);
	C (OP_STORE_A_1, ctx->args[0],        nullptr, type);
	C (OP_CALL_B,    ctx->funcs[tf_cols], nullptr, res);
	return res;
}

static type_func_t type_funcs[] = {
	[QC_ATTRIBUTE] = {
		.name = ".property",
		.check_params = single_type_property,
		.resolve = resolve_property,
		.evaluate = evaluate_property,
		.compute = compute_property,
	},
	[QC_AT_FUNCTION] = {
		.name = "@function",
		.check_params = single_type,
		.resolve = resolve_function,
		.compute = compute_function,
	},
	[QC_AT_FIELD] = {
		.name = "@field",
		.check_params = single_type,
		.resolve = resolve_field,
		.compute = compute_field,
	},
	[QC_AT_POINTER] = {
		.name = "@pointer",
		.check_params = single_type,
		.resolve = resolve_pointer,
		.compute = compute_pointer,
	},
	[QC_AT_ARRAY] = {
		.name = "@array",
		.check_params = single_type_opt_int,
		.resolve = resolve_array,
		.compute = compute_type_array,
	},
	[QC_AT_BASE] = {
		.name = "@base",
		.check_params = single_type,
		.resolve = resolve_base,
		.compute = compute_base,
	},
	[QC_AT_WIDTH] = {
		.name = "@width",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
		.compute = compute_width,
	},
	[QC_AT_VECTOR] = {
		.name = "@vector",
		.check_params = single_type_opt_int,
		.resolve = resolve_vector,
		.expand = expand_vector,
		.compute = compute_type_vector,
	},
	[QC_AT_ROWS] = {
		.name = "@rows",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
		.compute = compute_rows,
	},
	[QC_AT_COLS] = {
		.name = "@cols",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
		.compute = compute_cols,
	},
	[QC_AT_MATRIX] = {
		.name = "@matrix",
		.check_params = single_type_opt_int_pair,
		.resolve = resolve_matrix,
		.expand = expand_matrix,
		.compute = compute_matrix,
	},
	[QC_AT_INT] = {
		.name = "@int",
		.check_params = single_type,
		.resolve = resolve_int,
		.compute = compute_int,
	},
	[QC_AT_UINT] = {
		.name = "@uint",
		.check_params = single_type,
		.resolve = resolve_uint,
		.compute = compute_uint,
	},
	[QC_AT_BOOL] = {
		.name = "@bool",
		.check_params = single_type,
		.resolve = resolve_bool,
		.compute = compute_bool,
	},
	[QC_AT_FLOAT] = {
		.name = "@float",
		.check_params = single_type,
		.resolve = resolve_float,
		.compute = compute_float,
	},
};

expr_t *
new_type_function (int op, const expr_t *params)
{
	auto te = new_expr ();
	te->type = ex_type;
	te->nodag = true;
	te->typ = (ex_type_t) {
		.op = op,
		.params = params,
	};
	return te;
}

const expr_t *
type_function (int op, const expr_t *params)
{
	int         arg_count = list_count (&params->list);
	const expr_t *args[arg_count];
	list_scatter (&params->list, args);
	if ((unsigned) op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (params, "invalid type op: %d", op);
	}
	const char *msg = type_funcs[op].check_params (arg_count, args);
	if (msg) {
		return error (params, "%s for %s", msg, type_funcs[op].name);
	}
	return new_type_function (op, params);
}

symbol_t *
type_parameter (symbol_t *sym, const expr_t *type)
{
	if (sym->table) {
		sym = new_symbol (sym->name);
	}
	sym->sy_type = sy_type_param;
	sym->expr = type;
	return sym;
}

const type_t *
resolve_type (const expr_t *te, rua_ctx_t *ctx)
{
	if (te->type == ex_symbol) {
		auto sym = te->symbol;
		if (sym->type == sy_name) {
			sym = symtab_lookup (current_symtab, sym->name);
			if (sym && sym->sy_type == sy_type_param) {
				te = sym->expr;
			}
		} else if (sym->sy_type == sy_type) {
			return sym->type;
		}
	}
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	if (!te->typ.op) {
		//if (!te->typ.type) {
		//	internal_error (te, "no type in reference");
		//}
		return te->typ.type;
	}
	unsigned    op = te->typ.op;
	if (op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[op].resolve (arg_count, args, ctx);
}

const expr_t *
process_type (const expr_t *te, rua_ctx_t *ctx)
{
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	if (!te->typ.op) {
		if (!te->typ.type) {
			internal_error (te, "no type in reference");
		}
		return new_type_expr (te->typ.type);
	}
	unsigned    op = te->typ.op;
	if (op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	if (type_funcs[op].evaluate) {
		return type_funcs[op].evaluate (arg_count, args, ctx);
	} else {
		internal_error (te, "invalid type op: %s", type_funcs[op].name);
	}
}

const type_t **
expand_type (const expr_t *te, rua_ctx_t *ctx)
{
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	if (!te->typ.op) {
		if (!te->typ.type) {
			internal_error (te, "no type in reference");
		}
		//FIXME is this correct?
		const type_t **types = malloc (sizeof(type_t *[2]));
		types[0] = te->typ.type;
		types[1] = nullptr;
		return types;
	}
	unsigned    op = te->typ.op;
	if (op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	if (!type_funcs[op].expand) {
		error (te, "cannot expand %s", type_funcs[op].name);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[op].expand (arg_count, args, ctx);
}

const expr_t *
eval_type (const expr_t *te, rua_ctx_t *ctx)
{
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	unsigned    op = te->typ.op;
	if (op == 0) {
		if (!te->typ.type) {
			internal_error (te, "type ref with no type");
		}
		return te;
	}
	if (op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[op].evaluate (arg_count, args, ctx);
}

static def_t *
compute_tmp (comp_ctx_t *ctx)
{
	return new_def (nullptr, &type_int, ctx->data, sc_static);
}

static def_t *
compute_sized_tmp (int size, comp_ctx_t *ctx)
{
	auto def = new_def (nullptr, nullptr, ctx->data, sc_static);
	def->offset = defspace_alloc_loc (ctx->data, size);
	return def;
}

static pr_string_t
compute_str (const char *str, comp_ctx_t *ctx)
{
	return strpool_addstr (ctx->strings, str);
}

static def_t *
compute_val (const expr_t *arg, comp_ctx_t *ctx)
{
	auto val = compute_tmp (ctx);
	if (arg->type == ex_expr || arg->type == ex_uexpr) {
		def_t *d1 = compute_val (arg->expr.e1, ctx);
		def_t *d2 = nullptr;
		statement_t s = {
			.opcode = convert_op (arg->expr.op),
			.opa = def_operand (d1, d1->type, arg->expr.e1),
			.opc = def_operand (val, val->type, arg),
		};
		if (arg->type == ex_uexpr) {
			d2 = compute_val (arg->expr.e2, ctx);
			s.opb = def_operand (d2, d2->type, arg->expr.e2);
		}
		auto inst = opcode_find (s.opcode, s.opa, s.opb, s.opc);
		if (!inst) {
			print_statement (&s);
			internal_error (arg, "no such instruction");
		}
		C (opcode_get (inst), d1, d2, val);
	} else if (is_integral_val (arg)) {
		D_INT (val) = expr_integral (arg);
	} else if (arg->type == ex_type) {
		int op = arg->typ.op;
		if (arg->typ.type) {
			auto type = arg->typ.type;
			if (op == QC_AT_WIDTH) {
				D_INT (val) = type_width (type);
			} else if (op == QC_AT_ROWS) {
				D_INT (val) = type_rows (type);
			} else if (op == QC_AT_COLS) {
				D_INT (val) = type_cols (type);
			} else {
				internal_error (arg, "invalid type op");
			}
		} else if (arg->typ.params) {
			auto params = arg->typ.params;
			if (params->type != ex_list || !params->list.head
				|| params->list.head->next) {
				internal_error (arg, "invalid type params");
			}
			auto type = compute_type (params->list.head->expr, ctx);
			if (op == QC_AT_WIDTH) {
				op = tf_width;
			} else if (op == QC_AT_ROWS) {
				op = tf_rows;
			} else if (op == QC_AT_COLS) {
				op = tf_cols;
			} else {
				internal_error (arg, "invalid type op");
			}
			C (OP_STORE_A_1, ctx->args[0],   nullptr, type);
			C (OP_CALL_B,    ctx->funcs[op], nullptr, val);
		} else {
			error (arg, "invalid type passed to %s", type_funcs[op].name);
		}
	}
	return val;
}

static def_t *
compute_type (const expr_t *arg, comp_ctx_t *ctx)
{
	if (arg->type == ex_symbol) {
		auto sym = arg->symbol;
		auto gen = compute_tmp (ctx);
		auto res = compute_tmp (ctx);
		int ind = -1;
		for (int i = 0; i < ctx->num_types; i++) {
			if (strcmp (ctx->types[i].name, sym->name) == 0) {
				ind = i;
				break;
			}
		}
		if (ind < 0) {
			error (arg, "%s cannot be resolved here", sym->name);
			Sys_longjmp (ctx->jmpbuf);
		}
		D_INT (gen) = ind;
		C (OP_STORE_A_1, ctx->args[0],           nullptr, gen);
		C (OP_CALL_B,    ctx->funcs[tf_gentype], nullptr, res);
		return res;
	}
	if (arg->type == ex_field) {
		// type property
		const expr_t *args[2] = {arg->field.object, arg->field.member};
		return compute_property (2, args, ctx);
	}
	if (arg->type != ex_type) {
		internal_error (arg, "not a type expression");
	}
	if (!arg->typ.op) {
		if (!arg->typ.type) {
			internal_error (arg, "no type in reference");
		}
		auto val = compute_tmp (ctx);
		D_INT (val) = arg->typ.type->id;
		return val;
	}
	unsigned    op = arg->typ.op;
	if (op >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[op].name) {
		internal_error (arg, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&arg->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&arg->typ.params->list, args);
	bool fail = false;
	for (int i = 0; i < arg_count; i++) {
		if (((args[i]->type == ex_expr || args[i]->type == ex_uexpr)
			 && !args[i]->expr.type)
			|| (args[i]->type == ex_field && !args[i]->field.type)) {
			args[i] = expr_process (args[i], ctx->rua_ctx);
			fail |= is_error (args[i]);
		}
	}
	if (fail) {
		Sys_longjmp (ctx->jmpbuf);
	}
	const char *msg = type_funcs[op].check_params (arg_count, args);
	if (msg) {
		error (arg->typ.params, "%s for %s", msg, type_funcs[op].name);
		Sys_longjmp (ctx->jmpbuf);
	}
	return type_funcs[op].compute (arg_count, args, ctx);
}

typeeval_t *
build_type_function (const expr_t *te, int num_types, gentype_t *types,
					 rua_ctx_t *rua_ctx)
{
	auto code = codespace_new ();
	auto data = defspace_new (ds_backed);
	auto strings = strpool_new ();
	comp_ctx_t *ctx = &(comp_ctx_t) {
		.types = types,
		.num_types = num_types,
		.code = code,
		.data = data,
		.strings = strings,
		.rua_ctx = rua_ctx,
	};
	compute_tmp (ctx);
	for (int i = 0; i < 3; i++) {
		ctx->args[i] = compute_tmp (ctx);
	}
	for (int i = tf_null + 1; i < tf_num_functions; i++) {
		ctx->funcs[i] = compute_tmp (ctx);
		D_FUNCTION (ctx->funcs[i]) = i;
	}
	C (OP_NOP, nullptr, nullptr, nullptr);
	def_t *res = nullptr;
	typeeval_t *func = malloc (sizeof (typeeval_t));
	if (!Sys_setjmp (ctx->jmpbuf)) {
		res = compute_type (te, ctx);
		C (OP_RETURN, res, nullptr, 0);
		*func = (typeeval_t) {
			.code = code->code,
			.data = data->data,
			.strings = strings->strings,
			.code_size = code->size,
			.data_size = data->size,
			.string_size = strings->size,
		};
	} else {
		*func = (typeeval_t) { };
	}
	code->code = nullptr;
	data->data = nullptr;
	strings->strings = nullptr;
	codespace_delete (code);
	defspace_delete (data);
	strpool_delete (strings);
	return func;
}
