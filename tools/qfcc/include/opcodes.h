/*
	opcodes.h

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

#ifndef __opcodes_h
#define __opcodes_h

#include "QF/pr_comp.h"

typedef struct statref_s {
	struct statref_s *next;
	dstatement_t	*statement;
	int				field;		// a, b, c (0, 1, 2)
} statref_t;

extern opcode_t *op_done;
extern opcode_t *op_return;
extern opcode_t *op_if;
extern opcode_t *op_ifnot;
extern opcode_t *op_ifbe;
extern opcode_t *op_ifb;
extern opcode_t *op_ifae;
extern opcode_t *op_ifa;
extern opcode_t *op_state;
extern opcode_t *op_goto;
extern opcode_t *op_jump;
extern opcode_t *op_jumpb;

statref_t *PR_NewStatref (dstatement_t *st, int field);
void PR_AddStatementRef (struct def_s *def, dstatement_t *st, int field);
struct def_s *PR_Statement (opcode_t *op, struct def_s *var_a,
							struct def_s *var_b);
opcode_t *PR_Opcode_Find (const char *name, struct def_s *var_a,
						  struct def_s *var_b, struct def_s *var_c);
void PR_Opcode_Init_Tables (void);

#endif//__opcodes_h
