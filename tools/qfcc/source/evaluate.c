/*
	evaluate.c

	constant evaluation

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/11/16

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
#include <stdlib.h>

#include "QF/progs.h"

#include "QF/simd/types.h"

#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

//FIXME this (to setup_value_progs) should be in its own file and more
//general (good for constant folding, too, and maybe some others).
void
evaluate_debug_handler (prdebug_t event, void *param, void *data)
{
	progs_t      *pr = data;
	dstatement_t *st = 0;
	switch (event) {
		case prd_trace:
			st = pr->pr_statements + pr->pr_xstatement;
			PR_PrintStatement (pr, st, 0);
			break;
		case prd_breakpoint:
		case prd_subenter:
		case prd_subexit:
		case prd_runerror:
		case prd_watchpoint:
		case prd_begin:
		case prd_terminate:
		case prd_error:
		case prd_none:
			break;
	}
}

enum {
	vf_null,
	vf_convert,
	vf_foldconst,

	vf_num_functions
};

#define BASE(b, base) (((base) & 3) << OP_##b##_SHIFT)
#define OP(a, b, c, op) ((op) | BASE(A, a) | BASE(B, b) | BASE(C, c))

static bfunction_t value_functions[] = {
	{},	// null function
	[vf_convert]   = { .first_statement = vf_convert * 16 },
	[vf_foldconst] = { .first_statement = vf_foldconst * 16 },
};

static __attribute__((aligned(64)))
dstatement_t value_statements[] = {
	[vf_convert * 16 - 1] = {},
	{ OP_CONV, 0, 07777, 16 },
	{ OP_RETURN, 16, 0, 0 },

	[vf_foldconst * 16 - 1] = {},

	[vf_num_functions * 16 - 1] = {},
};
static codespace_t value_codespace = {
	.code = value_statements,
	.size = vf_foldconst * 16,
	.max_size = vf_num_functions * 16,
};

#define num_globals 16384
#define stack_size 8192
static __attribute__((aligned(64)))
pr_type_t evaluate_globals[num_globals + 128] = {
	[num_globals - stack_size] = { .uint_value = num_globals },
};
static defspace_t value_defspace = {
	.type = ds_backed,
	.def_tail = &value_defspace.defs,
	.data = evaluate_globals,
	.max_size = num_globals - stack_size,
	.grow = 0,
};

static dprograms_t value_progs = {
	.version = PROG_VERSION,
	.statements = {
		.count = sizeof (value_statements) / sizeof (value_statements[0]),
	},
};
static progs_t value_pr = {
	.progs = &value_progs,
	.debug_handler = evaluate_debug_handler,
	.debug_data = &value_pr,
	.pr_trace = 1,
	.pr_trace_depth = -1,
	.function_table = value_functions,
	.pr_statements = value_statements,
	.globals_size = num_globals,
	.pr_globals = evaluate_globals,
	.stack_bottom = num_globals - stack_size + 4,
	.pr_return_buffer = evaluate_globals + num_globals,
	.pr_return = evaluate_globals + num_globals,
	.globals = {
		.stack = (pr_ptr_t *) (evaluate_globals + num_globals - stack_size),
	}
};

void
setup_value_progs (void)
{
	PR_Init (&value_pr);
	PR_Debug_Init (&value_pr);
}

ex_value_t *
convert_value (ex_value_t *value, const type_t *type)
{
	if (!is_math (type) || !is_math (value->type)) {
		error (0, "unable to convert non-math value");
		return value;
	}
	if (type_width (type) != type_width (value->type)) {
		error (0, "unable to convert between values of different widths");
		return value;
	}
	int         from = type_cast_map[base_type (value->type)->type];
	int         to = type_cast_map[base_type (type)->type];
	int         width = type_width (value->type) - 1;
	int         conv = TYPE_CAST_CODE (from, to, width);
	int         addr = value_functions[vf_convert].first_statement;
	value_statements[addr + 0].b = conv;
	value_statements[addr + 1].c = type_size (type) - 1;
	memcpy (evaluate_globals, &value->raw_value,
			type_size (value->type) * sizeof (pr_type_t));
	value_pr.pr_trace = options.verbosity > 1;
	PR_ExecuteProgram (&value_pr, vf_convert);
	return new_type_value (type, value_pr.pr_return_buffer);
}

static def_t *get_def (operand_t *op);

static def_t *
get_temp_def (operand_t *op)
{
	tempop_t   *tempop = &op->tempop;
	if (tempop->def) {
		return tempop->def;
	}
	if (tempop->alias) {
		def_t      *tdef = get_def (tempop->alias);
		int         offset = tempop->offset;
		tempop->def = alias_def (tdef, op->type, offset);
	}
	if (!tempop->def) {
		tempop->def = new_def (0, op->type, &value_defspace, sc_static);
	}
	return tempop->def;
}

static def_t *
get_def (operand_t *op)
{
	if (is_short (op->type)) {
		auto def = new_def (0, &type_short, 0, sc_extern);
		def->offset = op->value->short_val;
		return def;
	}
	switch (op->op_type) {
		case op_def:
			return op->def;
		case op_value:
			return emit_value_core (op->value, 0, &value_defspace);
		case op_temp:
			return get_temp_def (op);
		case op_alias:
			return get_def (op->alias);
		case op_nil:
		case op_pseudo:
		case op_label:
			break;
	}
	internal_error (0, "unexpected operand");
}

const expr_t *
evaluate_constexpr (const expr_t *e)
{
	debug (e, "fold_constants");
	if (e->type == ex_uexpr) {
		if (!is_constant (e->expr.e1)) {
			return e;
		}
	} else if (e->type == ex_expr) {
		if (!is_constant (e->expr.e1) || !is_constant (e->expr.e2)) {
			return e;
		}
	} else if (e->type == ex_alias) {
		// offsets are always constant
		if (!is_constant (e->alias.expr)) {
			return e;
		}
	} else if (e->type == ex_extend) {
		if (!is_constant (e->extend.src)) {
			return e;
		}
	} else {
		return e;
	}

	defspace_reset (&value_defspace);
	auto saved_version = options.code.progsversion;
	options.code.progsversion = PROG_VERSION;
	sblock_t    sblock = {
		.tail = &sblock.statements,
	};
	ex_listitem_t return_expr = { .expr = new_return_expr (e) };
	ex_list_t return_st = { .head = &return_expr, .tail = &return_expr.next };
	auto sb = statement_slist (&sblock, &return_st);
	if (sblock.next != sb && sb->statements) {
		internal_error (e, "statement_slist did too much");
	}
	free_sblock (sb);
	sblock.next = 0;

	value_codespace.size = vf_foldconst * 16;
	for (auto s = sblock.statements; s; s = s->next) {
		auto opa = get_def (s->opa);
		auto opb = get_def (s->opb);
		auto opc = get_def (s->opc);
		auto inst = opcode_find (s->opcode, s->opa, s->opb, s->opc);
		if (!inst) {
			print_statement (s);
			internal_error (e, "no such instruction");
		}
		auto ds = codespace_newstatement (&value_codespace);
		*ds = (dstatement_t) {
			.op = opcode_get (inst),
			.a = opa->offset,
			.b = opb->offset,
			.c = opc->offset,
		};
	}
	options.code.progsversion = saved_version;
	value_pr.pr_trace = options.verbosity > 1;
	PR_ExecuteProgram (&value_pr, vf_foldconst);
	auto val = new_type_value (get_type (e), value_pr.pr_return_buffer);
	e = new_value_expr (val, false);
	return e;
}
