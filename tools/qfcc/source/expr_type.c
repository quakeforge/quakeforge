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

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

typedef struct {
	const char *name;
	const char *(*check_params) (int arg_count, const expr_t **args);
	const type_t *(*resolve) (int arg_count, const expr_t **args);
	const type_t **(*expand) (int arg_count, const expr_t **args);
	const expr_t *(*evaluate) (int arg_count, const expr_t **args);
} type_func_t;

static type_func_t type_funcs[];

static bool
check_type (const expr_t *arg)
{
	if ((arg->type != ex_symbol || arg->symbol->sy_type != sy_type_param)
		&& arg->type != ex_type) {
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
evaluate_int (const expr_t *expr)
{
	if (expr->type == ex_expr || expr->type == ex_uexpr) {
		auto e = new_expr ();
		*e = *expr;
		e->expr.e1 = evaluate_int (expr->expr.e1);
		if (expr->type == ex_uexpr) {
			e->expr.e2 = evaluate_int (expr->expr.e2);
		}
		return fold_constants (e);
	}
	if (is_integral_val (expr)) {
		return expr;
	}
	int op = expr->typ.op;
	int ind = op - QC_GENERIC;
	auto type = expr->typ.type;
	if (!type) {
		return error (expr, "invalid type passed to %s", type_funcs[ind].name);
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
evaluate_int_op (int arg_count, const expr_t **args)
{
	return evaluate_int (args[0]);
}

static const type_t *
resolve_function (int arg_count, const expr_t **args)
{
	return &type_func;//FIXME
	auto type = resolve_type (args[0]);
	if (type) {
		type = field_type (type);
		type = find_type (type);
	}
	return type;
}

static const type_t *
resolve_field (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = field_type (type);
		type = find_type (type);
	}
	return type;
}

static const type_t *
resolve_pointer (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = pointer_type (type);
		type = find_type (type);
	}
	return type;
}

static const type_t *
resolve_array (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		int count = 0;
		if (arg_count > 1) {
			auto count_expr = evaluate_int (args[1]);
			if (is_error (count_expr)) {
				return nullptr;
			}
			count = expr_int (count_expr);
		}
		type = array_type (type, count);
	}
	return type;
}

static const type_t *
resolve_base (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = base_type (type);
	}
	return type;
}

