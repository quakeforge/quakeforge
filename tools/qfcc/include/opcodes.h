/*
	opcodes.h

	opcode searching

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

*/

#ifndef __opcodes_h
#define __opcodes_h

#include "QF/progs/pr_comp.h"

typedef struct instruction_s instruction_t;

struct operand_s;
pr_ushort_t opcode_get (instruction_t *inst) __attribute__((pure));
instruction_t *opcode_find (const char *name, struct operand_s *op_a,
							struct operand_s *op_b, struct operand_s *op_c);
void opcode_init (void);

#endif//__opcodes_h
