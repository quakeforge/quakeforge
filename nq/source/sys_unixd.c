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

#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "client.h"
#include "host.h"

qboolean    isDedicated;

int         nostdout = 0;

cvar_t     *sys_linerefresh;
cvar_t     *timestamps;
cvar_t     *timeformat;


int
Sys_FileOpenRead (char *path, int *handle)
{
	struct stat fileinfo;
	int         h;

	h = open (path, O_RDONLY, 0666);
	*handle = h;
	if (h == -1)
		return -1;

	if (fstat (h, &fileinfo) == -1)
		Sys_Error ("Error fstating %s", path);

	return fileinfo.st_size;
}

int
Sys_FileOpenWrite (char *path)
{
	int         handle;

	umask (0);

	handle = open (path, O_RDWR | O_CREAT | O_TRUNC, 0666);

	if (handle == -1)
		Sys_Error ("Error opening %s: %s", path, strerror (errno));

	return handle;
}

int
Sys_FileWrite (int handle, void *src, int count)
{
	return write (handle, src, count);
}

void
Sys_FileClose (int handle)
{
	close (handle);
}

void
Sys_FileSeek (int handle, int position)
{
	lseek (handle, position, SEEK_SET);
}

int
Sys_FileRead (int handle, void *dest, int count)
{
	return read (handle, dest, count);
}

void
Sys_DebugLog (char *file, char *fmt, ...)
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

#define MAX_PRINT_MSG	4096
void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        start[MAX_PRINT_MSG];	// String we started with
	char        stamp[MAX_PRINT_MSG];	// Time stamp
	char        final[MAX_PRINT_MSG];	// String we print

	time_t      mytime = 0;
	struct tm  *local = NULL;

	unsigned char *p;

	va_start (argptr, fmt);
	vsnprintf (start, sizeof (start), fmt, argptr);
	va_end (argptr);

	if (nostdout)
		return;

	if (timestamps && timeformat && timestamps && timeformat
		&& timeformat->string && timestamps->int_val) {
		mytime = time (NULL);
		local = localtime (&mytime);
		strftime (stamp, sizeof (stamp), timeformat->string, local);

		snprintf (final, sizeof (final), "%s%s", stamp, start);
	} else {
		snprintf (final, sizeof (final), "%s", start);
	}

	for (p = (unsigned char *) final; *p; p++) {
		putc (qfont_table[*p], stdout);
	}
	fflush (stdout);
}

void
Sys_Error (char *error, ...)
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

double
Sys_DoubleTime (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int  secbase;

	gettimeofday (&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

char *
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
main (int argc, char *argv[])
{
	double      time, oldtime;
	quakeparms_t parms;
	char       *newargv[256];
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
	if (j) {
		parms.memsize = (int) (atof (com_argv[j + 1]) * 1024 * 1024);
	}
	if ((parms.membase = malloc (parms.memsize)) == NULL)
		Sys_Error ("Can't allocate %d\n", parms.memsize);

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	printf ("Host_Init\n");
	Host_Init (&parms);

	oldtime = Sys_DoubleTime () - 0.1;

	while (1) {							// Main message loop
		time = Sys_DoubleTime ();
		if ((time - oldtime) < sys_ticrate->value) {
			usleep (1);
			continue;
		}
		Host_Frame (time - oldtime);
		oldtime = time;
	}
	return true;						// return success
}
