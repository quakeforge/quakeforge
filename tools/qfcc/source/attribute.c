/*
	attribute.c

	Attribute list handling

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/02/02

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

#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"

ALLOC_STATE (attribute_t, attributes);

attribute_t *
new_attrfunc (const char *name, const expr_t *params)
{
	attribute_t *attr;
	ALLOC (16384, attribute_t, attributes, attr);
	attr->name = save_string (name);
	attr->params = params;
	return attr;
}

attribute_t *
new_attribute(const char *name, const expr_t *params, rua_ctx_t *ctx)
{
	bool err = false;
	if (params && params->type == ex_list) {
		for (auto p = params->list.head; p; p = p->next) {
			auto e = p->expr;
			if (e->type == ex_expr && e->expr.op == '=') {
				if (e->expr.op == '=' && is_string_val (e->expr.e1)) {
					p->expr = new_binary_expr ('=', e->expr.e1,
											   expr_process(e->expr.e2, ctx));
					e = p->expr;
				}
				if (e->expr.op != '='
					|| !is_string_val (e->expr.e1)
					|| !is_constant (e->expr.e2)) {
					error (e, "not a key=constant");
					err = true;
				}
			} else {
				p->expr = expr_process(e, ctx);
				e = p->expr;
				if (!is_constant (e)) {
					error (e, "not a constant");
					err = true;
				}
			}
		}
	}
	if (err) {
		return nullptr;
	}
	return new_attrfunc (name, params);
}

static attribute_t *
expr_attr (const expr_t *attr, rua_ctx_t *ctx)
{
	if (attr->type == ex_symbol) {
		// simple name attribute
		auto sym = attr->symbol;
		return new_attribute (sym->name, nullptr, ctx);
	} else if (attr->type == ex_branch && attr->branch.type == pr_branch_call
			   && attr->branch.target
			   && attr->branch.target->type == ex_symbol) {
		if (!attr->branch.args) {
			error (attr, "function-style attributes require parameters");
			return nullptr;
		}
		auto sym = attr->branch.target->symbol;
		return new_attribute (sym->name, attr->branch.args, ctx);
	} else {
		error (attr, "not a simple name");
		return nullptr;
	}
}

attribute_t *
expr_attributes (const expr_t *attrs, attribute_t *append_attrs, rua_ctx_t *ctx)
{
	if (!attrs) {
		return append_attrs;
	}
	attribute_t *attributes = nullptr;
	attribute_t **a = &attributes;
	if (attrs->type == ex_list) {
		for (auto l = attrs->list.head; l; l = l->next) {
			if ((*a = expr_attr (l->expr, ctx))) {
				a = &(*a)->next;
			}
		}
	} else {
		if ((*a = expr_attr (attrs, ctx))) {
			a = &(*a)->next;
		}
	}
	*a = append_attrs;
	return attributes;
}
