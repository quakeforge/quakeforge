/*
	spirv.c

	qfcc spir-v file support

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

#ifndef __spirv_h
#define __spirv_h

#include <spirv/unified1/spirv.h>

#include "QF/darray.h"

#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/expr.h"

typedef struct symbol_s symbol_t;

typedef struct entrypoint_s {
	struct entrypoint_s *next;
	SpvExecutionModel model;
	const char *name;
	attribute_t *modes;
	ex_list_t   interface;			///< list of symbols forming interface
	struct DARRAY_TYPE (symbol_t *) interface_syms;
	struct function_s *func;
} entrypoint_t;

typedef struct module_s {
	ex_list_t   capabilities;
	ex_list_t   extensions;
	SpvExecutionModel default_model;
	symtab_t   *extinst_imports;
	const expr_t *addressing_model;
	const expr_t *memory_model;
	entrypoint_t *entry_points;
	defspace_t *entry_point_space;
	defspace_t *exec_modes;
	// debug
	defspace_t *strings;
	defspace_t *names;
	defspace_t *module_processed;
	// annotations
	defspace_t *decorations;
	// types, non-function variables, undefs
	defspace_t *globals;
	// functions
	defspace_t *func_declarations;
	defspace_t *func_definitions;
} module_t;

void spirv_add_capability (module_t *module, SpvCapability capability);
void spirv_add_extension (module_t *module, const char *extension);
void spirv_add_extinst_import (module_t *module, const char *import);
void spirv_set_addressing_model (module_t *module, SpvAddressingModel model);
void spirv_set_memory_model (module_t *module, SpvMemoryModel model);
bool spirv_write (struct pr_info_s *pr, const char *filename);

const struct plitem_s *spirv_operand_kind (const char *set, const char *kind);

#endif//__spirv_h
