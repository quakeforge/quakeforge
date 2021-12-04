/*
	cexpr-lib.c

	Config expression parser. Or concurrent.

	Copyright (C) 2021  Bill Currie <bill@taniwha.org>

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
#include <stddef.h>
#include <math.h>

#include "QF/cexpr.h"
#include "QF/mathlib.h"
#include "QF/simd/vec4f.h"

#include "libs/util/cexpr-parse.h"

#define FUNC1(name, rtype, ptype, func)									\
static void																\
name (const exprval_t **params, exprval_t *result, exprctx_t *context)	\
{																		\
	ptype      *a = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a);														\
}

#define FUNC2(name, rtype, ptype, func)									\
static void																\
name (const exprval_t **params, exprval_t *result, exprctx_t *context)	\
{																		\
	/* parameters are reversed! */										\
	ptype      *a = params[1]->value;									\
	ptype      *b = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a, *b);													\
}

#define FUNC3(name, rtype, ptype, func)									\
static void																\
name (const exprval_t **params, exprval_t *result, exprctx_t *context)	\
{																		\
	/* parameters are reversed! */										\
	ptype      *a = params[2]->value;									\
	ptype      *b = params[1]->value;									\
	ptype      *c = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a, *b, *c);												\
}

FUNC2 (vec4f_dot, vec4f_t, vec4f_t, dotf)

FUNC1 (float_sin, float, float, sinf)
FUNC1 (double_sin, double, double, sin)

FUNC1 (float_cos, float, float, cosf)
FUNC1 (double_cos, double, double, cos)

FUNC1 (float_tan, float, float, tanf)
FUNC1 (double_tan, double, double, tan)

FUNC1 (float_asin, float, float, asinf)
FUNC1 (double_asin, double, double, asin)

FUNC1 (float_acos, float, float, acosf)
FUNC1 (double_acos, double, double, acos)

FUNC1 (float_atan, float, float, atanf)
FUNC1 (double_atan, double, double, atan)

FUNC2 (float_atan2, float, float, atan2f)
FUNC2 (double_atan2, float, float, atan2)

FUNC1 (int_int_cast, int, int, (int))
FUNC1 (int_float_cast, int, float, (int))
FUNC1 (int_double_cast, int, double, (int))

FUNC1 (float_int_cast, float, int, (float))
FUNC1 (float_float_cast, float, float, (float))
FUNC1 (float_double_cast, float, double, (float))

FUNC1 (double_int_cast, double, int, (double))
FUNC1 (double_float_cast, double, float, (double))
FUNC1 (double_double_cast, double, double, (double))

FUNC2 (int_min, int, int, min)
FUNC2 (uint_min, unsigned, unsigned, min)
FUNC2 (float_min, float, float, min)
FUNC2 (double_min, double, double, min)

FUNC2 (int_max, int, int, max)
FUNC2 (uint_max, unsigned, unsigned, max)
FUNC2 (float_max, float, float, max)
FUNC2 (double_max, double, double, max)

FUNC3 (int_bound, int, int, bound)
FUNC3 (uint_bound, unsigned, unsigned, bound)
FUNC3 (float_bound, float, float, bound)
FUNC3 (double_bound, double, double, bound)

static exprtype_t *vector_params[] = {
	&cexpr_vector,
	&cexpr_vector,
};

static exprtype_t *int_params[] = {
	&cexpr_int,
	&cexpr_int,
	&cexpr_int,
};

static exprtype_t *uint_params[] = {
	&cexpr_uint,
	&cexpr_uint,
	&cexpr_uint,
};

static exprtype_t *float_params[] = {
	&cexpr_float,
	&cexpr_float,
	&cexpr_float,
};

static exprtype_t *double_params[] = {
	&cexpr_double,
	&cexpr_double,
	&cexpr_double,
};

static exprfunc_t dot_func[] = {
	{ &cexpr_vector, 2, vector_params, vec4f_dot},
	{}
};

