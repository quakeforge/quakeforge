/*
	sv_sys_unix.c

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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "server.h"
#include "QF/sys.h"

#ifdef NeXT
# include <libc.h>
#endif

cvar_t     *sys_extrasleep;
cvar_t     *sys_dead_sleep;

qboolean    is_server = true;
qboolean    stdin_ready;
server_static_t svs;
char       *svs_info = svs.info;


/*
				REQUIRED SYS FUNCTIONS
*/

/*
	Sys_Error
*/
void
Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	printf ("Fatal error: %s\n", string);

	exit (1);
}


/*
	Sys_Quit
*/
void
Sys_Quit (void)
{

	Net_LogStop();

	exit (0);							// appkit isn't running
}

static int  do_stdin = 1;

/*
	Sys_ConsoleInput

	Checks for a complete line of text typed in at the console, then forwards
	it to the host command processor
*/
char       *
Sys_ConsoleInput (void)
{
	static char text[256];
	int         len;

	if (!stdin_ready || !do_stdin)
		return NULL;					// the select didn't say it was ready
	stdin_ready = false;

	len = read (0, text, sizeof (text));
	if (len == 0) {
		// end of file
		do_stdin = 0;
		return NULL;
	}
	if (len < 1)
		return NULL;
	text[len - 1] = 0;					// rip off the /n and terminate

	return text;
}

/*
	Sys_Init

	Quake calls this so the system can register variables before host_hunklevel
	is marked
*/
void
Sys_Init_Cvars (void)
{
	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL,
			"Toggles console screen output");
	sys_extrasleep = Cvar_Get ("sys_extrasleep", "0", CVAR_NONE, NULL, 
		"Set to cause whatever amount delay in microseconds you want. Mostly useful to generate simulated bad connections.");
	sys_dead_sleep = Cvar_Get ("sys_dead_sleep", "1", CVAR_NONE, NULL,
		"When set, the server gets NO cpu if no clients are connected"
		"and there's no other activity. *MIGHT* cause problems with"
		"some mods.");
}

void
Sys_Init (void)
{
#ifdef USE_INTEL_ASM
	Sys_SetFPCW ();
#endif
}

/*
	main
*/
int
main (int argc, char *argv[])
{
	double      time, oldtime, newtime;
	fd_set      fdset;
	extern int  net_socket;
	int         j;

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (argc, argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	host_parms.memsize = 8 * 1024 * 1024;

	j = COM_CheckParm ("-mem");
	if (j)
		host_parms.memsize = (int) (atof (com_argv[j + 1]) * 1024 * 1024);
	if ((host_parms.membase = malloc (host_parms.memsize)) == NULL)
		Sys_Error ("Can't allocate %d\n", host_parms.memsize);

	SV_Init ();

	// run one frame immediately for first heartbeat
	SV_Frame (0.1);

	// 
	// main loop
	// 
	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {
		struct timeval _timeout;
		struct timeval *timeout = 0;
		// select on the net socket and stdin
		// the only reason we have a timeout at all is so that if the last
		// connected client times out, the message would not otherwise
		// be printed until the next event.
		FD_ZERO (&fdset);
		if (do_stdin)
			FD_SET (0, &fdset);
		FD_SET (net_socket, &fdset);

		_timeout.tv_sec = 1;
		_timeout.tv_usec = 0;
		if (svs.num_clients || !sys_dead_sleep->int_val)
			timeout = &_timeout;

		if (select (net_socket + 1, &fdset, NULL, NULL, timeout) == -1)
			continue;
		stdin_ready = FD_ISSET (0, &fdset);

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		SV_Frame (time);

		// extrasleep is just a way to generate a fucked up connection on
		// purpose
		if (sys_extrasleep->int_val)
			usleep (sys_extrasleep->int_val);
	}
	return 1;
}
