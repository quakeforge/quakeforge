/*
	gib_modules.h

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
#include "QF/quakeio.h"

void GIB_Module_Load (char *name, QFile *f);
gib_module_t *GIB_Create_Module (char *name);
gib_sub_t *GIB_Create_Sub (gib_module_t *mod, char *name);
void GIB_Read_Sub (gib_sub_t *sub, QFile *f);
gib_module_t *GIB_Find_Module (char *name);
gib_sub_t *GIB_Find_Sub (gib_module_t *mod, char *name);
void GIB_Stats_f (void);
