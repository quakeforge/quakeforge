/*
	struct.h

	structure support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/12/08

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

	$Id$
*/

#ifndef __struct_h
#define __struct_h

typedef enum {
	vis_private,
	vis_protected,
	vis_public,
} visibility_type;

typedef enum {
	str_none,
	str_struct,
	str_union,
} struct_type;

typedef struct struct_field_s {
	struct struct_field_s *next;
	const char		*name;
	struct type_s	*type;
	int				offset;
	visibility_type	visibility;
} struct_field_t;

typedef struct struct_s {
	const char *name;
	struct type_s *type;
	struct_type stype;
	struct hashtab_s *struct_fields;
	struct_field_t *struct_head;
	struct_field_t **struct_tail;
	int         size;
	void       *return_addr;		// who allocated this
} struct_t;

typedef struct enum_s {
	const char *name;
	int         value;
} enum_t;

struct_field_t *new_struct_field (struct_t *strct, struct type_s *type,
								  const char *name,
								  visibility_type visibility);
struct_field_t *struct_find_field (struct_t *strct, const char *name);
int struct_compare_fields (struct_t *s1, struct_t *s2);
struct type_s *init_struct (struct_t *strct, struct type_s *type,
							struct_type stype, const char *name);
struct_t *get_struct (const char *name, int create);
void copy_struct_fields (struct_t *dst, struct_t *src);

struct def_s *emit_struct (struct_t *strct, const char *name);

void process_enum (struct expr_s *enm);
struct expr_s *get_enum (const char *name);

void clear_structs (void);
void clear_enums (void);

struct_t *new_struct (const char *name);
struct_t *new_union (const char *name);
struct_t *decl_struct (const char *name);
struct_t *decl_union (const char *name);

#endif//__struct_h
