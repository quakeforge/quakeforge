/*
	expr_vector.c

	vector expressions

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/04/27

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

#include <strings.h>
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

expr_t *
convert_vector (expr_t *e)
{
	float       val[4];

	if (e->type != ex_vector)
		return e;
	if (is_vector(e->e.vector.type)) {
		// guaranteed to have three elements
		expr_t     *x = e->e.vector.list;
		expr_t     *y = x->next;
		expr_t     *z = y->next;
		x = fold_constants (cast_expr (&type_float, x));
		y = fold_constants (cast_expr (&type_float, y));
		z = fold_constants (cast_expr (&type_float, z));
		if (is_constant (x) && is_constant (y) && is_constant (z)) {
			val[0] = expr_float(x);
			val[1] = expr_float(y);
			val[2] = expr_float(z);
			return new_vector_expr (val);
		}
		// at least one of x, y, z is not constant, so rebuild the
		// list incase any of them are new expressions
		z->next = 0;
		y->next = z;
		x->next = y;
		e->e.vector.list = x;
		return e;
	}
	if (is_quaternion(e->e.vector.type)) {
		// guaranteed to have two or four elements
		if (e->e.vector.list->next->next) {
			// four vals: x, y, z, w
			expr_t     *x = e->e.vector.list;
			expr_t     *y = x->next;
			expr_t     *z = y->next;
			expr_t     *w = z->next;
			x = fold_constants (cast_expr (&type_float, x));
			y = fold_constants (cast_expr (&type_float, y));
			z = fold_constants (cast_expr (&type_float, z));
			w = fold_constants (cast_expr (&type_float, w));
			if (is_constant (x) && is_constant (y) && is_constant (z)
				&& is_constant (w)) {
				val[0] = expr_float(x);
				val[1] = expr_float(y);
				val[2] = expr_float(z);
				val[3] = expr_float(w);
				return new_quaternion_expr (val);
			}
			// at least one of x, y, z, w is not constant, so rebuild the
			// list incase any of them are new expressions
			w->next = 0;
			z->next = w;
			y->next = z;
			x->next = y;
			e->e.vector.list = x;
			return e;
		} else {
			// v, s
			expr_t     *v = e->e.vector.list;
			expr_t     *s = v->next;

			v = convert_vector (v);
			s = fold_constants (cast_expr (&type_float, s));
			if (is_constant (v) && is_constant (s)) {
				memcpy (val, expr_vector (v), 3 * sizeof (float));
				val[3] = expr_float (s);
				return new_quaternion_expr (val);
			}
			// Either v or s is not constant, so can't convert to a quaternion
			// constant.
			// Rebuild the list in case v or s is a new expression
			// the list will always be v, s
			s->next = 0;
			v->next = s;
			e->e.vector.list = v;
			return e;
		}
	}
	internal_error (e, "bogus vector expression");
}

expr_t *
new_vector_list (expr_t *e)
{
	expr_t     *t;
	int         count;
	type_t     *type = &type_vector;
	expr_t     *vec;

	e = reverse_expr_list (e);		// put the elements in the right order
	for (t = e, count = 0; t; t = t->next)
		count++;
	switch (count) {
		case 4:
			type = &type_quaternion;
		case 3:
			// quaternion or vector. all expressions must be compatible with
			// a float (ie, a scalar)
			for (t = e; t; t = t->next) {
				if (t->type == ex_error) {
					return t;
				}
				if (!is_scalar (get_type (t))) {
					return error (t, "invalid type for vector element");
				}
			}
			vec = new_expr ();
			vec->type = ex_vector;
			vec->e.vector.type = type;
			vec->e.vector.list = e;
			break;
		case 2:
			if (e->type == ex_error || e->next->type == ex_error) {
				return e;
			}
			if (is_scalar (get_type (e)) && is_scalar (get_type (e->next))) {
				// scalar, scalar
				// expand [x, y] to [x, y, 0]
				e->next->next = new_float_expr (0);
				vec = new_expr ();
				vec->type = ex_vector;
				vec->e.vector.type = type;
				vec->e.vector.list = e;
				break;
			}
			// quaternion. either scalar, vector or vector, scalar
			if (is_scalar (get_type (e))
				&& is_vector (get_type (e->next))) {
				// scalar, vector
				// swap expressions
				t = e;
				e = e->next;
				e->next = t;
				t->next = 0;
			} else if (is_vector (get_type (e))
					   && is_scalar (get_type (e->next))) {
				// vector, scalar
				// do nothing
			} else {
				return error (t, "invalid types for vector elements");
			}
			// v, s
			vec = new_expr ();
			vec->type = ex_vector;
			vec->e.vector.type = &type_quaternion;
			vec->e.vector.list = e;
			break;
		default:
			return error (e, "invalid number of elements in vector exprssion");
	}
	return vec;
}
