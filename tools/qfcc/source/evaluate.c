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

//#include "QF/alloc.h"
//#include "QF/dstring.h"
//#include "QF/hash.h"
//#include "QF/mathlib.h"
#include "QF/progs.h"

#include "QF/simd/types.h"

#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

//FIXME this (to setup_value_progs) should be in its own file and more
//general (good for constant folding, too, and maybe some others).
static void
value_debug_handler (prdebug_t event, void *param, void *data)
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
};

#define BASE(b, base) (((base) & 3) << OP_##b##_SHIFT)
#define OP(a, b, c, op) ((op) | BASE(A, a) | BASE(B, b) | BASE(C, c))

static bfunction_t value_functions[] = {
	{},	// null function
	[vf_convert] = { .first_statement = vf_convert * 16 },
};

static __attribute__((aligned(64)))
dstatement_t value_statements[] = {
	[vf_convert * 16 - 1] = {},
	{ OP_CONV, 0, 07777, 16 },
	{ OP_RETURN, 16, 0, 0 },
};

#define num_globals 16384
#define stack_size 8192
static __attribute__((aligned(64)))
pr_type_t value_globals[num_globals + 128] = {
	[num_globals - stack_size] = { .uint_value = num_globals },
};

static dprograms_t value_progs = {
	.version = PROG_VERSION,
	.statements = {
		.count = sizeof (value_statements) / sizeof (value_statements[0]),
	},
};
static progs_t value_pr = {
	.progs = &value_progs,
	.debug_handler = value_debug_handler,
	.debug_data = &value_pr,
	.pr_trace = 1,
	.pr_trace_depth = -1,
	.function_table = value_functions,
	.pr_statements = value_statements,
	.globals_size = num_globals,
	.pr_globals = value_globals,
	.stack_bottom = num_globals - stack_size + 4,
	.pr_return_buffer = value_globals + num_globals,
	.pr_return = value_globals + num_globals,
	.globals = {
		.stack = (pr_ptr_t *) (value_globals + num_globals - stack_size),
	}
};

void
setup_value_progs (void)
{
	PR_Init (&value_pr);
	PR_Debug_Init (&value_pr);
}

ex_value_t *
convert_value (ex_value_t *value, type_t *type)
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
	memcpy (value_globals, &value->v,
			type_size (value->type) * sizeof (pr_type_t));
	value_pr.pr_trace = options.verbosity > 1;
	PR_ExecuteProgram (&value_pr, vf_convert);
	return new_type_value (type, value_pr.pr_return_buffer);
}
