/*
	com.c

	misc functions used in client and server

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/vfs.h"

#include "game.h"

void Cvar_Info (struct cvar_s *var);

cvar_t     *registered;
cvar_t     *cmdline;
int         static_registered = 1;


/*
	COM_CheckRegistered

	Looks for the pop.txt file and verifies it.
	Sets the "registered" cvar.
	Immediately exits out if an alternate game was attempted to be started
	without being registered.
*/
void
COM_CheckRegistered (void)
{
	unsigned short check[128];
	VFile      *h;

	COM_FOpenFile ("gfx/pop.lmp", &h);
	static_registered = 0;

	if (h) {
		static_registered = 1;
		Qread (h, check, sizeof (check));
		Qclose (h);
	}

	if (static_registered) {
		Cvar_Set (registered, "1");
		Con_Printf ("Playing registered version.\n");
	}
}

void
COM_Init (void)
{
	registered = Cvar_Get ("registered", "0", CVAR_NONE, NULL,
						   "Is the game the registered version. 1 yes 0 no");
	cmdline = Cvar_Get ("cmdline", "0", CVAR_SERVERINFO, Cvar_Info, "None");
	Cmd_AddCommand ("path", COM_Path_f, "Show what paths Quake is using");

	COM_Filesystem_Init_Cvars ();
	COM_Filesystem_Init ();
	COM_CheckRegistered ();
}
