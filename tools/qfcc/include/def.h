/*
	def.h

	def management and symbol tables

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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

#ifndef __def_h
#define __def_h

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

typedef struct def_s {
	struct def_s	*next;			///< general purpose linking

	struct type_s	*type;
	const char		*name;
	struct defspace_s *space;
	int				offset;

	struct def_s   *alias;
	struct reloc_s *relocs;			///< for relocations

	unsigned		offset_reloc:1;	///< use *_def_ofs relocs
	unsigned		initialized:1;
	unsigned		constant:1;		///< stores constant value
	unsigned		global:1;		///< globally declared def
	unsigned		external:1;		///< externally declared def
	unsigned		local:1;		///< function local def
	unsigned		system:1;		///< system def
	unsigned        nosave:1;		///< don't set DEF_SAVEGLOBAL

	string_t		file;			///< source file
	int				line;			///< source line

	int              obj_def;		///< index to def in qfo defs

	void			*return_addr;	///< who allocated this
} def_t;

typedef enum storage_class_e {
	st_global,
	st_system,
	st_extern,
	st_static,
	st_local
} storage_class_t;

extern storage_class_t current_storage;

def_t *new_def (const char *name, struct type_s *type,
				struct defspace_s *space, storage_class_t storage);
def_t *alias_def (def_t *def, struct type_s *type);
void free_def (def_t *def);

void def_to_ddef (def_t *def, ddef_t *ddef, int aux);

struct symbol_s;
struct expr_s;

void initialize_def (struct symbol_s *sym, struct type_s *type,
					 struct expr_s *init, struct defspace_s *space,
					 storage_class_t storage);

#endif//__def_h
