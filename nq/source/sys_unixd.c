/*
	sys_unixd.c

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
static const char rcsid[] = 
	"$Id$";

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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "client.h"
#include "host.h"

qboolean    isDedicated = true;

int         nostdout = 0;

void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	va_list     argptr;
	static char data[1024];
	int         fd;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof (data), fmt, argptr);
	va_end (argptr);

	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write (fd, data, strlen (data));
	close (fd);
}

static void
shutdown (void)
{
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	fflush (stdout);
}

void
Sys_Init (void)
{
#ifdef USE_INTEL_ASM
	Sys_SetFPCW ();
#endif
}

const char *
Sys_ConsoleInput (void)
{
	static char text[256];
	int         len;
	fd_set      fdset;
	struct timeval timeout;

	if (cls.state == ca_dedicated) {
		FD_ZERO (&fdset);
		FD_SET (0, &fdset);				// stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select (1, &fdset, NULL, NULL, &timeout) == -1
			|| !FD_ISSET (0, &fdset)) return NULL;

		len = read (0, text, sizeof (text));
		if (len < 1)
			return NULL;
		text[len - 1] = 0;				// rip off the /n and terminate

		return text;
	}
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

int
main (int argc, const char **argv)
{
	double      time, oldtime, newtime;
	quakeparms_t parms;
	const char *newargv[256];
	int         j;

	signal (SIGFPE, SIG_IGN);

	memset (&parms, 0, sizeof (parms));

	COM_InitArgv (argc, argv);

	// dedicated server ONLY!
	if (!COM_CheckParm ("-dedicated")) {
		memcpy (newargv, argv, argc * 4);
		newargv[argc] = "-dedicated";
		argc++;
		argv = newargv;
		COM_InitArgv (argc, argv);
	}

	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 16 * 1024 * 1024;

	j = COM_CheckParm ("-mem");
	if (j)
		parms.memsize = (int) (atof (com_argv[j + 1]) * 1024 * 1024);
	if ((parms.membase = malloc (parms.memsize)) == NULL)
		Sys_Error ("Can't allocate %d\n", parms.memsize);

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	Sys_RegisterShutdown (Host_Shutdown);
	Sys_RegisterShutdown (shutdown);

	Sys_Printf ("Host_Init\n");
	Host_Init (&parms);

	Sys_Init_Cvars ();
	Sys_Init ();

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {							// Main message loop
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		if (time < sys_ticrate->value) {
			usleep (1);
			continue;
		}
		time = sys_ticrate->value;

		if (time > sys_ticrate->value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame (time);
	}
	return true;						// return success
}
