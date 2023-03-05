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

attribute_t *new_attribute(const char *name, expr_t *value)
{
	if (value && value->type != ex_value) {
		error (value, "not a literal constant");
		return 0;
	}

	attribute_t *attr;
	ALLOC (16384, attribute_t, attributes, attr);
	attr->name = save_string (name);
	attr->value = value ? value->e.value : 0;
	return attr;
}
