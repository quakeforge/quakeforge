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

// standardized base register to use for all locals (arguments, local defs,
// params)
#define LOCALS_REG 1
// keep the stack aligned to 8 words (32 bytes) so lvec etc can be used without
// having to do shenanigans with mixed-alignment stack frames
#define STACK_ALIGN 8

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
ruamoko_build_code (function_t *func, const expr_t *statements)
{
	/* Create a function entry block to set up the stack frame and add the
	 * actual function code to that block. This ensure that the adjstk and
	 * with statements always come first, regardless of what ideas the
	 * optimizer gets.
	 */
	expr_t     *e;
	expr_t     *entry = new_block_expr (0);
	entry->loc = func->def->loc;

	e = new_adjstk_expr (0, 0);
	e->loc = entry->loc;
	append_expr (entry, e);

	e = new_with_expr (2, LOCALS_REG, new_short_expr (0));
	e->loc = entry->loc;
	append_expr (entry, e);

	append_expr (entry, statements);
	statements = entry;

	/* Mark all local defs as using the base register used for stack
	 * references.
	 */
	func->temp_reg = LOCALS_REG;
	for (def_t *def = func->locals->space->defs; def; def = def->next) {
		if (def->local || def->param) {
			def->reg = LOCALS_REG;
		}
	}
	for (def_t *def = func->parameters->space->defs; def; def = def->next) {
		if (def->local || def->param) {
			def->reg = LOCALS_REG;
		}
	}

	func->code = pr.code->size;
	lineno_base = func->def->loc.line;
	func->sblock = make_statements (statements);
	if (options.code.optimize) {
		flow_data_flow (func);
	} else {
		statements_count_temps (func->sblock);
	}
	emit_statements (func->sblock);

	defspace_sort_defs (func->parameters->space);
	defspace_sort_defs (func->locals->space);

	defspace_t *space = defspace_new (ds_virtual);

	if (func->arguments) {
		func->arguments->size = func->arguments->max_size;
		merge_spaces (space, func->arguments, STACK_ALIGN);
		func->arguments = 0;
	}

	merge_spaces (space, func->locals->space, STACK_ALIGN);
	func->locals->space = space;

	// allocate 0 words to force alignment and get the address
	func->params_start = defspace_alloc_aligned_highwater (space, 0,
														   STACK_ALIGN);

	dstatement_t *st = &pr.code->code[func->code];
	if (pr.code->size > func->code && st->op == OP_ADJSTK) {
		if (func->params_start) {
			st->b = -func->params_start;
		} else {
			// skip over adjstk so a zero adjustment doesn't get executed
			func->code += 1;
		}
	}
	merge_spaces (space, func->parameters->space, STACK_ALIGN);
	func->parameters->space = space;

	// force the alignment again so the full stack slot is counted when
	// the final parameter is smaller than STACK_ALIGN words
	defspace_alloc_aligned_highwater (space, 0, STACK_ALIGN);
}

target_t ruamoko_target = {
	.value_too_large = ruamoko_value_too_large,
	.build_scope = ruamoko_build_scope,
	.build_code = ruamoko_build_code,
	.declare_sym = declare_def,
};
