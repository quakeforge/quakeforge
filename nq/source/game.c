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
# include <config.h>
#endif

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "nq/include/game.h"
#include "nq/include/server.h"

qboolean standard_quake = false;

float registered;
static cvar_t registered_cvar = {
	.name = "registered",
	.description =
		"Is the game the registered version. 1 yes 0 no",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &registered },
};
char *cmdline;
static cvar_t cmdline_cvar = {
	.name = "cmdline",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_SERVERINFO,
	.value = { .type = 0, .value = &cmdline },
};
int         static_registered = 1;

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

void
Game_Init (memhunk_t *hunk)
{
	int         i;
	const char *game = "nq";

	// FIXME: make this dependant on QF metadata in the mission packs
	standard_quake = true;

	if ((i = COM_CheckParm ("-hipnotic"))) {
		standard_quake = false;
		game = "hipnotic";
	} else if ((i = COM_CheckParm ("-rogue"))) {
		standard_quake = false;
		game = "rogue";
	} else if ((i = COM_CheckParm ("-abyss"))) {
		game = "abyss";
	}
	QFS_Init (hunk, game);

	Cvar_Register (&registered_cvar, 0, 0);
	Cvar_Register (&cmdline_cvar, Cvar_Info, &cmdline);

	Game_CheckRegistered ();
}
