/*
	reloc.h

	relocation support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/07

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

#ifndef __reloc_h
#define __reloc_h

/** \defgroup qfcc_reloc Relocation handling
	\ingroup qfcc
*/
///@{

/** Relocation record types.
	Types marked with * are relative and fixed up before the qfo is written.
	Types marked with ! are handled by only the linker.
	Types marked with + use pr.relocs
*/
typedef enum {
	rel_none,			///< no relocation
	rel_op_a_def,		///< code[ref.offset].a = def offset
	rel_op_b_def,		///< code[ref.offset].b = def offset
	rel_op_c_def,		///< code[ref.offset].c = def offset
	rel_op_a_op,		///< * code[ref.offset].a = code offset - ref.offset
	rel_op_b_op,		///< * code[ref.offset].b = code offset - ref.offset
	rel_op_c_op,		///< * code[ref.offset].c = code offset - ref.offset
	rel_def_op,			///< + data[ref.offset] = code offset
	rel_def_def,		///< data[ref.offset] = def offset
	rel_def_func,		///< +(sometimes) data[ref.offset] = offset
	rel_def_string,		///< + ! data[ref.offset] = string index
	rel_def_field,		///< ! data[ref.offset] = field def offset
	rel_op_a_def_ofs,	///< code[ref.offset].a += def offset
	rel_op_b_def_ofs,	///< code[ref.offset].b += def offset
	rel_op_c_def_ofs,	///< code[ref.offset].c += def offset
	rel_def_def_ofs,	///< data[ref.offset] += def offset
	rel_def_field_ofs,	///< data[ref.offset] += field def offset
} reloc_type;

typedef struct reloc_s {
	struct reloc_s *next;
	struct ex_label_s *label;
	int			offset;
	reloc_type	type;
	int			line;
	string_t	file;
} reloc_t;

struct statement_s;
struct def_s;
struct function_s;

reloc_t *new_reloc (int offset, reloc_type type);
void relocate_refs (reloc_t *refs, int offset);
void reloc_op_def (struct def_s *def, int offset, int field);
void reloc_op_def_ofs (struct def_s *def, int offset, int field);
void reloc_def_def (struct def_s *def, int offset);
void reloc_def_def_ofs (struct def_s *def, int offset);
void reloc_def_func (struct function_s *func, int offset);
void reloc_def_string (int offset);
void reloc_def_field (struct def_s *def, int offset);
void reloc_def_field_ofs (struct def_s *def, int offset);
void reloc_def_op (struct ex_label_s *label, int offset);

///@}

#endif//__reloc_h
