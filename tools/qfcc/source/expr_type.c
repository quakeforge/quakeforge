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
	const char *(*check_params) (int count, const expr_t **args);
} type_func_t;

static bool
check_type (const expr_t *arg)
{
	if ((arg->type != ex_symbol || arg->symbol->sy_type != sy_type_param)
		&& arg->type != ex_type) {
		return false;
	}
	return true;
}

static bool
check_int (const expr_t *arg)
{
	if ((arg->type != ex_symbol || arg->symbol->sy_type != sy_type_param)
		&& arg->type != ex_type) {
		return false;
	}
	return true;
}

static const char *
single_type (int count, const expr_t **args)
{
	if (count < 1) {
		return "too few arguments";
	}
	if (count > 1) {
		return "too many arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	return nullptr;
}

static const char *
single_type_opt_int (int count, const expr_t **args)
{
	if (count < 1) {
		return "too few arguments";
	}
	if (count > 2) {
		return "too many arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	if (count > 1 && !check_int (args[1])) {
		return "second parameter must be int";
	}
	return nullptr;
}

static const char *
single_type_opt_int_pair (int count, const expr_t **args)
{
	if (count < 1) {
		return "too few arguments";
	}
	if (count > 3) {
		return "too many arguments";
	}
	if (count == 2) {
		return "must have one or three arguments";
	}
	if (!check_type (args[0])) {
		return "first parameter must be a type";
	}
	if (count > 1 && (!check_int (args[1]) || !check_int (args[2]))) {
		return "second and third parameters must be int";
	}
	return nullptr;
}

static type_func_t type_funcs[] = {
	[QC_AT_FIELD - QC_GENERIC] = {
		.name = "@field",
		.check_params = single_type,
	},
	[QC_AT_POINTER - QC_GENERIC] = {
		.name = "@pointer",
		.check_params = single_type,
	},
	[QC_AT_ARRAY - QC_GENERIC] = {
		.name = "@array",
		.check_params = single_type_opt_int,
	},
	[QC_AT_BASE - QC_GENERIC] = {
		.name = "@base",
		.check_params = single_type,
	},
	[QC_AT_WIDTH - QC_GENERIC] = {
		.name = "@width",
		.check_params = single_type,
	},
	[QC_AT_VECTOR - QC_GENERIC] = {
		.name = "@vector",
		.check_params = single_type_opt_int,
	},
	[QC_AT_ROWS - QC_GENERIC] = {
		.name = "@rows",
		.check_params = single_type,
	},
	[QC_AT_COLS - QC_GENERIC] = {
		.name = "@cols",
		.check_params = single_type,
	},
	[QC_AT_MATRIX - QC_GENERIC] = {
		.name = "@matrix",
		.check_params = single_type_opt_int_pair,
	},
	[QC_AT_INT - QC_GENERIC] = {
		.name = "@int",
		.check_params = single_type,
	},
	[QC_AT_UINT - QC_GENERIC] = {
		.name = "@uint",
		.check_params = single_type,
	},
	[QC_AT_BOOL - QC_GENERIC] = {
		.name = "@bool",
		.check_params = single_type,
	},
	[QC_AT_FLOAT - QC_GENERIC] = {
		.name = "@float",
		.check_params = single_type,
	},
};

const expr_t *
type_function (int op, const expr_t *params)
{
	int         arg_count = list_count (&params->list);
	const expr_t *args[arg_count];
	list_scatter (&params->list, args);
	unsigned    ind = op - QC_GENERIC;
	if (ind >= sizeof (type_funcs) / sizeof (type_funcs[0])) {
		internal_error (params, "invalid type op: %d", op);
	}
	const char *msg = type_funcs[ind].check_params (arg_count, args);
	if (msg) {
		return error (params, "%s for %s", msg, type_funcs[ind].name);
	}
	auto te = new_expr ();
	te->type = ex_type;
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
	sym->s.expr = type;
	return sym;
}
