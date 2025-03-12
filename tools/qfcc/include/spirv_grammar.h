/*
	spirv_grammar.c

	SPIR-V grammar json embedding

	Copyright (C) 2024 Bill Currie

	Author: Bill Currie <bill@taniwha.org>

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

#ifndef __spirv_grammar_h
#define __spirv_grammar_h

#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/strpool.h"

typedef struct symtab_s symtab_t;
typedef struct symbol_s symbol_t;

typedef struct validx_s {
	uint32_t    value;
	uint32_t    index;
} validx_t;

typedef struct DARRAY_TYPE (validx_t) validx_set_t;

typedef struct spirv_operand_s {
	pr_string_t kind;
	char        quantifier;	// ?
	pr_string_t name;		// ?
} spirv_operand_t;

typedef struct DARRAY_TYPE (spirv_operand_t) spirv_operand_set_t;

typedef struct spiv_enumerant_s {
	pr_string_t enumerant;
	uint32_t    value;

	pr_ptr_t    aliases;
	uint32_t    num_aliases;

	uint32_t    num_capabilities;
	pr_ptr_t    capabilities;

	uint32_t    num_extensions;
	pr_ptr_t    extensions;

	uint32_t    num_parameters;
	pr_ptr_t    parameters;

	uint16_t    version;
	uint16_t    lastVersion;

	uint32_t    index;
	symbol_t   *sym;
} spirv_enumerant_t;

typedef struct DARRAY_TYPE (spirv_enumerant_t) spirv_enumerant_set_t;

typedef struct spirv_kind_s {
	pr_string_t category;
	pr_string_t kind;
	pr_string_t doc;
	uint32_t    num;
	union {
		struct {
			pr_ptr_t    enumerants;
			pr_ptr_t    names;
			uint32_t    num_names;
		};
		pr_ptr_t    bases;
	};
	symbol_t   *sym;
	struct spirv_grammar_s *grammar;
} spirv_kind_t;

typedef struct spirv_instruction_s {
	pr_string_t opname;
	uint32_t    opcode;
	pr_ptr_t    aliases;
	uint32_t    num_aliases;
	uint32_t    num_operands;
	pr_ptr_t    operands;
	pr_ptr_t    capabilities;
	uint32_t    num_capabilities;
	uint32_t    num_extensions;
	pr_ptr_t    extensions;
	uint16_t    version;
	uint16_t    lastVersion;
} spirv_instruction_t;

typedef struct DARRAY_TYPE (uint32_t) spirv_val_set_t;

typedef struct spirv_grammar_s {
	struct spirv_grammar_s *parent;	// for inheriting operand kinds
	const char **copyright;	// array of string objects
	uint32_t    num_copyright;
	uint32_t    magic_number;
	uint32_t    major_version;
	uint32_t    minor_version;
	uint32_t    version;
	uint32_t    revision;

	uint32_t    num_instructions;
	spirv_instruction_t *instructions;
	strid_set_t opname_index;
	validx_set_t opcode_index;

	uint32_t    num_operand_kinds;
	spirv_kind_t *operand_kinds;
	strid_set_t kind_index;

	spirv_enumerant_set_t enumerants;
	strid_set_t enumerant_index;
	validx_set_t value_index;

	spirv_operand_set_t operands;

	strid_set_t capabilities;
	strid_set_t extensions;

	spirv_val_set_t aliases;
	spirv_val_set_t bases;

	strpool_t  *strings;
} spirv_grammar_t;

typedef struct plitem_s plitem_t;
typedef struct expr_s expr_t;

const spirv_grammar_t *spirv_grammar (const char *set);
const spirv_instruction_t *spirv_instruction_op (const spirv_grammar_t *grammar,
												 uint32_t op) __attribute__((pure));
const spirv_instruction_t *spirv_instruction (const spirv_grammar_t *grammar,
											  const expr_t *opcode);

bool spirv_setup_intrinsic_symtab (symtab_t *symtab);
bool spirv_enum_val_silent (const char *enum_name, const char *enumerant,
							uint32_t *val);
uint32_t spirv_enum_val (const char *enum_name, const char *enumerant);
const spirv_enumerant_t *spirv_enumerant (const spirv_grammar_t *grammar,
										  const char *enum_name,
										  const char *enumerant);
const spirv_enumerant_t *spirv_enumerant_idx (const spirv_grammar_t *grammar,
											  const char *enum_name,
											  uint32_t index);
const spirv_enumerant_t *spirv_enumerant_val (const spirv_grammar_t *grammar,
											  const char *enum_name,
											  uint32_t val);

#endif//__spirv_grammar_h
