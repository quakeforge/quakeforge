/*
	attribute.h

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

#ifndef attribute_h
#define attribute_h

typedef struct attribute_s {
	struct attribute_s *next;
	const char *name;
	const struct expr_s *params;
} attribute_t;

typedef struct expr_s expr_t;
typedef struct rua_ctx_s rua_ctx_t;
attribute_t *new_attrfunc(const char *name, const expr_t *params);
attribute_t *new_attribute(const char *name, const expr_t *params,
						   rua_ctx_t *ctx);
attribute_t *expr_attributes (const expr_t *attrs, attribute_t *append_attrs,
							  rua_ctx_t *ctx);

#endif//attribute_h
