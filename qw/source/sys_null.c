/*
	sys_null.c

	null system driver to aid porting efforts

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

#include <errno.h>


/*
	filelength
*/
int
filelength (QFile *f)
{
	int         pos;
	int         end;

	pos = Qtell (f);
	Qseek (f, 0, SEEK_END);
	end = Qtell (f);
	Qseek (f, pos, SEEK_SET);

	return end;
}


int
Sys_FileTime (char *path)
{
	QFile      *f;

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


/*
	SYSTEM IO
*/

void
Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void
Sys_DebugLog (char *file, char *fmt, ...)
{
}

void
Sys_Error (char *error, ...)
{
	va_list     argptr;

	printf ("I_Error: ");
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
Sys_FloatTime (void)
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
	host_parms.memsize = 5861376;
	host_parms.membase = malloc (host_parms.memsize);

	COM_InitArgv (argc, argv);

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	printf ("Host_Init\n");
	Host_Init ();
	while (1) {
		Host_Frame (0.1);
	}
}
