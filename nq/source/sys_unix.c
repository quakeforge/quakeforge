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

	$Id$
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>

#include "QF/sys.h"
#include "QF/qargs.h"
#include "QF/cvar.h"
#include "server.h"
#include "host.h"



qboolean    isDedicated;

char       *basedir = ".";
char       *cachedir = "/tmp";

cvar_t     *sys_linerefresh;
cvar_t     *timestamps;
cvar_t     *timeformat;

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


/*
 *	System I/O
 */


void
Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	fprintf (stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);

}

void
Sys_Quit (void)
{
	Host_Shutdown ();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	fflush (stdout);
	exit (0);
}

void
Sys_Init (void)
{
#ifdef USE_INTEL_ASM
	Sys_SetFPCW ();
#endif
}


void
floating_point_exception_handler (int whatever)
{
//  Sys_Warn("floating point exception\n");
	signal (SIGFPE, floating_point_exception_handler);
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
main (int c, const char *v[])
{

	double      time, oldtime, newtime;
	quakeparms_t parms;
	extern int  vcrFile;
	extern int  recording;
	int         j;

	signal (SIGFPE, SIG_IGN);

	memset (&parms, 0, sizeof (parms));

	COM_InitArgv (c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 16 * 1024 * 1024;

	j = COM_CheckParm ("-mem");
	if (j)
		parms.memsize = (int) (atof (com_argv[j + 1]) * 1024 * 1024);
	parms.membase = malloc (parms.memsize);

	parms.basedir = basedir;
// caching is disabled by default, use -cachedir to enable
//  parms.cachedir = cachedir;

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	Host_Init (&parms);

	Sys_Init ();

	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL,
			"set to disable std out");
	if (COM_CheckParm ("-nostdout"))
		Cvar_Set (sys_nostdout, "1");
	else {
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
		printf ("Quake -- Version %s\n", NQ_VERSION);
	}

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {
// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated) {	// play vcrfiles at max speed
			if (time < sys_ticrate->value && (vcrFile == -1 || recording)) {
				usleep (1);
				continue;				// not time to run a server only tic
				// yet
			}
			time = sys_ticrate->value;
		}

		if (time > sys_ticrate->value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame (time);

	}

}

