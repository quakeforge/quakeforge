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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "QF/qtypes.h"
#include "QF/cbuf.h"
#include "QF/quakefs.h"
#include "QF/cmd.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/cvar.h"
#include "QF/gib.h"

#include "gib_parse.h"
#include "gib_vars.h"
#include "gib_regex.h"
#include "gib_builtin.h"
#include "gib_thread.h"
#include "gib_handle.h"
#include "gib_object.h"

#include "QF/csqc.h"
#define U __attribute__ ((used))
static U void (*const gib_progs_init)(struct progs_s *) = GIB_Progs_Init;
#undef U


static void
GIB_Exec_Override_f (void)
{
	char       *f;
	size_t      mark;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark (0);
	f = (char *) QFS_LoadHunkFile (QFS_FOpenFile (Cmd_Argv (1)));
	if (!f) {
		Sys_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}
	if (!Cvar_Command ()
		&& (cmd_warncmd
			|| (developer && developer & SYS_dev)))
		Sys_Printf ("execing %s\n", Cmd_Argv (1));
	if ((strlen (Cmd_Argv (1)) >= 4
	     && !strcmp (Cmd_Argv (1) + strlen (Cmd_Argv (1)) - 4, ".gib"))
		|| cbuf_active->interpreter == GIB_Interpreter ()) {
		// GIB script, put it in a new buffer on the stack
		cbuf_t     *sub = Cbuf_PushStack (GIB_Interpreter ());

		GIB_DATA (sub)->script = malloc (sizeof (gib_script_t));
		GIB_DATA (sub)->script->file = strdup (Cmd_Argv (1));
		GIB_DATA (sub)->script->text = strdup (f);
		GIB_DATA (sub)->script->refs = 1;
		Cbuf_AddText (sub, f);
		if (gib_parse_error && cbuf_active->interpreter == GIB_Interpreter ())
			GIB_Error ("parse", "%s: Parse error while executing %s.",
					   Cmd_Argv (0), Cmd_Argv (1));
	} else
		Cbuf_InsertText (cbuf_active, f);
	Hunk_FreeToLowMark (0, mark);
}

VISIBLE void
GIB_Init (qboolean sandbox)
{
	// Override the exec command with a GIB-aware one
	if (Cmd_Exists ("exec")) {
		Cmd_RemoveCommand ("exec");
		Cmd_AddCommand ("exec", GIB_Exec_Override_f, "Execute a script file.");
	}
	// Initialize handle system
	GIB_Handle_Init ();
	// Initialize variables
	GIB_Var_Init ();
	// Initialize regex cache
	GIB_Regex_Init ();
	// Initialize builtins
	GIB_Builtin_Init (sandbox);
	// Initialize thread system;
	GIB_Thread_Init ();
	// Initialize event system
	GIB_Event_Init ();
	// Initialize object system
	GIB_Object_Init ();
}
