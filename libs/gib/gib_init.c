/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

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

*/

static const char rcsid[] =
        "$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include "QF/qtypes.h"
#include "QF/cbuf.h"
#include "QF/quakefs.h"
#include "QF/gib_init.h"
#include "QF/gib_parse.h"
#include "QF/gib_builtin.h"
#include "QF/gib_regex.h"
#include "QF/gib_thread.h"
#include "QF/cmd.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/cvar.h"

static void
GIB_Exec_Override_f (void) {
	char       *f;
	int         mark;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark ();
	f = (char *) COM_LoadHunkFile (Cmd_Argv (1));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command() && (cmd_warncmd->int_val || (developer && developer->int_val)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));
	if (!strcmp (Cmd_Argv (1) + strlen (Cmd_Argv(1)) - 4, ".gib") || cbuf_active->interpreter == &gib_interp) {
		// GIB script, put it in a new buffer on the stack
		cbuf_t *sub = Cbuf_New (&gib_interp);
		if (cbuf_active->down)
			Cbuf_DeleteStack (cbuf_active->down);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		cbuf_active->state = CBUF_STATE_STACK;
		Cbuf_AddText (sub, f);
		GIB_Parse_Strip_Comments (sub);
	} else
		Cbuf_InsertText (cbuf_active, f);
	Hunk_FreeToLowMark (mark);
}

void
GIB_Init (qboolean sandbox)
{
	// Override the exec command with a GIB-aware one
	if (Cmd_Exists("exec")) {
		Cmd_RemoveCommand ("exec");
		Cmd_AddCommand ("exec", GIB_Exec_Override_f, "Execute a script file.");
	}
	// Initialize regex cache
	GIB_Regex_Init ();
	// Initialize builtins
	GIB_Builtin_Init (sandbox);
	// Initialize event system
	GIB_Event_Init ();
}
