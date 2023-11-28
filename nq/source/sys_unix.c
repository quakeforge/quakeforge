/*
	sys_unix.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/fcntl.h>
#endif
#include <locale.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "nq/include/client.h"
#include "nq/include/host.h"

bool        isDedicated = false;

static void
shutdown_f (void *data)
{
	// change stdin to blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
	fflush (stdout);
}

int
main (int argc, const char **argv)
{
	double      time, oldtime, newtime;

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (argc, argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

	Sys_RegisterShutdown (shutdown_f, 0);

	Host_Init ();

	if (!sys_nostdout) {
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | O_NONBLOCK);
		Sys_Printf ("Quake -- Version %s\n", NQ_VERSION);
	}
	setlocale (LC_ALL, "");

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {							// Main message loop
		qfFrameMark;
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		if (net_is_dedicated) {	// play vcrfiles at max speed
			if (time < sys_ticrate && (!vcrFile || recording)) {
				usleep (1);
				continue;			// not time to run a server-only tic yet
			}
			time = sys_ticrate;
		}

		if (time > sys_ticrate * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame (time);
	}
}
