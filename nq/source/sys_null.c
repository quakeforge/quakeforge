/*
	sys_null.c

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

#include "errno.h"

#define MAX_HANDLES             10

VFile      *sys_handles[MAX_HANDLES];


// VFile IO ===================================================================

int
findhandle (void)
{
	int         i;

	for (i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

int
filelength (VFile *f)
{
	int         pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int
Sys_FileOpenRead (char *path, int *hndl)
{
	VFile      *f;
	int         i;

	i = findhandle ();

	f = Qopen (path, "rb");
	if (!f) {
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;

	return filelength (f);
}

int
Sys_FileOpenWrite (char *path)
{
	VFile      *f;
	int         i;

	i = findhandle ();

	f = Qopen (path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path, strerror (errno));
	sys_handles[i] = f;

	return i;
}

void
Sys_FileClose (int handle)
{
	Qclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void
Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int
Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int
Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int
Sys_FileTime (char *path)
{
	VFile      *f;

	f = Qopen (path, "rb");
	if (f) {
		Qclose (f);
		return 1;
	}

	return -1;
}

void
Sys_mkdir (char *path)
{
}

// SYSTEM IO ==================================================================

void
Sys_Error (char *error, ...)
{
	va_list     argptr;

	printf ("Sys_Error: ");
	va_start (argptr, error);
	vprintf (error, argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;

	va_start (argptr, fmt);
	vprintf (fmt, argptr);
	va_end (argptr);
}

void
Sys_Quit (void)
{
	exit (0);
}

double
Sys_DoubleTime (void)
{
	static double t;

	t += 0.1;

	return t;
}

char       *
Sys_ConsoleInput (void)
{
	return NULL;
}

void
Sys_Sleep (void)
{
}

void
IN_SendKeyEvents (void)
{
}

void
Sys_HighFPPrecision (void)
{
}

void
Sys_LowFPPrecision (void)
{
}

//=============================================================================

void
main (int argc, char **argv)
{
	static quakeparms_t parms;

	parms.memsize = 8 * 1024 * 1024;
	parms.membase = malloc (parms.memsize);
	parms.basedir = ".";

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	printf ("Host_Init\n");
	Host_Init (&parms);
	while (1) {
		Host_Frame (0.1);
	}
}
