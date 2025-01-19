/*
	evaluate_type.c

	type evaluation

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
#include <stdlib.h>

#include "QF/progs.h"
#include "QF/sys.h"

#include "QF/simd/types.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/evaluate_type.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/statements.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#undef P_PACKED
#define P_PACKED(p,t,n) (*(t *) &(p)->pr_globals[1+n])

typedef struct typectx_s {
	const type_t **types;
	int         num_types;
	const expr_t *expr;
	rua_ctx_t  *rua_ctx;
	sys_jmpbuf  jmpbuf;
} typectx_t;

static const type_t *
fetch_type (unsigned id, typectx_t *ctx)
{
	const type_t *type = nullptr;
	if (id < type_registry.size) {
		type = type_registry.a[id];
	}
	if (!type) {
		internal_error (ctx->expr, "invalid type id");
	}
	return type;
}

static void
tf_property_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	if (!type->property) {
		error (ctx->expr, "type doesn't support attributes");
		Sys_longjmp (ctx->jmpbuf);
	}
	auto attr_params = &P_STRUCT (pr, pr_int_t, 1);
	auto name = PR_GetString (pr, attr_params[0]);
	int count = attr_params[1];
	attribute_t *property = nullptr;
	if (count) {
		const expr_t *param_exprs[count] = {};
		internal_error (ctx->expr, "not implemented");
		auto params = new_list_expr (nullptr);
		list_gather (&params->list, param_exprs, count);
		property = new_attribute (name, params);
	} else {
		property = new_attribute (name, nullptr);
	}
	auto e = type->property (type, property);
	if (is_error (e)) {
		Sys_longjmp (ctx->jmpbuf);
	}
	type = resolve_type (e, ctx->rua_ctx);
	R_UINT (pr) = type->id;
}

static void
tf_function_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	internal_error (ctx->expr, "not implemented");
}

static void
tf_field_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = pointer_type (type);
	type = find_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_pointer_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = pointer_type (type);
	type = find_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_array_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	unsigned count = P_UINT (pr, 1);
	auto type = fetch_type (id, ctx);
	type = array_type (type, count);
	type = find_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_base_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = base_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_width_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	R_UINT (pr) = type_width (type);
}

static void
tf_vector_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	unsigned width = P_UINT (pr, 1);
	auto base = fetch_type (id, ctx);
	auto type = vector_type (base, width);
	R_UINT (pr) = type->id;
}

static void
tf_rows_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	R_UINT (pr) = type_rows (type);
}

static void
tf_cols_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	R_UINT (pr) = type_cols (type);
}

static void
tf_matrix_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	unsigned cols = P_UINT (pr, 1);
	unsigned rows = P_UINT (pr, 2);
	auto base = fetch_type (id, ctx);
	auto type = matrix_type (base, cols, rows);
	R_UINT (pr) = type->id;
}

static void
tf_int_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = int_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_uint_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = uint_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_bool_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = bool_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_float_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	unsigned id = P_UINT (pr, 0);
	auto type = fetch_type (id, ctx);
	type = float_type (type);
	R_UINT (pr) = type->id;
}

static void
tf_gentype_func (progs_t *pr, void *data)
{
	auto ctx = *(typectx_t **) data;
	int ind = P_INT (pr, 0);
	if (ind < 0 || ind >= ctx->num_types) {
		internal_error (ctx->expr, "invalid type index");
	}
	auto type = ctx->types[ind];
	if (!type) {
		error (ctx->expr, "refernce to unresolved type");
		Sys_longjmp (ctx->jmpbuf);
	}
	R_UINT (pr) = type->id;
}

#define BASE(b, base) (((base) & 3) << OP_##b##_SHIFT)
#define OP(a, b, c, op) ((op) | BASE(A, a) | BASE(B, b) | BASE(C, c))

static typectx_t *type_genfunc;
#define TF_FUNC(tf) \
	[tf] = { .first_statement = -1, .func = tf##_func, .data = &type_genfunc }
static bfunction_t type_functions[] = {
	{},	// null function
	[tf_eval] = { .first_statement = 1 },
	TF_FUNC(tf_property),
	TF_FUNC(tf_function),
	TF_FUNC(tf_field),
	TF_FUNC(tf_pointer),
	TF_FUNC(tf_array),
	TF_FUNC(tf_base),
	TF_FUNC(tf_width),
	TF_FUNC(tf_vector),
	TF_FUNC(tf_rows),
	TF_FUNC(tf_cols),
	TF_FUNC(tf_matrix),
	TF_FUNC(tf_int),
	TF_FUNC(tf_uint),
	TF_FUNC(tf_bool),
	TF_FUNC(tf_float),
	TF_FUNC(tf_gentype),
};
#undef TF_FUNC

#define num_globals 16384
#define stack_size 8192
static __attribute__((aligned(64)))
pr_type_t type_globals[num_globals + 128] = {
	[num_globals - stack_size] = { .uint_value = num_globals },
};
static defspace_t type_defspace = {
	.type = ds_backed,
	.def_tail = &type_defspace.defs,
	.data = type_globals,
	.max_size = num_globals - stack_size,
	.grow = 0,
};

static dprograms_t type_progs = {
	.version = PROG_VERSION,
	//.statements.count filled in at runtime
};
static progs_t type_pr = {
	.progs = &type_progs,
	.debug_handler = evaluate_debug_handler,
	.debug_data = &type_pr,
	.pr_trace = 1,
	.pr_trace_depth = -1,
	.function_table = type_functions,
	//.pr_statements filled in at runtime
	.globals_size = num_globals,
	.pr_globals = type_globals,
	.stack_bottom = num_globals - stack_size + 4,
	.pr_return_buffer = type_globals + num_globals,
	.pr_return = type_globals + num_globals,
	.globals = {
		.stack = (pr_ptr_t *) (type_globals + num_globals - stack_size),
	}
};

void
setup_type_progs (void)
{
	PR_Init (&type_pr);
	PR_Debug_Init (&type_pr);
}

const type_t *
evaluate_type (const typeeval_t *typeeval, int num_types, const type_t **types,
			   const expr_t *expr, rua_ctx_t *rua_ctx)
{
	typectx_t   ctx = {
		.types = types,
		.num_types = num_types,
		.expr = expr,
		.rua_ctx = rua_ctx,
	};
	int         err;
	if ((err = Sys_setjmp (ctx.jmpbuf))) {
		return nullptr;
	}
	type_genfunc = &ctx;
	type_pr.pr_statements = typeeval->code;
	type_pr.pr_globals = typeeval->data;
	type_pr.globals_size = typeeval->data_size;
	type_pr.pr_strings = (char *) typeeval->strings;
	type_pr.pr_stringsize = typeeval->string_size;
	type_pr.pr_trace = options.verbosity > 1;
	PR_ExecuteProgram (&type_pr, tf_eval);
	unsigned id = R_UINT (&type_pr);
	return fetch_type (id, &ctx);
}
