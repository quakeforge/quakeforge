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

typedef struct spirv_operand_s {
	const char *kind;
	char        quantifier;	// ?
	const char *name;		// ?
} spirv_operand_t;

typedef struct spiv_enumerant_s {
	const char *enumerant;
	uint32_t    value;
	uint32_t    num_capabilities;
	const char **capabilities;
	const char **extensions;
	uint32_t    num_extensions;
	uint32_t    num_parameters;
	spirv_operand_t *parameters;
	const char *version;
	const char *lastVersion;
} spirv_enumerant_t;

typedef struct spirv_kind_s {
	const char *category;
	const char *kind;
	const char *doc;
	uint32_t    num;
	union {
		spirv_enumerant_t *enumerants;
		const char **bases;
	};
} spirv_kind_t;

typedef struct spirv_instruction_s {
	const char *opname;
	uint32_t    opcode;
	uint32_t    num_operands;
	spirv_operand_t *operands;
	const char **capabilities;
	uint32_t    num_capabilities;
	uint32_t    num_extensions;
	const char **extensions;
	const char *version;
	const char *lastVersion;
} spirv_instruction_t;

typedef struct spirv_grammar_s {
	struct spirv_grammar_s *parent;	// for inheriting operand kinds
	const char **copyright;	// array of string objects
	uint32_t    num_copyright;
	uint32_t    magic_number;
	uint32_t    major_version;
	uint32_t    minor_version;
	uint32_t    version;
	uint32_t    revision;
	spirv_instruction_t *instructions;
	uint32_t    num_instructions;
	uint32_t    num_operand_kinds;
	spirv_kind_t *operand_kinds;
} spirv_grammar_t;

typedef struct plitem_s plitem_t;
typedef struct expr_s expr_t;

const plitem_t *spirv_operand_kind (const char *set, const char *kind);

uint32_t spirv_instruction_opcode (const char *set, const expr_t *opcode);

#endif//__spirv_grammar_h
