/*
	expr_array.c

	array expressions

	Copyright (C) 2001, 2025 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

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
#include "tools/qfcc/include/attribute.h"
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
#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

static const expr_t *
array_count (const type_t *array_type, rua_ctx_t *ctx)
{
	auto sym = new_symbol (".array_count");
	auto param = new_param (nullptr, array_type, "arr");
	auto type = find_type (parse_params (&type_uint, param));
	specifier_t spec = {
		.type = type,
		.params = param,
		.sym = sym,
		.is_overload = true,
	};
	auto symtab = current_symtab;
	current_symtab = pr.symtab;//FIXME clean up current_symtab
	sym = function_symbol (spec, ctx);
	spec.sym = sym;
	array_type = dereference_type (array_type);
	if (!sym->metafunc->expr) {
		auto func = begin_function (spec, nullptr, current_symtab, ctx);
		auto count_expr = new_uint_expr (type_count (array_type));
		sym->metafunc->expr = return_expr (func, count_expr);
		sym->metafunc->can_inline = true;
	}
	current_symtab = symtab;
	return new_symbol_expr (sym);
}

const expr_t *
array_property (const type_t *type, const attribute_t *attr, rua_ctx_t *ctx)
{
	auto array = attr->params;
	auto args = new_list_expr (edag_add_expr (array));

	auto array_type = get_type (array);
	auto expr = new_expr ();
	expr->type = ex_functor;
	expr->functor.func = array_count (array_type, ctx);
	expr->functor.args = args;
	return expr;
}
