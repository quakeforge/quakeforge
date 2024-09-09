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
glsl_parse_declaration (specifier_t spec, symbol_t *sym, const type_t *array,
						const expr_t *init, symtab_t *symtab)
{
	if (array) {
		spec.type = append_type (array, spec.type);
		spec.type = find_type (spec.type);
	}
	/*auto attributes =*/ glsl_optimize_attributes (spec.attributes);
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
			declare_symbol (spec, init, symtab);
		}
	} else {
		spec.sym = sym;
		if (spec.sym) {
			declare_symbol (spec, init, symtab);
		}
	}
}

void
glsl_declare_field (specifier_t spec, symtab_t *symtab)
{
	/*auto attributes =*/ glsl_optimize_attributes (spec.attributes);
	declare_field (spec, symtab);
}
