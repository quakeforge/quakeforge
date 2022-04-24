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

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "qw/include/server.h"

#ifdef NeXT
# include <libc.h>
#endif

server_static_t svs;

#ifdef __alpha__
static inline unsigned long
rdfpcr (void)
{
	unsigned long tmp, ret;

#if defined(__alpha_cix__) || defined(__alpha_fix__)
	__asm__ __volatile__ (
		"ftoit $f0,%0\n\t"
		"mf_fpcr $f0\n\t"
		"ftoit $f0,%1\n\t"
		"itoft %0,$f0"
		: "=r"(tmp), "=r"(ret));
#else
	__asm__ __volatile__ (
		"stt $f0,%0\n\t"
		"mf_fpcr $f0\n\t"
		"stt $f0,%1\n\t"
		"ldt $f0,%0"
		: "=m"(tmp), "=m"(ret));
#endif

	return ret;
}

static inline void
wrfpcr (unsigned long val)
{
	unsigned long tmp;

#if defined(__alpha_cix__) || defined(__alpha_fix__)
	__asm__ __volatile__ (
		"ftoit $f0,%0\n\t"
		"itoft %1,$f0\n\t"
		"mt_fpcr $f0\n\t"
		"itoft %0,$f0"
		: "=&r"(tmp) : "r"(val));
#else
	__asm__ __volatile__ (
		"stt $f0,%0\n\t"
		"ldt $f0,%1\n\t"
		"mt_fpcr $f0\n\t"
		"ldt $f0,%0"
		: "=m"(tmp) : "m"(val));
#endif
}
#endif

static void
startup (void)
{
#ifdef __alpha__
	wrfpcr (rdfpcr () | 1L << 47);
#endif
}

int
main (int argc, const char **argv)
{
	double      time, oldtime, newtime;

	startup ();

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (argc, argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	SV_Init ();

	Sys_RegisterShutdown (Net_LogStop, 0);

	// run one frame immediately for first heartbeat
	SV_Frame (0.1);

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {							// Main message loop
		Sys_CheckInput (!svs.num_clients, net_socket);

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		SV_Frame (time);

		// extrasleep is just a way to generate a bad connection on purpose
		if (sys_extrasleep)
			usleep (sys_extrasleep);
	}
}
