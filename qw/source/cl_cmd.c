/*
	cl_cmd.c

	Client-side script command processing module

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/teamplay.h"

#include "qw/include/client.h"

static void
send_say (const char *str)
{
	static dstring_t *teambuf;
	const char *s;

	if (!teambuf) {
		teambuf = dstring_newstr ();
	}

	s = Team_ParseSay (teambuf, Cmd_Args (1));
	if (*s && *s < 32 && *s != 10) {
		// otherwise the server would eat leading characters
		// less than 32 or greater than 127
		SZ_Print (&cls.netchan.message, "\"");
		SZ_Print (&cls.netchan.message, s);
		SZ_Print (&cls.netchan.message, "\"");
	} else
		SZ_Print (&cls.netchan.message, s);
}

/*
	CL_Cmd_ForwardToServer

	adds the current command line as a clc_stringcmd to the client message.
	things like godmode, noclip, etc, are commands directed to the server,
	so when they are typed in at the console, they will need to be forwarded.
*/
void
CL_Cmd_ForwardToServer (void)
{
	if (cls.state == ca_disconnected) {
		Sys_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected


	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, Cmd_Argv (0));
	if (Cmd_Argc () > 1) {
		SZ_Print (&cls.netchan.message, " ");

		if (!strcasecmp (Cmd_Argv (0), "say") ||
			!strcasecmp (Cmd_Argv (0), "say_team")) {
			send_say(Cmd_Args (1));
		} else {
			SZ_Print (&cls.netchan.message, Cmd_Args (1));
		}
	}
}

// don't forward the first argument
static void
CL_Cmd_ForwardToServer_f (void)
{
	if (cls.state == ca_disconnected) {
		Sys_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (strcasecmp (Cmd_Argv (1), "snap") == 0) {
		Cbuf_InsertText (cl_cbuf, "snap\n");
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected

	if (Cmd_Argc () > 1) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args (1));
	}
}

void
CL_Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("cmd", CL_Cmd_ForwardToServer_f, "Send a command to the "
					"server.\n"
					"Commands:\n"
					"download - Same as the command.\n"
					"kill - Same as the command.\n"
					"msg (value) - Same as the command.\n"
					"prespawn (entity) (spot) - Find a spawn spot for the"
					" player entity.\n"
					"spawn (entity) - Spawn the player entity.\n"
					"setinfo - Same as the command.\n"
					"serverinfo - Same as the command.");
}
