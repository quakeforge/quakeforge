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
	.name = "int[4]",
	.size = 4 * sizeof (int),
	.binops = cexpr_array_binops,
	.unops = 0,
	.data = &int_array_4_data,
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
	{}
};
exprval_t test_result = { &cexpr_int, &c };
exprval_t plane_result = { &cexpr_vector, &plane };
// a bit hacky, but no l-values
exprval_t dist_result = { &cexpr_float, (float *)&plane + 3 };
exprval_t intercept_result = { &cexpr_vector, &intercept };

exprtab_t root_symtab = {
	.symbols = cexpr_lib_symbols,
};
exprtab_t symtab = {
	.symbols = symbols,
};

exprctx_t root_context = {
	.symtab = &root_symtab
};
exprctx_t context = {
	.parent = &root_context,
	.result = &test_result,
	.symtab = &symtab
};

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

	cexpr_init_symtab (&root_symtab, &context);
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