static const type_t *
resolve_vector (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		int width = 0;
		if (arg_count > 1) {
			auto width_expr = evaluate_int (args[1]);
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
resolve_matrix (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		int rows = 0;
		int cols = 0;
		if (arg_count > 1) {
			auto rows_expr = evaluate_int (args[2]);
			auto cols_expr = evaluate_int (args[1]);
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
resolve_int (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = int_type (type);
	}
	return type;
}

static const type_t *
resolve_uint (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = uint_type (type);
	}
	return type;
}

static const type_t *
resolve_bool (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = bool_type (type);
	}
	return type;
}

static const type_t *
resolve_float (int arg_count, const expr_t **args)
{
	auto type = resolve_type (args[0]);
	if (type) {
		type = float_type (type);
	}
	return type;
}

static const type_t **
expand_vector (int arg_count, const expr_t **args)
{
	const type_t *base = resolve_type (args[0]);
	if (!is_bool (base) && !is_scalar (base)) {
		error (args[0], "invalid vector component type");
		return nullptr;
	}

	if (arg_count == 2) {
		auto comps_expr = evaluate_int (args[1]);
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
expand_matrix (int arg_count, const expr_t **args)
{
	const type_t *base = resolve_type (args[0]);
	if (!is_scalar (base)) {
		error (args[0], "invalid matrix component type");
		return nullptr;
	}

	// @matrix doesn't include vectors for generic parameters
	if (arg_count == 3) {
		auto rows_expr = evaluate_int (args[2]);
		auto cols_expr = evaluate_int (args[1]);
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

static type_func_t type_funcs[] = {
	[QC_AT_FUNCTION - QC_GENERIC] = {
		.name = "@function",
		.check_params = single_type,
		.resolve = resolve_function,
	},
	[QC_AT_FIELD - QC_GENERIC] = {
		.name = "@field",
		.check_params = single_type,
		.resolve = resolve_field,
	},
	[QC_AT_POINTER - QC_GENERIC] = {
		.name = "@pointer",
		.check_params = single_type,
		.resolve = resolve_pointer,
	},
	[QC_AT_ARRAY - QC_GENERIC] = {
		.name = "@array",
		.check_params = single_type_opt_int,
		.resolve = resolve_array,
	},
	[QC_AT_BASE - QC_GENERIC] = {
		.name = "@base",
		.check_params = single_type,
		.resolve = resolve_base,
	},
	[QC_AT_WIDTH - QC_GENERIC] = {
		.name = "@width",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
	},
	[QC_AT_VECTOR - QC_GENERIC] = {
		.name = "@vector",
		.check_params = single_type_opt_int,
		.resolve = resolve_vector,
		.expand = expand_vector,
	},
	[QC_AT_ROWS - QC_GENERIC] = {
		.name = "@rows",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
	},
	[QC_AT_COLS - QC_GENERIC] = {
		.name = "@cols",
		.check_params = single_type,
		.evaluate = evaluate_int_op,
	},
	[QC_AT_MATRIX - QC_GENERIC] = {
		.name = "@matrix",
		.check_params = single_type_opt_int_pair,
		.resolve = resolve_matrix,
		.expand = expand_matrix,
	},
	[QC_AT_INT - QC_GENERIC] = {
		.name = "@int",
		.check_params = single_type,
		.resolve = resolve_int,
	},
	[QC_AT_UINT - QC_GENERIC] = {
		.name = "@uint",
		.check_params = single_type,
		.resolve = resolve_uint,
	},
	[QC_AT_BOOL - QC_GENERIC] = {
		.name = "@bool",
		.check_params = single_type,
		.resolve = resolve_bool,
	},
	[QC_AT_FLOAT - QC_GENERIC] = {
		.name = "@float",
		.check_params = single_type,
		.resolve = resolve_float,
	},
};

const expr_t *
type_function (int op, const expr_t *params)
{
	int         arg_count = list_count (&params->list);
	const expr_t *args[arg_count];
	list_scatter (&params->list, args);
	unsigned    ind = op - QC_GENERIC;
	if (ind >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[ind].name) {
		internal_error (params, "invalid type op: %d", op);
	}
	const char *msg = type_funcs[ind].check_params (arg_count, args);
	if (msg) {
		return error (params, "%s for %s", msg, type_funcs[ind].name);
	}
	auto te = new_expr ();
	te->type = ex_type;
	te->nodag = 1;
	te->typ = (ex_type_t) {
		.op = op,
		.params = params,
	};
	return te;
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
resolve_type (const expr_t *te)
{
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	if (!te->typ.op) {
		if (!te->typ.type) {
			internal_error (te, "no type in reference");
		}
		return te->typ.type;
	}
	int         op = te->typ.op;
	unsigned    ind = op - QC_GENERIC;
	if (ind >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[ind].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[ind].resolve (arg_count, args);
}

const type_t **
expand_type (const expr_t *te)
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
	int         op = te->typ.op;
	unsigned    ind = op - QC_GENERIC;
	if (ind >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[ind].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	if (!type_funcs[ind].expand) {
		error (te, "cannot expand %s", type_funcs[ind].name);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[ind].expand (arg_count, args);
}

const expr_t *
evaluate_type (const expr_t *te)
{
	if (te->type != ex_type) {
		internal_error (te, "not a type expression");
	}
	int         op = te->typ.op;
	unsigned    ind = op - QC_GENERIC;
	if (ind >= sizeof (type_funcs) / sizeof (type_funcs[0])
		|| !type_funcs[ind].name) {
		internal_error (te, "invalid type op: %d", op);
	}
	int         arg_count = list_count (&te->typ.params->list);
	const expr_t *args[arg_count];
	list_scatter (&te->typ.params->list, args);
	return type_funcs[ind].evaluate (arg_count, args);
}
