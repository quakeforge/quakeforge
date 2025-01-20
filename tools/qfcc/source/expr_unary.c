/*
	expr_unary.c

	expression construction and manipulations

	Copyright (C) 2001,2024 Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/evaluate.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

typedef struct {
	int         op;
	const type_t *result_type;
	const expr_t *(*process)(const expr_t *e);
	const expr_t *(*constant)(const expr_t *e);
	bool        no_vectorize;
} unary_type_t;

static const expr_t *
string_not (const expr_t *e)
{
	const char *s = expr_string (e);
	return new_int_expr (!s || !s[0], false);
}

static const expr_t *
float_negate (const expr_t *e)
{
	return new_float_expr (-expr_float (e), e->implicit);
}

static const expr_t *
float_not (const expr_t *e)
{
	return new_int_expr (!expr_float (e), false);
}

static const expr_t *
float_bitnot (const expr_t *e)
{
	return new_float_expr (~(int) expr_float (e), false);
}

static const expr_t *
vector_negate (const expr_t *e)
{
	vec3_t      v;

	VectorNegate (expr_vector (e), v);
	return new_vector_expr (v);
}

static const expr_t *
vector_not (const expr_t *e)
{
	return new_int_expr (!VectorIsZero (expr_vector (e)), false);
}

static const expr_t *
quat_negate (const expr_t *e)
{
	quat_t      q;

	QuatNegate (expr_quaternion (e), q);
	return new_vector_expr (q);
}

const expr_t *
pointer_deref (const expr_t *e)
{
	scoped_src_loc (e);
	auto new = new_unary_expr ('.', e);
	new->expr.type = get_type (e)->fldptr.type;
	return edag_add_expr (fold_constants (new));
}

static const expr_t *
quat_not (const expr_t *e)
{
	return new_bool_expr (!QuatIsZero (expr_quaternion (e)));
}

static const expr_t *
quat_conj (const expr_t *e)
{
	quat_t      q;

	QuatConj (expr_vector (e), q);
	return new_quaternion_expr (q);
}

static const expr_t *
int_negate (const expr_t *e)
{
	return new_int_expr (-expr_int (e), e->implicit);
}

static const expr_t *
int_not (const expr_t *e)
{
	return new_bool_expr (!expr_int (e));
}

static const expr_t *
int_bitnot (const expr_t *e)
{
	return new_int_expr (~expr_int (e), e->implicit);
}

static const expr_t *
uint_negate (const expr_t *e)
{
	return new_uint_expr (-expr_uint (e));
}

static const expr_t *
uint_not (const expr_t *e)
{
	return new_bool_expr (!expr_uint (e));
}

static const expr_t *
uint_bitnot (const expr_t *e)
{
	return new_uint_expr (~expr_uint (e));
}

static const expr_t *
short_negate (const expr_t *e)
{
	return new_short_expr (-expr_short (e));
}

static const expr_t *
short_not (const expr_t *e)
{
	return new_bool_expr (!expr_short (e));
}

static const expr_t *
short_bitnot (const expr_t *e)
{
	return new_short_expr (~expr_short (e));
}

static const expr_t *
ushort_negate (const expr_t *e)
{
	return new_ushort_expr (-expr_ushort (e));
}

static const expr_t *
ushort_not (const expr_t *e)
{
	return new_bool_expr (!expr_ushort (e));
}

static const expr_t *
ushort_bitnot (const expr_t *e)
{
	return new_ushort_expr (~expr_ushort (e));
}

static const expr_t *
double_negate (const expr_t *e)
{
	return new_double_expr (-expr_double (e), e->implicit);
}

static const expr_t *
double_not (const expr_t *e)
{
	return new_lbool_expr (!expr_double (e));
}

static const expr_t *
double_bitnot (const expr_t *e)
{
	return new_double_expr (~(pr_long_t) expr_double (e), false);
}

static const expr_t *
long_negate (const expr_t *e)
{
	return new_long_expr (-expr_long (e), e->implicit);
}

static const expr_t *
long_not (const expr_t *e)
{
	return new_lbool_expr (!expr_long (e));
}

static const expr_t *
long_bitnot (const expr_t *e)
{
	return new_long_expr (~expr_long (e), e->implicit);
}

static const expr_t *
ulong_negate (const expr_t *e)
{
	return new_ulong_expr (-expr_ulong (e));
}

static const expr_t *
ulong_not (const expr_t *e)
{
	return new_lbool_expr (!expr_ulong (e));
}

static const expr_t *
ulong_bitnot (const expr_t *e)
{
	return new_ulong_expr (~expr_ulong (e));
}

static const expr_t *
bool_not (const expr_t *e)
{
	return new_bool_expr (expr_int (e));
}

static const expr_t *
lbool_not (const expr_t *e)
{
	return new_lbool_expr (expr_long (e));
}

static unary_type_t string_u[] = {
	{ .op = '!', .result_type = &type_bool, .constant = string_not, },

	{}
};

static unary_type_t float_u[] = {
	{ .op = '-', .result_type = &type_float, .constant = float_negate, },
	{ .op = '!', .result_type = &type_bool,  .constant = float_not,    },
	{ .op = '~', .result_type = &type_float, .constant = float_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t vector_u[] = {
	{ .op = '-', .result_type = &type_vector, .constant = vector_negate,
		.no_vectorize = true },
	{ .op = '!', .result_type = &type_bool,   .constant = vector_not,
		.no_vectorize = true },

	{}
};

static unary_type_t entity_u[] = {
	{ .op = '!', .result_type = &type_bool, },

	{}
};

static unary_type_t field_u[] = {
	{ .op = '!', .result_type = &type_bool, },

	{}
};

static unary_type_t func_u[] = {
	{ .op = '!', .result_type = &type_bool, },

	{}
};

static unary_type_t pointer_u[] = {
	{ .op = '!', .result_type = &type_bool, },
	{ .op = '.', .process = pointer_deref, },

	{}
};

static unary_type_t quat_u[] = {
	{ .op = '-', .result_type = &type_quaternion, .constant = quat_negate,
		.no_vectorize = true },
	{ .op = '!', .result_type = &type_bool,       .constant = quat_not,
		.no_vectorize = true },
	{ .op = '~', .result_type = &type_quaternion, .constant = quat_conj,
		.no_vectorize = true },

	{}
};

static unary_type_t int_u[] = {
	{ .op = '-', .result_type = &type_int,  .constant = int_negate, },
	{ .op = '!', .result_type = &type_bool, .constant = int_not,    },
	{ .op = '~', .result_type = &type_int,  .constant = int_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t uint_u[] = {
	{ .op = '-', .result_type = &type_uint, .constant = uint_negate, },
	{ .op = '!', .result_type = &type_bool, .constant = uint_not, },
	{ .op = '~', .result_type = &type_uint, .constant = uint_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t short_u[] = {
	{ .op = '-', .result_type = &type_short, .constant = short_negate, },
	{ .op = '!', .result_type = &type_bool,  .constant = short_not,    },
	{ .op = '~', .result_type = &type_short, .constant = short_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t ushort_u[] = {
	{ .op = '-', .result_type = &type_ushort, .constant = ushort_negate, },
	{ .op = '!', .result_type = &type_bool,   .constant = ushort_not,    },
	{ .op = '~', .result_type = &type_ushort, .constant = ushort_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t double_u[] = {
	{ .op = '-', .result_type = &type_double, .constant = double_negate, },
	{ .op = '!', .result_type = &type_lbool,  .constant = double_not,    },
	{ .op = '~', .result_type = &type_double, .constant = double_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t long_u[] = {
	{ .op = '-', .result_type = &type_long,  .constant = ulong_negate, },
	{ .op = '!', .result_type = &type_lbool, .constant = ulong_not,    },
	{ .op = '~', .result_type = &type_long,  .constant = ulong_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t ulong_u[] = {
	{ .op = '-', .result_type = &type_ulong, .constant = long_negate, },
	{ .op = '!', .result_type = &type_lbool, .constant = long_not,    },
	{ .op = '~', .result_type = &type_ulong, .constant = long_bitnot, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t algebra_u[] = {
	{ .op = '-',        .process = algebra_negate,  },
	{ .op = '!',        .process = algebra_dual,    },
	{ .op = '~',        .process = algebra_reverse, },
	{ .op = QC_REVERSE, .process = algebra_reverse, },
	{ .op = QC_DUAL,    .process = algebra_dual,    },
	{ .op = QC_UNDUAL,  .process = algebra_undual,  },

	{}
};

static unary_type_t bool_u[] = {
	{ .op = '!', .result_type = &type_bool,       .constant = bool_not,    },

	{}
};

static unary_type_t lbool_u[] = {
	{ .op = '!', .result_type = &type_lbool,      .constant = lbool_not,    },

	{}
};

static unary_type_t *unary_expr_types[ev_type_count] = {
	[ev_string] = string_u,
	[ev_float] = float_u,
	[ev_vector] = vector_u,
	[ev_entity] = entity_u,
	[ev_field] = field_u,
	[ev_func] = func_u,
	[ev_ptr] = pointer_u,
	[ev_quaternion] = quat_u,
	[ev_int] = int_u,
	[ev_uint] = uint_u,
	[ev_short] = short_u,
	[ev_ushort] = ushort_u,
	[ev_double] = double_u,
	[ev_long] = long_u,
	[ev_ulong] = ulong_u,
};

const expr_t *
unary_expr (int op, const expr_t *e)
{
	if (e->type == ex_error) {
		return e;
	}

	if (is_reference (get_type (e))) {
		e = pointer_deref (e);
	}

	unary_type_t *unary_type = nullptr;
	auto t = get_type (e);

	if (op == '!' && e->type == ex_bool) {
		return new_boolean_expr (e->boolean.false_list,
								 e->boolean.true_list, e);
	}

	if (op == '+' && is_math (t)) {
		// unary + is a no-op
		return e;
	}
	if (is_algebra (t)) {
		unary_type = algebra_u;
	} else if (is_enum (t)) {
		unary_type = int_u;
	} else if (is_bool (t)) {
		unary_type = bool_u;
	} else if (is_lbool (t)) {
		unary_type = lbool_u;
	} else if (t->meta == ty_basic || is_handle (t)) {
		unary_type = unary_expr_types[t->type];
	}

	if (!unary_type) {
		return error (e, "invalid type for unary expression");
	}
	while (unary_type->op && unary_type->op != op) {
		unary_type++;
	}
	if (!unary_type->op) {
		return error (e, "invalid type for unary %s", get_op_string (op));
	}

	if (op == '-' && e->type == ex_expr && e->expr.anticommute) {
		auto neg = new_expr ();
		*neg = *e;
		neg->expr.e1 = e->expr.e2;
		neg->expr.e2 = e->expr.e1;
		return edag_add_expr (neg);
	}
	if (unary_type->process) {
		e = unary_type->process (e);
		e = fold_constants (e);
		return edag_add_expr (e);
	}
	if (is_constant (e) && !unary_type->constant) {
		internal_error (e, "unexpected expression type");
	}
	if (is_constant (e) && unary_type->constant) {
		return edag_add_expr (unary_type->constant (e));
	}
	auto result_type = t;
	if (unary_type->result_type) {
		result_type = unary_type->result_type;
		if (!unary_type->no_vectorize && !is_handle (t)) {
			result_type = matrix_type (result_type,
									   type_cols (t), type_rows (t));
		}
	}
	auto new = new_unary_expr (op, e);
	new->expr.type = result_type;
	return edag_add_expr (fold_constants (new));
}
