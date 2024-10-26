/*
	target_v6.c

	V6 and V6+ progs backends.

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

#include "QF/va.h"
#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/flow.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

static void
v6p_build_scope (symbol_t *fsym)
{
	int         i;
	param_t    *p;
	symbol_t   *args = 0;
	symbol_t   *param;
	function_t *func = fsym->metafunc->func;
	symtab_t   *parameters = func->parameters;
	symtab_t   *locals = func->locals;

	if (func->type->func.num_params < 0) {
		args = new_symbol_type (".args", &type_va_list);
		initialize_def (args, 0, parameters->space, sc_param, locals);
	}

	for (p = fsym->params, i = 0; p; p = p->next) {
		if (!p->selector && !p->type && !p->name)
			continue;					// ellipsis marker
		if (!p->type)
			continue;					// non-param selector
		if (!p->name) {
			error (0, "parameter name omitted");
			p->name = save_string ("");
		}
		param = new_symbol_type (p->name, p->type);
		initialize_def (param, 0, parameters->space, sc_param, locals);
		if (p->qual == pq_out) {
			param->def->param = false;
			param->def->out_param = true;
		} else if (p->qual == pq_inout) {
			param->def->out_param = true;
		} else if (p->qual == pq_const) {
			param->def->readonly = true;
		}
		i++;
	}

	if (args) {
		while (i < PR_MAX_PARAMS) {
			param = new_symbol_type (va (0, ".par%d", i), &type_param);
			initialize_def (param, 0, parameters->space, sc_param, locals);
			i++;
		}
	}
}

static bool
v6_value_too_large (const type_t *val_type)
{
	return type_size (val_type) > type_size (&type_param);
}

static void
v6p_emit_function (function_t *f, const expr_t *e)
{
	f->code = pr.code->size;
	lineno_base = f->def->loc.line;
	f->sblock = make_statements (e);
	if (options.code.optimize) {
		flow_data_flow (f);
	} else {
		statements_count_temps (f->sblock);
	}
	emit_statements (f->sblock);
}

target_t v6_target = {
	.value_too_large = v6_value_too_large,
	.build_scope = v6p_build_scope,
	.emit_function = v6p_emit_function,
};

target_t v6p_target = {
	.value_too_large = v6_value_too_large,
	.build_scope = v6p_build_scope,
	.emit_function = v6p_emit_function,
};
