/*
	gib_interpret.h

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

#define GIB_MAXCALLS 2048
#define GIB_MAXSUBARGS 256

extern char *gib_subargv[256];
extern int gib_subargc;

char *GIB_Argv(int i);
int GIB_Argc(void);
void GIB_Strip_Arg (char *arg);
int GIB_Execute_Block (char *block, int retflag);
int GIB_Execute_Inst (void);
int GIB_Execute_Sub (void);
int GIB_Interpret_Inst (char *inst);
int GIB_Run_Inst (char *inst);
int GIB_Run_Sub (gib_module_t *mod, gib_sub_t *sub);
