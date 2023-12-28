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

struct progs_s;
struct dstring_s;

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

// the variable args are assumed to come immediately after fmt_arg
void RUA_Sprintf (struct progs_s *pr, struct dstring_s *dstr, const char *func,
				  int fmt_arg);

int QFile_AllocHandle (struct progs_s *pr, QFile *file);
QFile *QFile_GetFile (struct progs_s *pr, int handle);
struct plitem_s *Plist_GetItem (struct progs_s *pr, int handle);

void RUA_GUI_Init (struct progs_s *pr, int secure);
struct canvas_system_s RUA_GUI_GetCanvasSystem (struct progs_s *pr);
struct passage_s *RUA_GUI_GetPassage (struct progs_s *pr, int handle);
void RUA_IMUI_Init (struct progs_s *pr, int secure);
void RUA_Input_Init (struct progs_s *pr, int secure);
void RUA_Mersenne_Init (struct progs_s *pr, int secure);
void RUA_Model_Init (struct progs_s *pr, int secure);
struct model_s *Model_GetModel (progs_t *pr, int handle);
void RUA_Scene_Init (struct progs_s *pr, int secure);
struct scene_s *Scene_GetScene (struct progs_s *pr, pr_ulong_t handle);

#endif//__rua_internal_h
