/*
	expr_names.h

	expression construction and manipulations

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/01/7

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

/**	\defgroup qfcc_expr Expressions
	\ingroup qfcc
*/
///@{

#ifndef EX_EXPR
#define EX_EXPR(expr)
#endif

EX_EXPR(error)		///< error expression. used to signal an error
EX_EXPR(state)		///< state expression (::ex_state_t)
EX_EXPR(bool)		///< short circuit boolean logic expression (::ex_bool_t)
EX_EXPR(label)		///< goto/branch label (::ex_label_t)
EX_EXPR(labelref)	///< label reference (::ex_labelref_t)
EX_EXPR(block)		///< statement block expression (::ex_block_t)
EX_EXPR(expr)		///< binary expression (::ex_expr_t)
EX_EXPR(uexpr)		///< unary expression (::ex_expr_t)
EX_EXPR(def)		///< non-temporary variable (::def_t)
EX_EXPR(symbol)		///< non-temporary variable (::symbol_t)
EX_EXPR(temp)		///< temporary variable (::ex_temp_t)
EX_EXPR(vector)		///< "vector" expression (::ex_vector_t)
EX_EXPR(selector)	///< selector expression (::ex_selector_t)
EX_EXPR(nil)		///< umm, nil, null. nuff said (0 of any type)
EX_EXPR(value)		///< constant value (::ex_value_t)
EX_EXPR(compound)	///< compound initializer
EX_EXPR(memset)		///< memset needs three params...
EX_EXPR(alias)		///< view expression as different type (::ex_alias_t)
EX_EXPR(address)	///< address of an lvalue expression (::ex_address_t)
EX_EXPR(assign)		///< assignment of src expr to dst expr (::ex_assign_t)
EX_EXPR(branch)		///< branch expression  (::ex_branch_t)
EX_EXPR(inout)		///< inout arg expression  (::ex_inout_t)
EX_EXPR(return)		///< return expression  (::ex_return_t)
EX_EXPR(adjstk)		///< stack adjust expression  (::ex_adjstk_t)
EX_EXPR(with)		///< with expression  (::ex_with_t)
EX_EXPR(args)		///< @args marker in parameter list. no data
EX_EXPR(horizontal)	///< horizontal vector operation (::ex_horzontal_t)
EX_EXPR(swizzle)	///< vector swizzle operation (::ex_swizzle_t)
EX_EXPR(extend)		///< vector extend operation (::ex_extend_t)
EX_EXPR(multivec)	///< geometric algebra multivector (::ex_multivec_t)
EX_EXPR(list)		///< non-invasive list of expressions (::ex_list_t)
EX_EXPR(type)		///< type expression for generics
EX_EXPR(incop)		///< pre or post increment/decrement (::ex_incop_t)
EX_EXPR(cond)		///< ?: conditional expression (::ex_cond_t)
EX_EXPR(field)		///< field reference expression (::ex_field_t)
EX_EXPR(array)		///< array index expression (::ex_array_t)
EX_EXPR(decl)		///< delcaration expression (::ex_array_t)
EX_EXPR(loop)		///< loop construct expression (::ex_loop_t)
EX_EXPR(select)		///< select construct expression (::ex_select_t)

#undef EX_EXPR

///@}
