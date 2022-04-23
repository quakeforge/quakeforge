/*
	game.c

	game specific support (notably hipnotic, rogue and abyss)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
#include "QF/info.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "qw/include/game.h"
#include "qw/include/server.h"

char *registered;
static cvar_t registered_cvar = {
	.name = "registered",
	.description =
		"Is the game the registered version. 1 yes 0 no",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = 0/* not used */, .value = &registered },
};
int         static_registered = 1;		// only for startup check, then set

/*
	Game_CheckRegistered

	Looks for the pop.txt file and verifies it.
	Sets the "registered" cvar.
	Immediately exits out if an alternate game was attempted to be started
	without being registered.
*/
static void
Game_CheckRegistered (void)
{
	QFile      *h;

	h = QFS_FOpenFile ("gfx/pop.lmp");
	static_registered = 0;

	if (h) {
		static_registered = 1;
		Qclose (h);
	}

	if (static_registered) {
		Cvar_Set ("registered", "1");
		Sys_Printf ("Playing registered version.\n");
	}
}

/*
	SV_Gamedir_f

	Sets the gamedir and path to a different directory.
*/
static void
SV_Gamedir_f (void)
{
	const char *dir;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("Current gamedir: %s\n", qfs_gamedir->gamedir);
		return;
	}

	if (Cmd_Argc () != 2) {
		Sys_Printf ("Usage: gamedir <newdir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":")) {
		Sys_Printf ("Gamedir must be a single filename, not a path\n");
		return;
	}

	QFS_Gamedir (dir);
}

void
Game_Init (void)
{
	Cvar_Register (&registered_cvar, 0, 0);
	Game_CheckRegistered ();
	Cmd_AddCommand ("gamedir", SV_Gamedir_f,
					"Specifies the directory to be used while playing.");
}

void
Game_Init_Cvars (void)
{
}
