/*
	gib_instructions.h

	#DESCRIPTION#

	Copyright (C) 2000       #AUTHOR#

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

#include "QF/gib.h"

void GIB_AddInstruction (const char *name, gib_func_t func);
gib_inst_t *GIB_Find_Instruction (const char *name);
void GIB_Init_Instructions (void);
int GIB_Echo_f (void);
int GIB_Call_f (void);
int GIB_Return_f (void);
int GIB_Con_f (void);
int GIB_ListFetch_f (void);
int GIB_ExpandVars (const char *source, char *buffer, int bufferlen);
int GIB_ExpandBackticks (const char *source, char *buffer, int bufferlen);
