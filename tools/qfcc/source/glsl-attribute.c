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
#include "tools/qfcc/include/value.h"

const char *glsl_interface_names[glsl_num_interfaces] = {
	"in",
	"out",
	"uniform",
	"buffer",
	"shared",
	"push_constant",
};

symtab_t *
glsl_optimize_attributes (attribute_t *attributes)
{
	auto attrtab = new_symtab (nullptr, stab_param);

	for (auto attr = attributes; attr; attr = attr->next) {
		auto attrsym = symtab_lookup (attrtab, attr->name);
		if (attrsym) {
			if (strcmp (attr->name, "layout") == 0) {
				list_append_list (&attrsym->list, &attr->params->list);
			} else {
				error (attr->params, "duplicate %s", attr->name);
			}
		} else {
			attrsym = new_symbol (attr->name);
			if (strcmp (attr->name, "layout") == 0) {
				attrsym->sy_type = sy_list;
				list_append_list (&attrsym->list, &attr->params->list);
			} else {
				if (attr->params) {
					notice (attr->params, "attribute params: %s", attr->name);
					attrsym->sy_type = sy_expr;
					attrsym->expr = attr->params;
				}
			}
			symtab_addsymbol (attrtab, attrsym);
		}
#if 0
		notice (0, "%s", attr->name);
		if (!attr->params) continue;
		for (auto p = attr->params->list.head; p; p = p->next) {
			if (p->expr->type == ex_expr) {
				notice (0, "    %s = %s",
						get_value_string (p->expr->expr.e1->value),
						get_value_string (p->expr->expr.e2->value));
			} else {
				notice (0, "    %s", get_value_string (p->expr->value));
			}
		}
#endif
	}
	return attrtab;
}

void
glsl_apply_attributes (symtab_t *attributes, specifier_t spec)
{
	for (auto attr = attributes->symbols; attr; attr = attr->next) {
		if (strcmp (attr->name, "layout") == 0) {
			if (attr->sy_type != sy_list) {
				internal_error (0, "bogus layout qualifier");
			}
			glsl_layout (&attr->list, spec);
		}
	}
}
