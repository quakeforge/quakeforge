/*
	gib.c

	#DESCRIPTION#

	Copyright (C) 2000       #AUTHOR#

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/gib.h"
#include "QF/quakefs.h"

#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_modules.h"
#include "gib_parse.h"


// Standard cvars

void
GIB_Init (void)
{
	Cmd_AddCommand ("gibload", GIB_Load_f, "No Description");
	Cmd_AddCommand ("gibstats", GIB_Stats_f, "No Description");
	Cmd_AddCommand ("gib", GIB_Gib_f, "No Description");
	GIB_Init_Instructions ();
}

void
GIB_Gib_f (void)
{
	gib_sub_t  *sub;
	gib_module_t *mod;
	int         i, ret;

	if (!(mod = GIB_Get_ModSub_Mod (Cmd_Argv (1)))) {
		Con_Printf ("Module not found!\n");
		return;
	}
	if (!(sub = GIB_Get_ModSub_Sub (Cmd_Argv (1))))
		Con_Printf ("Subroutine not found!\n");
	else {
		gib_subargc = Cmd_Argc () - 1;
		gib_subargv[0] = sub->name;
		for (i = 1; i <= gib_subargc; i++)
			gib_subargv[i] = Cmd_Argv (i + 1);
		ret = GIB_Run_Sub (mod, sub);
		if (ret != 0)
			Con_Printf ("Error in execution of %s!\nError code: %i\n",
						Cmd_Argv (1), ret);
	}
}

void
GIB_Load_f (void)
{
	char        filename[256];
	VFile      *f;

	snprintf (filename, sizeof (filename), "%s/%s.gib", com_gamedir,
			  Cmd_Argv (1));
	f = Qopen (filename, "r");
	if (f) {
		GIB_Module_Load (Cmd_Argv (1), f);
		Qclose (f);
	} else
		Con_Printf ("gibload: File not found.\n");
}
