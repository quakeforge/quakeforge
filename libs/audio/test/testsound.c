/*
	testsound.c

	sound system test program

	Copyright (C) 2010 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2010/8/10

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

#include <stdlib.h>
#include <unistd.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/idparse.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "QF/sound.h"
#include "QF/scene/transform.h"

#ifdef _WIN32
# include "winquake.h"
HWND win_mainwindow;
#endif

#define MEMSIZE (32 * 1024 * 1024)

cbuf_t     *testsound_cbuf;
cbuf_args_t *testsound_args;


static void
init (void)
{
	testsound_cbuf = Cbuf_New (&id_interp);
	testsound_args = Cbuf_ArgsNew ();

	Sys_Init ();
	COM_ParseConfig (testsound_cbuf);
	cmd_warncmd = 1;

	memhunk_t *hunk = Memory_Init (Sys_Alloc (MEMSIZE), MEMSIZE);

	QFS_Init (hunk, "qw");
	PI_Init ();

	S_Init_Cvars ();
	S_Init (0, 0);

	Cmd_StuffCmds (testsound_cbuf);
}

int
main (int argc, const char *argv[])
{
	COM_InitArgv (argc, argv);
	init ();
	Cbuf_Execute_Stack (testsound_cbuf);
	while (1) {
		Cbuf_Execute_Stack (testsound_cbuf);

		S_Update (nulltransform, 0);
		usleep(20 * 1000);
	}
	Sys_Quit ();
}
