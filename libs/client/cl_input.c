/*
	cl_legacy.c

	Client legacy commands

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/11/24

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/plist.h"
#include "QF/sys.h"

#include "old_keys.h"

#include "client/input.h"

static const char default_input_config[] = {
#include "libs/client/default_input.plc"
};

static void
cl_bind_f (void)
{
	int         c, i;
	const char *key;
	static dstring_t *cmd_buf;

	if (!cmd_buf) {
		cmd_buf = dstring_newstr ();
	}

	c = Cmd_Argc ();

	if (c < 2) {
		Sys_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	if (strcasecmp (Cmd_Argv (1), "ESCAPE") == 0) {
		return;
	}

	if (!(key = OK_TranslateKeyName (Cmd_Argv (1)))) {
		return;
	}

	dsprintf (cmd_buf, "in_bind imt_mod %s", key);
	if (c >= 3) {
		for (i = 2; i < c; i++) {
			dasprintf (cmd_buf, " \"%s\"", Cmd_Argv (i));
		}
	}

	Cmd_ExecuteString (cmd_buf->str, src_command);
}

static void
cl_unbind_f (void)
{
	int         c;
	const char *key;
	static dstring_t *cmd_buf;

	if (!cmd_buf) {
		cmd_buf = dstring_newstr ();
	}

	c = Cmd_Argc ();

	if (c < 2) {
		Sys_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	if (!(key = OK_TranslateKeyName (Cmd_Argv (1)))) {
		return;
	}
	dsprintf (cmd_buf, "in_unbind imt_mod %s", key);
	Cmd_ExecuteString (cmd_buf->str, src_command);
}

void
CL_Legacy_Init (void)
{
	OK_Init ();
	Cmd_AddCommand ("bind", cl_bind_f, "compatibility wrapper for in_bind");
	Cmd_AddCommand ("unbind", cl_unbind_f, "compatibility wrapper for in_bind");
	// FIXME hashlinks
	IN_LoadConfig (PL_GetPropertyList (default_input_config, 0));
}
