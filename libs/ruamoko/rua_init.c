/*
	bi_init.c

	CSQC builtins init

	Copyright (C) 1996-1997  Id Software, Inc.

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/progs.h"
#include "QF/ruamoko.h"

#include "rua_internal.h"

static void (*init_funcs[])(progs_t *, int) = {
	RUA_Obj_Init,			// done early (for the heck of it at this stage)

	RUA_Cbuf_Init,
	RUA_Cmd_Init,
	RUA_Cvar_Init,
	RUA_Hash_Init,
	RUA_Math_Init,
	RUA_MsgBuf_Init,
	RUA_Plist_Init,
	RUA_QFile_Init,
	RUA_QFS_Init,
	RUA_Runtime_Init,
	RUA_Script_Init,
	RUA_Set_Init,
	RUA_String_Init,
};

VISIBLE void
RUA_Init (progs_t *pr, int secure)
{
	size_t      i;

	for (i = 0; i < sizeof (init_funcs) / sizeof (init_funcs[0]); i++)
		init_funcs[i] (pr, secure);
}
