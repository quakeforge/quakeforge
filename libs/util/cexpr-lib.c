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

#undef uint
#define uint unsigned
#undef cexpr_unsigned
#define cexpr_unsigned cexpr_uint

#define FNAME(name, rtype, ptype) name##_##rtype##_##ptype

#define FUNC1(name, rtype, ptype, func)									\
static void																\
FNAME(name, rtype, ptype) (const exprval_t **params, exprval_t *result,	\
						   exprctx_t *context)							\
{																		\
	ptype      *a = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a);														\
}

#define FUNC2(name, rtype, ptype, func)									\
static void																\
FNAME(name, rtype, ptype) (const exprval_t **params, exprval_t *result,	\
						   exprctx_t *context)							\
{																		\
	/* parameters are reversed! */										\
	ptype      *a = params[1]->value;									\
	ptype      *b = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a, *b);													\
}

#define FUNC3(name, rtype, ptype, func)									\
static void																\
FNAME(name, rtype, ptype) (const exprval_t **params, exprval_t *result,	\
						   exprctx_t *context)							\
{																		\
	/* parameters are reversed! */										\
	ptype      *a = params[2]->value;									\
	ptype      *b = params[1]->value;									\
	ptype      *c = params[0]->value;									\
	rtype      *r = result->value;										\
	*r = func (*a, *b, *c);												\
}

#undef vector
#define vector vec4f_t

FUNC2 (dot, vector, vector, dotf)

FUNC1 (sin, float, float, sinf)
FUNC1 (sin, double, double, sin)

FUNC1 (cos, float, float, cosf)
FUNC1 (cos, double, double, cos)

FUNC1 (tan, float, float, tanf)
FUNC1 (tan, double, double, tan)

FUNC1 (asin, float, float, asinf)
FUNC1 (asin, double, double, asin)

FUNC1 (acos, float, float, acosf)
FUNC1 (acos, double, double, acos)

FUNC1 (atan, float, float, atanf)
FUNC1 (atan, double, double, atan)

FUNC2 (atan2, float, float, atan2f)
FUNC2 (atan2, double, double, atan2)

#define CAST_TO(rtype)					\
FUNC1 (cast, rtype, int, (rtype))		\
FUNC1 (cast, rtype, uint, (rtype))		\
FUNC1 (cast, rtype, size_t, (rtype))	\
FUNC1 (cast, rtype, float, (rtype))		\
FUNC1 (cast, rtype, double, (rtype))

CAST_TO (int)
CAST_TO (uint)
CAST_TO (size_t)
CAST_TO (float)
CAST_TO (double)
#undef CAST_TO

static void
cast_plitem (const exprval_t **params, exprval_t *result, exprctx_t *context)
{
	// first argument is ignored because cexpr_cast_plitem is meant to used
	// in an operator list
	cexpr_cast_plitem (0, params[0], result, context);
}

FUNC2 (min, int, int, min)
FUNC2 (min, uint, uint, min)
FUNC2 (min, size_t, size_t, min)
FUNC2 (min, float, float, min)
FUNC2 (min, double, double, min)

FUNC2 (max, int, int, max)
FUNC2 (max, uint, uint, max)
FUNC2 (max, size_t, size_t, max)
FUNC2 (max, float, float, max)
FUNC2 (max, double, double, max)

FUNC3 (bound, int, int, bound)
FUNC3 (bound, uint, uint, bound)
FUNC3 (bound, size_t, size_t, bound)
FUNC3 (bound, float, float, bound)
FUNC3 (bound, double, double, bound)

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

static exprtype_t *size_t_params[] = {
	&cexpr_size_t,
	&cexpr_size_t,
	&cexpr_size_t,
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

static exprtype_t *plitem_params[] = {
	&cexpr_plitem,
	&cexpr_plitem,
	&cexpr_plitem,
};

#define FUNC(name, rtype, n, ptype) \
	{ &cexpr_##rtype, n, ptype##_params, FNAME(name, rtype, ptype) }

static exprfunc_t dot_func[] = {
	FUNC (dot, vector, 2, vector),
	{}
};

static exprfunc_t sin_func[] = {
	FUNC (sin, float, 1, float),
	FUNC (sin, double, 1, double),
	{}
};

static exprfunc_t cos_func[] = {
	FUNC (cos, float, 1, float),
	FUNC (cos, double, 1, double),
	{}
};

static exprfunc_t tan_func[] = {
	FUNC (tan, float, 1, float),
	FUNC (tan, double, 1, double),
	{}
};

static exprfunc_t asin_func[] = {
	FUNC (asin, float, 1, float),
	FUNC (asin, double, 1, double),
	{}
};

static exprfunc_t acos_func[] = {
	FUNC (acos, float, 1, float),
	FUNC (acos, double, 1, double),
	{}
};

static exprfunc_t atan_func[] = {
	FUNC (atan, float, 1, float),
	FUNC (atan, double, 1, double),
	{}
};

static exprfunc_t atan2_func[] = {
	FUNC (atan2, float, 2, float),
	FUNC (atan2, double, 2, double),
	{}
};

#define CAST_TO(rtype) \
static exprfunc_t rtype##_func[] = {	\
	FUNC (cast, rtype, 1, int),			\
	FUNC (cast, rtype, 1, uint),		\
	FUNC (cast, rtype, 1, size_t),		\
	FUNC (cast, rtype, 1, float),		\
	FUNC (cast, rtype, 1, double),		\
	{ &cexpr_##rtype, 1, plitem_params, cast_plitem }, \
	{}									\
};

CAST_TO (int)
CAST_TO (uint)
CAST_TO (size_t)
CAST_TO (float)
CAST_TO (double)
#undef CAST_TO

static exprfunc_t min_func[] = {
	FUNC (min, int, 2, int),
	FUNC (min, uint, 2, uint),
	FUNC (min, size_t, 2, size_t),
	FUNC (min, float, 2, float),
	FUNC (min, double, 2, double),
	{}
};

static exprfunc_t max_func[] = {
	FUNC (max, int, 2, int),
	FUNC (max, uint, 2, uint),
	FUNC (max, size_t, 2, size_t),
	FUNC (max, float, 2, float),
	FUNC (max, double, 2, double),
	{}
};

static exprfunc_t bound_func[] = {
	FUNC (bound, int, 3, int),
	FUNC (bound, uint, 3, uint),
	FUNC (bound, size_t, 3, size_t),
	FUNC (bound, float, 3, float),
	FUNC (bound, double, 3, double),
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
	{ "uint",   &cexpr_function, uint_func      },
	{ "size_t", &cexpr_function, size_t_func    },
	{ "float",  &cexpr_function, float_func     },
	{ "double", &cexpr_function, double_func    },
	{ "min",    &cexpr_function, min_func       },
	{ "max",    &cexpr_function, max_func       },
	{ "bound",  &cexpr_function, bound_func     },
	{}
};
