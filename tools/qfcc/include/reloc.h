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

/**	Relocation record.

	One relocation record is created for each reference that needs to be
	updated.
*/
typedef struct reloc_s {
	struct reloc_s *next;		///< next reloc in reloc chain
	const struct ex_label_s *label;	///< instruction label for *_op relocs
	struct defspace_s *space;	///< the space containing the location in
								///< need of adjustment for def_* relocations
								///< (op_* relocations always use the code
								///< space)
	int			offset;			///< the address of the location in need of
								///< adjustment
	reloc_type	type;			///< type type of relocation to perform
	int			line;			///< current source line when creating reloc
	pr_string_t file;			///< current source file when creating reloc
	const void *return_address;	///< for debugging
} reloc_t;

struct statement_s;
struct def_s;
struct function_s;

/**	Perform all relocations in a relocation chain.

	\param refs		The head of the relocation chain.
	\param offset	The value to be used in the relocations. This will either
					replace or be added to the value in the relocation target.
*/
void relocate_refs (reloc_t *refs, int offset);

/**	Create a relocation record of the specified type.

	The current source file and line will be preserved in the relocation
	record.

	\param space	The defspace containting the location to be adjusted.
	\param offset	The address of the location to be adjusted.
	\param type		The type of relocation to be performed.
*/
reloc_t *new_reloc (struct defspace_s *space, int offset, reloc_type type);

/**	Create a relocation record for an instruction referencing a def.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will replace the
	value stored in the instruction's operand field.

	\param def		The def being referenced.
	\param offset	The address of the instruction that will be adjusted.
	\param field	The instruction field to be adjusted: 0 = opa, 1 = opb,
					2 = opc.
*/
void reloc_op_def (struct def_s *def, int offset, int field);

/**	Create a relative relocation record for an instruction referencing a def.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will be added to
	the value stored in the instruction's operand field.

	\param def		The def being referenced.
	\param offset	The address of the instruction that will be adjusted.
	\param field	The instruction field to be adjusted: 0 = opa, 1 = opb,
					2 = opc.
*/
void reloc_op_def_ofs (struct def_s *def, int offset, int field);

/**	Create a relocation record for a data location referencing a def.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will replace the
	value stored in the data location.

	\param def		The def being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_def (struct def_s *def, const struct def_s *location);

/**	Create a relocation record for a data location referencing a def.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will be added to
	the value stored in the data location.

	\param def		The def being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_def_ofs (struct def_s *def, const struct def_s *location);

/**	Create a relocation record for a data location referencing a function.

	The relocation record will be linked into the function's chain of
	relocation records.

	When the relocation is performed, the function number will replace the
	value stored in the data location.

	\param func		The function being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_func (struct function_s *func, const struct def_s *location);

/**	Create a relocation record for a data location referencing a string.

	The relocation record will be linked into the global chain of
	relocation records.

	When the relocation is performed, the string index will replace the
	value stored in the data location.

	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_string (const struct def_s *location);

/**	Create a relocation record for a data location referencing a field.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will replace the
	value stored in the data location.

	\param def		The def representing the field being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_field (struct def_s *def, const struct def_s *location);

/**	Create a relocation record for a data location referencing a field.

	The relocation record will be linked into the def's chain of relocation
	records.

	When the relocation is performed, the target address will be added to
	the value stored in the data location.

	\param def		The def representing the field being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_field_ofs (struct def_s *def, const struct def_s *location);

/**	Create a relocation record for a data location referencing an
	instruction.

	The relocation record will be linked into the global chain of
	relocation records.

	When the relocation is performed, the string index will replace the
	value stored in the data location.

	\param label	The label representing the instruction being referenced.
	\param location	Def describing the location of the reference to be
					adjusted. As the def's space and offset will be copied
					into the relocation record, a dummy def may be used.
*/
void reloc_def_op (const struct ex_label_s *label,
				   const struct def_s *location);

void reloc_attach_relocs (reloc_t *relocs, reloc_t **location);

///@}

#endif//__reloc_h
