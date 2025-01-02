/*
	glsl-declaration.c

	GLSL declaration handling

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

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/va.h"

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/glsl-lang.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"

void
glsl_parse_declaration (specifier_t spec, symbol_t *sym, const expr_t *init,
						symtab_t *symtab, expr_t *block, rua_ctx_t *ctx)
{
	if (sym && sym->type) {
		internal_error (0, "unexected typed symbol");
	}
	auto attributes = glsl_optimize_attributes (spec.attributes);
	if (sym && sym->sy_type == sy_expr) {
		auto id_list = sym->expr;
		if (id_list->type != ex_list) {
			internal_error (id_list, "not a list");
		}
		for (auto id = id_list->list.head; id; id = id->next) {
			if (id->expr->type != ex_symbol) {
				internal_error (id_list, "not a symbol");
			}
			spec.sym = id->expr->symbol;
			spec.sym = declare_symbol (spec, init, symtab, block, ctx);
			glsl_apply_attributes (attributes, spec);
		}
	} else {
		spec.sym = sym;
		if (spec.sym) {
			if (spec.is_const && !init) {
				error (0, "uninitialized const %s", sym->name);
				init = new_zero_expr (spec.type);
			}
			if (spec.is_const && init->type != ex_compound) {
				auto type = get_type (init);
				if (type != spec.type && type_assignable (spec.type, type)) {
					if (!init->implicit && !type_promotes (spec.type, type)) {
						warning (init, "initialization of %s with %s"
								 " (use a cast)\n)",
								 get_type_string (spec.type),
								 get_type_string (type));
					}
					init = cast_expr (spec.type, init);
				}
				if (get_type (init) != spec.type) {
					error (init, "type mismatch in initializer");
					init = new_zero_expr (spec.type);
				}
				if (!is_constexpr (init)) {
					error (init, "non-constant initializer");
					init = new_zero_expr (spec.type);
				}
				sym->type = spec.type;
				sym->sy_type = sy_expr;
				sym->expr = init;
				auto s = symtab_lookup (symtab, sym->name);
				if (s && s->table == symtab) {
					error (0, "%s redeclared", sym->name);
				}
				symtab_addsymbol (symtab, sym);
			} else {
				spec.sym = declare_symbol (spec, init, symtab, block, ctx);
			}
		}
		glsl_apply_attributes (attributes, spec);
	}
}

void
glsl_declare_field (specifier_t spec, symtab_t *symtab, rua_ctx_t *ctx)
{
	auto attributes = glsl_optimize_attributes (spec.attributes);
	spec.sym = declare_field (spec, symtab, ctx);
	glsl_apply_attributes (attributes, spec);
}
