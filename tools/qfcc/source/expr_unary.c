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

const expr_t *
unary_expr (int op, const expr_t *e)
{
	vec3_t      v;
	quat_t      q;
	const char *s;
	const type_t *t;

	e = convert_name (e);
	if (e->type == ex_error)
		return e;
	switch (op) {
		case '-':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary -");
			if (is_algebra (get_type (e))) {
				return algebra_negate (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, "type check failed!");
					case ev_double:
						return new_double_expr (-expr_double (e), e->implicit);
					case ev_float:
						return new_float_expr (-expr_float (e), e->implicit);
					case ev_vector:
						VectorNegate (expr_vector (e), v);
						return new_vector_expr (v);
					case ev_quaternion:
						QuatNegate (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (-expr_int (e), false);
					case ev_uint:
						return new_uint_expr (-expr_uint (e));
					case ev_short:
						return new_short_expr (-expr_short (e));
					case ev_invalid:
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->expr.op == '-') {
						return e->expr.e1;
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_block:
					if (!e->block.result) {
						return error (e, "invalid type for unary -");
					}
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_branch:
				case ex_inout:
					return error (e, "invalid type for unary -");
				case ex_expr:
				case ex_bool:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = get_type (e);
						return edag_add_expr (n);
					}
				case ex_def:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = e->def->type;
						return edag_add_expr (n);
					}
				case ex_symbol:
					{
						expr_t     *n = new_unary_expr (op, e);

						n->expr.type = e->symbol->type;
						return edag_add_expr (n);
					}
				case ex_multivec:
					return algebra_negate (e);
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary -");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '!':
			if (is_algebra (get_type (e))) {
				return algebra_dual (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
						internal_error (e, 0);
					case ev_string:
						s = expr_string (e);
						return new_int_expr (!s || !s[0], false);
					case ev_double:
						return new_int_expr (!expr_double (e), false);
					case ev_float:
						return new_int_expr (!expr_float (e), false);
					case ev_vector:
						return new_int_expr (!VectorIsZero (expr_vector (e)),
											 false);
					case ev_quaternion:
						return new_int_expr (!QuatIsZero (expr_quaternion (e)),
											 false);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (!expr_int (e), e->implicit);
					case ev_uint:
						return new_uint_expr (!expr_uint (e));
					case ev_short:
						return new_short_expr (!expr_short (e));
					case ev_invalid:
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_bool:
					return new_bool_expr (e->boolean.false_list,
										  e->boolean.true_list, e);
				case ex_block:
					if (!e->block.result)
						return error (e, "invalid type for unary !");
				case ex_uexpr:
				case ex_expr:
				case ex_def:
				case ex_symbol:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_address:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
					if (options.code.progsversion == PROG_VERSION) {
						return binary_expr (QC_EQ, e, new_nil_expr ());
					} else {
						expr_t     *n = new_unary_expr (op, e);

						if (options.code.progsversion > PROG_ID_VERSION)
							n->expr.type = &type_int;
						else
							n->expr.type = &type_float;
						return n;
					}
				case ex_multivec:
					return algebra_dual (e);
				case ex_branch:
				case ex_inout:
				case ex_nil:
					return error (e, "invalid type for unary !");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '~':
			if (is_algebra (get_type (e))) {
				return algebra_reverse (e);
			}
			if (is_constant (e)) {
				switch (extract_type (e)) {
					case ev_string:
					case ev_entity:
					case ev_field:
					case ev_func:
					case ev_ptr:
					case ev_vector:
					case ev_double:
						return error (e, "invalid type for unary ~");
					case ev_float:
						return new_float_expr (~(int) expr_float (e),
											   e->implicit);
					case ev_quaternion:
						QuatConj (expr_vector (e), q);
						return new_vector_expr (q);
					case ev_long:
					case ev_ulong:
					case ev_ushort:
						internal_error (e, "long not implemented");
					case ev_int:
						return new_int_expr (~expr_int (e), e->implicit);
					case ev_uint:
						return new_uint_expr (~expr_uint (e));
					case ev_short:
						return new_short_expr (~expr_short (e));
					case ev_invalid:
						t = get_type (e);
						if (t->meta == ty_enum) {
							return new_int_expr (~expr_int (e), false);
						}
						break;
					case ev_type_count:
					case ev_void:
						break;
				}
				internal_error (e, "weird expression type");
			}
			switch (e->type) {
				case ex_value:	// should be handled above
				case ex_error:
				case ex_label:
				case ex_labelref:
				case ex_state:
				case ex_compound:
				case ex_memset:
				case ex_selector:
				case ex_return:
				case ex_adjstk:
				case ex_with:
				case ex_args:
				case ex_list:
				case ex_type:
					internal_error (e, "unexpected expression type");
				case ex_uexpr:
					if (e->expr.op == '~')
						return e->expr.e1;
					goto bitnot_expr;
				case ex_block:
					if (!e->block.result)
						return error (e, "invalid type for unary ~");
					goto bitnot_expr;
				case ex_branch:
				case ex_inout:
					return error (e, "invalid type for unary ~");
				case ex_expr:
				case ex_bool:
				case ex_def:
				case ex_symbol:
				case ex_temp:
				case ex_vector:
				case ex_alias:
				case ex_assign:
				case ex_horizontal:
				case ex_swizzle:
				case ex_extend:
bitnot_expr:
					if (options.code.progsversion == PROG_ID_VERSION) {
						const expr_t *n1 = new_int_expr (-1, false);
						return binary_expr ('-', n1, e);
					} else {
						expr_t     *n = new_unary_expr (op, e);
						auto t = get_type (e);

						if (!is_integral(t) && !is_float(t)
							&& !is_quaternion(t))
							return error (e, "invalid type for unary ~");
						n->expr.type = t;
						return n;
					}
				case ex_multivec:
					return algebra_reverse (e);
				case ex_nil:
				case ex_address:
					return error (e, "invalid type for unary ~");
				case ex_count:
					internal_error (e, "invalid expression");
			}
			break;
		case '.':
			{
				if (extract_type (e) != ev_ptr)
					return error (e, "invalid type for unary .");
				scoped_src_loc (e);
				auto new = new_unary_expr ('.', e);
				new->expr.type = get_type (e)->fldptr.type;
				return new;
			}
		case '+':
			if (!is_math (get_type (e)))
				return error (e, "invalid type for unary +");
			return e;
		case QC_REVERSE:
			return algebra_reverse (e);
		case QC_DUAL:
			return algebra_dual (e);
		case QC_UNDUAL:
			return algebra_undual (e);
	}
	internal_error (e, 0);
}