static exprfunc_t sin_func[] = {
	{ &cexpr_float,  1, float_params,  float_sin },
	{ &cexpr_double, 1, double_params, double_sin },
	{}
};

static exprfunc_t cos_func[] = {
	{ &cexpr_float,  1, float_params,  float_cos },
	{ &cexpr_double, 1, double_params, double_cos },
	{}
};

static exprfunc_t tan_func[] = {
	{ &cexpr_float,  1, float_params,  float_tan },
	{ &cexpr_double, 1, double_params, double_tan },
	{}
};

static exprfunc_t asin_func[] = {
	{ &cexpr_float,  1, float_params,  float_asin },
	{ &cexpr_double, 1, double_params, double_asin },
	{}
};

static exprfunc_t acos_func[] = {
	{ &cexpr_float,  1, float_params,  float_acos },
	{ &cexpr_double, 1, double_params, double_acos },
	{}
};

static exprfunc_t atan_func[] = {
	{ &cexpr_float,  1, float_params,  float_atan },
	{ &cexpr_double, 1, double_params, double_atan },
	{}
};

static exprfunc_t atan2_func[] = {
	{ &cexpr_float,  2, float_params,  float_atan2 },
	{ &cexpr_double, 2, double_params, double_atan2 },
	{}
};

static exprfunc_t int_func[] = {
	{ &cexpr_int, 1, int_params,    int_int_cast },
	{ &cexpr_int, 1, float_params,  int_float_cast },
	{ &cexpr_int, 1, double_params, int_double_cast },
	{}
};

static exprfunc_t float_func[] = {
	{ &cexpr_float, 1, int_params,    float_int_cast },
	{ &cexpr_float, 1, float_params,  float_float_cast },
	{ &cexpr_float, 1, double_params, float_double_cast },
	{}
};

static exprfunc_t double_func[] = {
	{ &cexpr_double, 1, int_params,    double_int_cast },
	{ &cexpr_double, 1, float_params,  double_float_cast },
	{ &cexpr_double, 1, double_params, double_double_cast },
	{}
};

static exprfunc_t min_func[] = {
	{ &cexpr_int,    2, int_params,    int_min },
	{ &cexpr_uint,   2, uint_params,   uint_min },
	{ &cexpr_float,  2, float_params,  float_min },
	{ &cexpr_double, 2, double_params, double_min },
	{}
};

static exprfunc_t max_func[] = {
	{ &cexpr_int,    2, int_params,    int_max },
	{ &cexpr_uint,   2, uint_params,   uint_max },
	{ &cexpr_float,  2, float_params,  float_max },
	{ &cexpr_double, 2, double_params, double_max },
	{}
};

static exprfunc_t bound_func[] = {
	{ &cexpr_int,    3, int_params,    int_bound },
	{ &cexpr_uint,   3, uint_params,   uint_bound },
	{ &cexpr_float,  3, float_params,  float_bound },
	{ &cexpr_double, 3, double_params, double_bound },
	{}
};

VISIBLE exprsym_t cexpr_lib_symbols[] = {
	{ "dot",    &cexpr_function, dot_func       },
	{ "sin",    &cexpr_function, sin_func       },
	{ "cos",    &cexpr_function, cos_func       },
	{ "tan",    &cexpr_function, tan_func       },
	{ "asin",   &cexpr_function, asin_func      },
	{ "acos",   &cexpr_function, acos_func      },
	{ "atan",   &cexpr_function, atan_func      },
	{ "atan2",  &cexpr_function, atan2_func     },
	{ "int",    &cexpr_function, int_func       },
	{ "float",  &cexpr_function, float_func     },
	{ "double", &cexpr_function, double_func    },
	{ "min",    &cexpr_function, min_func       },
	{ "max",    &cexpr_function, max_func       },
	{ "bound",  &cexpr_function, bound_func     },
	{}
};
