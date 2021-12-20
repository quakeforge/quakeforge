/*
	rua_internal.h

	ruamoko internal prototypes

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2003/1/15

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

#ifndef __rua_internal_h
#define __rua_internal_h

#include "QF/quakeio.h"

void RUA_Cbuf_Init (struct progs_s *pr, int secure);
void RUA_Cmd_Init (struct progs_s *pr, int secure);
void RUA_Cvar_Init (struct progs_s *pr, int secure);
void RUA_Hash_Init (struct progs_s *pr, int secure);
void RUA_Math_Init (struct progs_s *pr, int secure);
void RUA_MsgBuf_Init (struct progs_s *pr, int secure);
void RUA_Obj_Init (struct progs_s *pr, int secure);
void RUA_Plist_Init (struct progs_s *pr, int secure);
void RUA_Runtime_Init (struct progs_s *pr, int secure);
void RUA_Script_Init (progs_t *pr, int secure);
void RUA_Set_Init (progs_t *pr, int secure);
void RUA_Stdlib_Init (struct progs_s *pr, int secure);
void RUA_String_Init (struct progs_s *pr, int secure);
void RUA_QFile_Init (struct progs_s *pr, int secure);
void RUA_QFS_Init (struct progs_s *pr, int secure);

int QFile_AllocHandle (struct progs_s *pr, QFile *file);
QFile *QFile_GetFile (struct progs_s *pr, int handle);

void RUA_Input_Init (struct progs_s *pr, int secure);

#endif//__rua_internal_h
