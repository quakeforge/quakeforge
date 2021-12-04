/*
	test-cexpr.c

	Config expression parser. Or concurrent.

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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

#include "QF/cexpr.h"
#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/va.h"
#include "QF/simd/vec4f.h"

int a = 5;
int b = 6;
int c;
int array[4] = { 9, 16, 25, 36 };
vec4f_t point = { 2, 3, 4, 1 };		// a point, so w = 1
vec4f_t normal = { 1, 2, 3, 0 };	// a vector, so w = 0
vec4f_t direction = { 4, 5, 6, 0 };	// a vector, so w = 0
vec4f_t plane;
vec4f_t intercept;

exprarray_t int_array_4_data = {
	&cexpr_int,
	sizeof (array) / sizeof (array[0]),
};
exprtype_t int_array_4 = {
	"int[4]",
	4 * sizeof (int),
	cexpr_array_binops,
	0,
	&int_array_4_data,
};

exprtype_t *vector_params[] = {
	&cexpr_vector,
	&cexpr_vector,
};

exprtype_t *int_params[] = {
	&cexpr_int,
	&cexpr_int,
};

exprtype_t *float_params[] = {
	&cexpr_float,
	&cexpr_float,
};

exprtype_t *double_params[] = {
	&cexpr_double,
	&cexpr_double,
};

static void
float_dot (const exprval_t **params, exprval_t *result, exprctx_t *context)
{
	// parameters are reversed!
	vec4f_t    *a = params[1]->value;
	vec4f_t    *b = params[0]->value;
	vec4f_t    *d = result->value;
	*d = dotf (*a, *b);
}

exprfunc_t dot_func[] = {
	{ &cexpr_vector, 2, vector_params, float_dot},
	{}
};

exprfunc_t sin_func[] = {
	{ &cexpr_float, 1, float_params },
	{ &cexpr_double, 1, double_params },
	{}
};

exprfunc_t cos_func[] = {
	{ &cexpr_float, 1, float_params },
	{ &cexpr_double, 1, double_params },
	{}
};

exprfunc_t atan2_func[] = {
	{ &cexpr_float, 2, float_params },
	{ &cexpr_double, 2, double_params },
	{}
};

exprfunc_t int_func[] = {
	{ &cexpr_int, 1, float_params },
	{ &cexpr_int, 1, double_params },
	{}
};

exprfunc_t float_func[] = {
	{ &cexpr_float, 1, int_params },
	{ &cexpr_float, 1, double_params },
	{}
};

exprfunc_t double_func[] = {
	{ &cexpr_double, 1, float_params },
	{ &cexpr_double, 1, double_params },
	{}
};

exprsym_t symbols[] = {
	{ "a", &cexpr_int, &a },
	{ "b", &cexpr_int, &b },
	{ "array", &int_array_4, &array },
	{ "point", &cexpr_vector, &point },
	{ "normal", &cexpr_vector, &normal },
	{ "plane", &cexpr_vector, &plane },
	{ "direction", &cexpr_vector, &direction },
	{ "intercept", &cexpr_vector, &intercept },
	{ "dot", &cexpr_function, dot_func },
	{ "sin", &cexpr_function, sin_func },
	{ "cos", &cexpr_function, cos_func },
	{ "atan2", &cexpr_function, atan2_func },
	{ "float", &cexpr_function, float_func },
	{ "double", &cexpr_function, double_func },
	{ "int", &cexpr_function, int_func },
	{}
};
exprval_t test_result = { &cexpr_int, &c };
exprval_t plane_result = { &cexpr_vector, &plane };
// a bit hacky, but no l-values
exprval_t dist_result = { &cexpr_float, (float *)&plane + 3 };
exprval_t intercept_result = { &cexpr_vector, &intercept };

exprtab_t symtab = {
	symbols,
	0
};

exprctx_t context = { &test_result, &symtab };

#define TEST_BINOP(op)													\
	do {																\
		c = -4096;														\
		context.result = &test_result;									\
		cexpr_eval_string ("a " #op " b", &context);					\
		printf ("c = a %s b -> %d = %d %s %d\n", #op, c, a, #op, b);	\
		if (c != (a op b)) {											\
			ret |= 1;													\
		}																\
	} while (0)

#define TEST_ARRAY(ind)													\
	do {																\
		c = -4096;														\
		context.result = &test_result;									\
		cexpr_eval_string (va (0, "array[%d]", ind), &context);			\
		printf ("c = array[%d] -> %d = %d\n", ind, c, array[ind]);		\
		if (c != array[ind]) {											\
			ret |= 1;													\
		}																\
	} while (0)

int
main(int argc, const char **argv)
{
	int         ret = 0;

	cexpr_init_symtab (&symtab, &context);
	context.memsuper = new_memsuper();

	TEST_BINOP (<<);
	TEST_BINOP (>>);
	TEST_BINOP (+);
	TEST_BINOP (-);
	TEST_BINOP (*);
	TEST_BINOP (/);
	TEST_BINOP (&);
	TEST_BINOP (|);
	TEST_BINOP (^);
	TEST_BINOP (%);

	TEST_ARRAY (0);
	TEST_ARRAY (1);
	TEST_ARRAY (2);
	TEST_ARRAY (3);

	context.result = &plane_result;
	cexpr_eval_string ("point.wzyx", &context);
	if (plane[0] != point[3] || plane[1] != point[2]
		|| plane[2] != point[1] || plane[3] != point[0]) {
		printf ("point.wzyx [%.9g, %.9g, %.9g] %.9g\n",
				VectorExpand (plane), plane[3]);
		ret |= 1;
	}
	cexpr_eval_string ("point.zyx", &context);
	if (plane[0] != point[2] || plane[1] != point[1]
		|| plane[2] != point[0] || plane[3] != 0) {
		printf ("point.zyx [%.9g, %.9g, %.9g] %.9g\n",
				VectorExpand (plane), plane[3]);
		ret |= 1;
	}
	cexpr_eval_string ("point.yx", &context);
	if (plane[0] != point[1] || plane[1] != point[0]
		|| plane[2] != 0 || plane[3] != 0) {
		printf ("point.yx [%.9g, %.9g, %.9g] %.9g\n",
				VectorExpand (plane), plane[3]);
		ret |= 1;
	}
	cexpr_eval_string ("normal", &context);
	context.result = &dist_result;
	cexpr_eval_string ("-dot(point, normal).x / 2f", &context);
	if (!VectorCompare (normal, plane)
		|| plane[3] != -DotProduct (normal, point) / 2) {
		printf ("plane [%.9g, %.9g, %.9g] %.9g\n",
				VectorExpand (plane), plane[3]);
		ret |= 1;
	}
	context.result = &intercept_result;
	cexpr_eval_string ("point - direction * dot(point, plane)/dot(direction,plane)", &context);
	if (intercept[0] != 0.75 || intercept[1] != 1.4375
		|| intercept[2] != 2.125 || intercept[3] != 1) {
		printf ("[%.9g, %.9g, %.9g] %.9g\n", VectorExpand (intercept), intercept[3]);
		ret |= 1;
	}

	Hash_DelTable (symtab.tab);
	delete_memsuper (context.memsuper);
	return ret;
}
