/*
	sys_wind.c

	@description@

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#include <sys/types.h>
#include <sys/timeb.h>
#include <conio.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "game.h"
#include "host.h"
#include "winquake.h"


qboolean	isDedicated = true;

static void
shutdown (void)
{
}

int
main (int argc, const char **argv)
{
	double      time, oldtime;
	int         i;

	memset (&host_parms, 0, sizeof (host_parms));

#if 0
	_getcwd (cwd, sizeof (cwd));
	if (cwd[Q_strlen (cwd) - 1] == '\\')
		cwd[Q_strlen (cwd) - 1] = 0;
#endif
	// dedicated server ONLY!
	for (i = 1; i < argc; i++)
		if (!strcmp (argv[i], "-dedicated"))
			break;
	if (i == argc) {
		const char **newargv;

		newargv = malloc ((argc + 2) * sizeof (*newargv));
		memcpy (newargv, argv, argc * 4);
		newargv[argc++] = "-dedicated";
		newargv[argc] = 0;
		argv = newargv;
	}

	COM_InitArgv (argc, argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	Sys_RegisterShutdown (Host_Shutdown);
	Sys_RegisterShutdown (shutdown);

	Host_Init ();

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1) {
		time = Sys_DoubleTime ();
		if (time - oldtime < sys_ticrate->value) {
			Sleep (1);
			continue;
		}

		Host_Frame (time - oldtime);
		oldtime = time;
	}

	/* return success of application */
	return TRUE;
}
