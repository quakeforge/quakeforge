/*
	gib_keys.c

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Brian Koropoff <brianhk@cs.washington.edu>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/keys.h"
#include "QF/sys.h"
#include "QF/gib.h"

#include "compat.h"
#include "old_keys.h"

static void
Key_GIB_Bind_Get_f (void)
{
	const char *key, *cmd;
	imt_t      *imt;
	int k;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("key");
		return;
	}

	key = OK_TranslateKeyName (GIB_Argv (1));

	if ((k = Key_StringToKeynum (key)) == -1) {
		GIB_Error ("bind", "bind::get: invalid key %s", key);
		return;
	}

	imt = Key_FindIMT ("imt_mod");
	if (!imt || !(cmd = Key_GetBinding (imt, k)))
		GIB_Return ("");
	else
		GIB_Return (cmd);
}

void
GIB_Key_Init (void)
{
	GIB_Builtin_Add ("bind::get", Key_GIB_Bind_Get_f);
}
