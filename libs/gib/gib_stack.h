/*
	gib_stack.h

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

#define GIB_LOCALS gib_substack[gib_subsp - 1].local
#define GIB_CURRENTMOD gib_substack[gib_subsp - 1].mod
#define GIB_CURRENTSUB gib_substack[gib_subsp - 1].sub

typedef struct gib_instack_s
{
	gib_inst_t *instruction;
	char **argv;
	int argc;
} gib_instack_t;

typedef struct gib_substack_s
{
	gib_module_t *mod;
	gib_sub_t *sub;
	gib_var_t *local;
} gib_substack_t;

extern gib_instack_t *gib_instack;
extern gib_substack_t *gib_substack;

extern int gib_insp;
extern int gib_subsp;

void GIB_InStack_Push (gib_inst_t *instruction, int argc, char **argv);
void GIB_InStack_Pop ();
void GIB_SubStack_Push (gib_module_t *mod, gib_sub_t *sub, gib_var_t *local);
void GIB_SubStack_Pop ();

