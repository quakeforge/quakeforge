/*
	cl_sys_sdl.c

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_CONIO_H
# include <conio.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
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

#ifndef _WIN32
# include <stdarg.h>
# include <ctype.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/mman.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_main.h>

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/qargs.h"

#include "client.h"
#include "compat.h"
#include "host.h"

qboolean    is_server = false;
char       *svs_info;

int         starttime;
int         noconinput;

#ifdef _WIN32
# include "winquake.h"
// FIXME: minimized is not currently supported under SDL
qboolean    Minimized = false;
void        MaskExceptions (void);
#endif


/*
	Sys_Init_Cvars

	Quake calls this so the system can register variables before host_hunklevel
	is marked
*/
void
Sys_Init_Cvars (void)
{
	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL,
							 "set to disable std out");
	if (COM_CheckParm ("-nostdout"))
		Cvar_Set (sys_nostdout, "1");
}

void
Sys_Init (void)
{
#ifdef WIN32
	OSVERSIONINFO vinfo;
#endif
#ifdef USE_INTEL_ASM
#ifdef _WIN32
	MaskExceptions ();
#endif
	Sys_SetFPCW ();
#endif

#ifdef _WIN32
	// make sure the timer is high precision, otherwise NT gets 18ms resolution
	timeBeginPeriod (1);

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4)
		|| (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)) {
		Sys_Error ("This version of " PROGRAM
				   " requires at least Win95 or NT 4.0");
	}
#endif
}

void
Sys_Quit (void)
{
	Host_Shutdown ();
	exit (0);
}

void
Sys_Error (const char *error, ...)
{
	char        text[1024];
	va_list     argptr;

#ifndef _WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif
	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

#ifdef WIN32
	MessageBox (NULL, text, "Error", 0 /* MB_OK */ );
#endif
	fprintf (stderr, "Error: %s\n", text);

	Host_Shutdown ();
	exit (1);
}



void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	int         fd;
	static char data[1024];				// why static ?
	va_list     argptr;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof (data), fmt, argptr);
	va_end (argptr);
	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write (fd, data, strlen (data));
	close (fd);
};

/*
	Sys_ConsoleInput

	Checks for a complete line of text typed in at the console, then forwards
	it to the host command processor
*/
const char *
Sys_ConsoleInput (void)
{
	return NULL;
}

#ifndef USE_INTEL_ASM
void
Sys_HighFPPrecision (void)
{
}

void
Sys_LowFPPrecision (void)
{
}
#endif

void
Sys_Sleep (void)
{
}

#ifndef SDL_main
# define SDL_main main
#endif

int
SDL_main (int c, char **v)
{
	int         j;
	double      time, oldtime, newtime;

#ifndef WIN32
	signal (SIGFPE, SIG_IGN);
#endif

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (c, (const char **)v);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	host_parms.memsize = 16 * 1024 * 1024;  // 16MB default heap

	j = COM_CheckParm ("-mem");
	if (j)
		host_parms.memsize = (int) (atof (com_argv[j + 1]) * 1024 * 1024);
	host_parms.membase = malloc (host_parms.memsize);

	if (!host_parms.membase) {
		printf ("Can't allocate memory for zone.\n");
		return 1;
	}

#ifndef WIN32
	noconinput = COM_CheckParm ("-noconinput");
	if (!noconinput)
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | O_NONBLOCK);
#endif

	Host_Init ();

	oldtime = Sys_DoubleTime ();
	while (1) {
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);
		oldtime = newtime;
	}
}
