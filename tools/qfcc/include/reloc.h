/*
	reloc.h

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

typedef enum {
	rel_none,
	rel_op_a_def,
	rel_op_b_def,
	rel_op_c_def,
	rel_op_a_op,
	rel_op_b_op,
	rel_op_c_op,
	rel_def_op,
	rel_def_def,
	rel_def_func,
	rel_def_string,
	rel_def_field,
} reloc_type;

typedef struct reloc_s {
	struct reloc_s *next;
	int			ofs;
	reloc_type	type;
} reloc_t;

struct statement_s;

reloc_t *new_reloc (int ofs, reloc_type type);
void relocate_refs (reloc_t *refs, int ofs);
void reloc_def_def (struct def_s *def, int ofs);
void reloc_def_func (struct function_s *func, int ofs);
void reloc_def_string (int ofs);
void reloc_def_field (struct def_s *def, int ofs);

#endif//__reloc_h
