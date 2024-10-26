/*
	target_rua.c

	Ruamoko progs backend.

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
#include "tools/qfcc/include/defspace.h"
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

static bool
ruamoko_value_too_large (const type_t *val_type)
{
	return type_size (val_type) > MAX_DEF_SIZE;
}

static void
create_param (symtab_t *parameters, symbol_t *param)
{
	defspace_t *space = parameters->space;
	def_t      *def = new_def (param->name, 0, space, sc_param);
	int         size = type_size (param->type);
	int         alignment = param->type->alignment;
	if (alignment < 4) {
		alignment = 4;
	}
	def->offset = defspace_alloc_aligned_highwater (space, size, alignment);
	def->type = param->type;
	param->def = def;
	param->sy_type = sy_def;
	param->lvalue = !def->readonly;
	symtab_addsymbol (parameters, param);
	if (is_vector(param->type) && options.code.vector_components)
		init_vector_components (param, 0, parameters);
}

static void
ruamoko_build_scope (symbol_t *fsym)
{
	function_t *func = fsym->metafunc->func;

	for (param_t *p = fsym->params; p; p = p->next) {
		symbol_t   *param;
		if (!p->selector && !p->type && !p->name) {
			// ellipsis marker
			param = new_symbol_type (".args", &type_va_list);
		} else {
			if (!p->type) {
				continue;					// non-param selector
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
				error (0, "parameter name omitted");
				p->name = save_string ("");
			}
			param = new_symbol_type (p->name, p->type);
		}
		create_param (func->parameters, param);
		if (p->qual == pq_out) {
			param->def->param = false;
			param->def->out_param = true;
		} else if (p->qual == pq_inout) {
			param->def->out_param = true;
		} else if (p->qual == pq_const) {
			param->def->readonly = true;
		}
		param->def->reg = func->temp_reg;
	}
}

static void
ruamoko_emit_function (function_t *f, const expr_t *e)
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

target_t ruamoko_target = {
	.value_too_large = ruamoko_value_too_large,
	.build_scope = ruamoko_build_scope,
	.emit_function = ruamoko_emit_function,
};
