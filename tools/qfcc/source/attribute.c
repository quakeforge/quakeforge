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

ALLOC_STATE (attribute_t, attributes);

attribute_t *
new_attribute(const char *name, const expr_t *params)
{
	if (params
		&& params->type != ex_list
		&& params->type != ex_value) {
		internal_error (params, "attribute params not a list");
	}
	bool err = false;
	if (params && params->type == ex_list) {
		for (auto p = params->list.head; p; p = p->next) {
			auto e = p->expr;
			if (e->type == ex_expr) {
				if (e->expr.op != '='
					|| !is_string_val (e->expr.e1)
					|| e->expr.e2->type != ex_value) {
					error (e, "not a key=literal constnat");
					err = true;
				}
			} else if (e->type != ex_value) {
				error (e, "not a literal constant");
				err = true;
			}
		}
	}
	if (err) {
		return nullptr;
	}

	attribute_t *attr;
	ALLOC (16384, attribute_t, attributes, attr);
	attr->name = save_string (name);
	attr->params = params;
	return attr;
}
