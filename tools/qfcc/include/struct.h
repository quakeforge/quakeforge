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

*/

#ifndef __struct_h
#define __struct_h

struct def_s;
enum storage_class_e;
struct symbol_s;
struct symtab_s;
struct type_s;

typedef struct {
	const char *name;
	struct type_s *type;
	void (*emit) (struct def_s *def, void *data, int index);
} struct_def_t;

struct symbol_s *find_struct (int su, struct symbol_s *tag,
							  struct type_s *type);
struct symbol_s *build_struct (int su, struct symbol_s *tag,
							   struct symtab_s *symtab, struct type_s *type);
struct symbol_s *find_enum (struct symbol_s *tag);
struct symtab_s *start_enum (struct symbol_s *enm);
struct symbol_s *finish_enum (struct symbol_s *sym);
void add_enum (struct symbol_s *enm, struct symbol_s *name,
			   struct expr_s *val);
int enum_as_bool (struct type_s *enm, struct expr_s **zero,
				  struct expr_s **one);

struct symbol_s *make_structure (const char *name, int su, struct_def_t *defs,
								 struct type_s *type);
struct def_s * emit_structure (const char *name, int su, struct_def_t *defs,
							   struct type_s *type, void *data,
							   struct defspace_s *space,
							   enum storage_class_e storage);

#endif//__struct_h
