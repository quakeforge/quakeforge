/*
	sys_sdl.c

	(description)

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/fcntl.h>
#endif

#include <SDL.h>
#include <SDL_main.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "nq/include/client.h"
#include "nq/include/host.h"

#ifdef _WIN32
# include "winquake.h"
#endif

int qf_sdl_link;
qboolean    isDedicated = false;

static void
startup (void)
{
#ifdef _WIN32
	OSVERSIONINFO vinfo;
	Sys_MaskFPUExceptions ();

	// make sure the timer is high precision, otherwise NT gets 18ms resolution
	timeBeginPeriod (1);

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4)
		|| (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)) {
		Sys_Error ("This version of " PACKAGE_NAME
				   " requires at least Win95 or NT 4.0");
	}
#endif
}

static void
shutdown_f (void *data)
{
#ifndef _WIN32
	// change stdin to blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
	fflush (stdout);
#endif
}

#ifndef SDL_main
# define SDL_main main
#endif

int
SDL_main (int argc, char *argv[])
{
	double      time, oldtime, newtime;

	startup ();

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (argc, (const char **) argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

	Sys_RegisterShutdown (Host_Shutdown, 0);
	Sys_RegisterShutdown (shutdown_f, 0);

	Host_Init ();

#ifndef _WIN32
	if (!sys_nostdout) {
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | O_NONBLOCK);
		Sys_Printf ("Quake -- Version %s\n", NQ_VERSION);
	}
#else
	// hack to prevent gcc suggesting noreturn
	if (!sys_nostdout) {
		return 1;
	}
#endif

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {							// Main message loop
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
