/*
	gib_parse.h

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

int GIB_Get_Inst (const char *start);
int GIB_Get_Arg (const char *start);
int GIB_End_Quote (const char *start);
int GIB_End_DQuote (const char *start);
int GIB_End_Bracket (const char *start);
gib_sub_t *GIB_Get_ModSub_Sub (const char *modsub);
gib_module_t *GIB_Get_ModSub_Mod (const char *modsub);
int GIB_ExpandEscapes (char *source);
