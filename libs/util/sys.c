/*
	sys.c

	virtual filesystem functions

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
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "QF/cvar.h"
#include "QF/sys.h"

#include "compat.h"

static void Sys_StdPrintf (const char *fmt, va_list args);

cvar_t     *sys_nostdout;
cvar_t     *sys_extrasleep;
cvar_t     *sys_dead_sleep;
cvar_t     *sys_sleep;

static sys_printf_t sys_printf_function = Sys_StdPrintf;

typedef struct shutdown_list_s {
	struct shutdown_list_s *next;
	void (*func)(void);
} shutdown_list_t;

static shutdown_list_t *shutdown_list;

/* The translation table between the graphical font and plain ASCII  --KB */
const char sys_char_map[256] = {
	'\0', '#', '#', '#', '#', '.', '#', '#',
	'#', 9, 10, '#', ' ', 13, '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

#define MAXPRINTMSG 4096


void
Sys_mkdir (const char *path)
{
#ifdef HAVE_MKDIR
# ifdef _WIN32
	if (mkdir (path) == 0)
		return;
# else
	if (mkdir (path, 0777) == 0)
		return;
# endif
#else
# ifdef HAVE__MKDIR
	if (_mkdir (path) == 0)
		return;
# else
#  error do not know how to make directories
# endif
#endif
	if (errno != EEXIST)
		Sys_Error ("mkdir %s: %s", path, strerror (errno));
}

int
Sys_FileTime (const char *path)
{
#ifdef HAVE_ACCESS
	if (access (path, R_OK) == 0)
		return 0;
#else
# ifdef HAVE__ACCESS
	if (_access (path, R_OK) == 0)
		return 0;
# else
#  error do not know how to check access
# endif
#endif
	return -1;
}

/*
	Sys_SetPrintf

	for want of a better name, but it sets the function pointer for the
	actual implementation of Sys_Printf.
*/
void
Sys_SetPrintf (sys_printf_t func)
{
	sys_printf_function = func;
}

static void
Sys_StdPrintf (const char *fmt, va_list args)
{
	char        msg[MAXPRINTMSG];

	unsigned char *p;

	if (sys_nostdout && sys_nostdout->int_val)
		return;

	vsnprintf (msg, sizeof (msg), fmt, args);

	/* translate to ASCII instead of printing [xx]  --KB */
	for (p = (unsigned char *) msg; *p; p++)
		putc (sys_char_map[*p], stdout);

	fflush (stdout);
}

void
Sys_Printf (const char *fmt, ...)
{
	va_list     args;
	va_start (args, fmt);
	sys_printf_function (fmt, args);
	va_end (args);
}

void
Sys_DPrintf (const char *fmt, ...)
{
	va_list     args;

	if (!developer || !developer->int_val)
		return;
	va_start (args, fmt);
	sys_printf_function (fmt, args);
	va_end (args);
}

double
Sys_DoubleTime (void)
{
	static qboolean first = true;
#ifdef _WIN32
	static DWORD starttime;
	DWORD       now;

	now = timeGetTime ();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime)				// wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
#else
	struct timeval tp;
	struct timezone tzp;
	double now;
	static double start_time;

	gettimeofday (&tp, &tzp);
	now = tp.tv_sec + tp.tv_usec / 1e6;

	if (first) {
		first = false;
		start_time = now;
	}

	return now - start_time;
#endif
}

void
Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
#ifdef _WIN32
	DWORD       flOldProtect;

	if (!VirtualProtect
		((LPVOID) startaddr, length, PAGE_READWRITE,
		 &flOldProtect)) Sys_Error ("Protection change failed\n");
#else
# ifdef HAVE_MPROTECT
	int         r;
	unsigned long addr;
	int         psize = getpagesize ();

	addr = (startaddr & ~(psize - 1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect ((char *) addr, length + startaddr - addr + psize, 7);

	if (r < 0)
		Sys_Error ("Protection change failed\n");
# endif
#endif
}

void
Sys_Init_Cvars (void)
{
	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL,
							 "Set to disable std out");
	sys_extrasleep = Cvar_Get ("sys_extrasleep", "0", CVAR_NONE, NULL,
							   "Set to cause whatever amount delay in "
							   "microseconds you want. Mostly "
							   "useful to generate simulated bad "
							   "connections.");
	sys_dead_sleep = Cvar_Get ("sys_dead_sleep", "1", CVAR_NONE, NULL,
							   "When set, the server gets NO cpu if no "
							   "clients are connected and there's no other "
							   "activity. *MIGHT* cause problems with some "
							   "mods.");
	sys_sleep = Cvar_Get ("sys_sleep", "8", CVAR_NONE, NULL, "Sleep how long "
						  "in seconds between checking for connections. "
						  "Minimum is 0, maximum is 13");
}

static void
run_shutdown_list (void)
{
	shutdown_list_t *p = shutdown_list;

	while (p) {
		p->func ();
		p = p->next;
	}
}

void
Sys_Quit (void)
{
	run_shutdown_list ();

	exit (0);
}

void
Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

#ifdef WIN32
	MessageBox (NULL, text, "Error", 0 /* MB_OK */ );
#endif
	fprintf (stderr, "Fatal error: %s\n", string);

	run_shutdown_list ();

	exit (1);
}

void
Sys_RegisterShutdown (void (*func) (void))
{
	shutdown_list_t *p;
	if (!func)
		return;
	p = malloc (sizeof (*p));
	if (!p)
		Sys_Error ("Sys_RegisterShutdown: insufficient memory");
	p->func = func;
	p->next = shutdown_list;
	shutdown_list = p;
}
