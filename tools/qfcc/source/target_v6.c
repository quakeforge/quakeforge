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
		initialize_def (args, nullptr, parameters->space, sc_param, locals,
						nullptr);
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
		initialize_def (param, nullptr, parameters->space, sc_param, locals,
						nullptr);
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
			initialize_def (param, nullptr, parameters->space, sc_param,
							locals, nullptr);
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
v6p_build_code (function_t *func, const expr_t *statements)
{
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

	// stitch parameter and locals data together with parameters coming
	// first
	defspace_t *space = defspace_new (ds_virtual);

	func->params_start = 0;

	merge_spaces (space, func->parameters->space, 1);
	func->parameters->space = space;

	merge_spaces (space, func->locals->space, 1);
	func->locals->space = space;
}

static void
vararg_int (const expr_t *e)
{
	if (is_int_val (e) && options.warnings.vararg_integer) {
		warning (e, "passing int constant into ... function");
	}
}

static const expr_t *
v6p_assign_vector (const expr_t *dst, const expr_t *src)
{
	expr_t     *block = new_block_expr (0);
	const expr_t *dx, *sx;
	const expr_t *dy, *sy;
	const expr_t *dz, *sz;
	const expr_t *dw, *sw;
	const expr_t *ds, *ss;
	const expr_t *dv, *sv;
	int         count = list_count (&src->vector.list);
	const expr_t *components[count];
	list_scatter (&src->vector.list, components);

	if (is_vector (src->vector.type)) {
		// guaranteed to have three elements
		sx = components[0];
		sy = components[1];
		sz = components[2];
		dx = field_expr (dst, new_name_expr ("x"));
		dy = field_expr (dst, new_name_expr ("y"));
		dz = field_expr (dst, new_name_expr ("z"));
		append_expr (block, assign_expr (dx, sx));
		append_expr (block, assign_expr (dy, sy));
		append_expr (block, assign_expr (dz, sz));
	} else {
		// guaranteed to have two or four elements
		if (count == 4) {
			// four vals: x, y, z, w
			sx = components[0];
			sy = components[1];
			sz = components[2];
			sw = components[3];
			dx = field_expr (dst, new_name_expr ("x"));
			dy = field_expr (dst, new_name_expr ("y"));
			dz = field_expr (dst, new_name_expr ("z"));
			dw = field_expr (dst, new_name_expr ("w"));
			append_expr (block, assign_expr (dx, sx));
			append_expr (block, assign_expr (dy, sy));
			append_expr (block, assign_expr (dz, sz));
			append_expr (block, assign_expr (dw, sw));
		} else {
			// v, s
			sv = components[0];
			ss = components[1];
			dv = field_expr (dst, new_name_expr ("v"));
			ds = field_expr (dst, new_name_expr ("s"));
			append_expr (block, assign_expr (dv, sv));
			append_expr (block, assign_expr (ds, ss));
		}
	}
	return block;
}

static const expr_t *
do_vector_compare (int op, const expr_t *e1, const expr_t *e2,
				   const type_t *res_type)
{
	// both e1 and e2 should have the same types here
	auto type = get_type (e1);
	if (op != QC_EQ && op != QC_NE) {
		return error (e2, "invalid comparison for %s", type->name);
	}
	auto e = new_binary_expr (op, e1, e2);
	e->expr.type = res_type;
	return e;
}

static const expr_t *
v6_vector_compare (int op, const expr_t *e1, const expr_t *e2)
{
	return do_vector_compare (op, e1, e2, &type_float);
}

static const expr_t *
v6p_vector_compare (int op, const expr_t *e1, const expr_t *e2)
{
	return do_vector_compare (op, e1, e2, &type_int);
}

target_t v6_target = {
	.value_too_large = v6_value_too_large,
	.build_scope = v6p_build_scope,
	.build_code = v6p_build_code,
	.declare_sym = declare_def,
	.vararg_int = vararg_int,
	.initialized_temp = initialized_temp_expr,
	.assign_vector = v6p_assign_vector,
	.proc_switch = ruamoko_proc_switch,
	.proc_caselabel = ruamoko_proc_caselabel,
	.proc_address = ruamoko_proc_address,
	.vector_compare = v6_vector_compare,
};

target_t v6p_target = {
	.value_too_large = v6_value_too_large,
	.build_scope = v6p_build_scope,
	.build_code = v6p_build_code,
	.declare_sym = declare_def,
	.vararg_int = vararg_int,
	.initialized_temp = initialized_temp_expr,
	.assign_vector = v6p_assign_vector,
	.proc_switch = ruamoko_proc_switch,
	.proc_caselabel = ruamoko_proc_caselabel,
	.proc_address = ruamoko_proc_address,
	.vector_compare = v6p_vector_compare,
};
