/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "QF/cbuf.h"
#include "QF/cdaudio.h"
#include "QF/console.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/model.h"
#include "QF/plugin.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/zone.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "qwaq.h"

CLIENT_PLUGIN_PROTOS
static plugin_list_t client_plugin_list[] = {
	CLIENT_PLUGIN_LIST
};

double      con_frametime;
double      con_realtime, basetime;
double      old_conrealtime;


static void
quit_f (void)
{
	if (!con_module)
		Sys_Printf ("I hope you wanted to quit\n");
	Sys_Quit ();
}

static void
bi_printf (progs_t *pr)
{
	const char *fmt = P_GSTRING (pr, 0);
	int         count = pr->pr_argc - 1;
	pr_type_t **args = pr->pr_params + 1;
	dstring_t  *dstr = dstring_new ();

	PR_Sprintf (pr, dstr, "bi_printf", fmt, count, args);
	if (dstr->str)
		Con_Printf (dstr->str, stdout);
	dstring_delete (dstr);
}

static progs_t *bi_rprogs;
static func_t qc3d, qc2d;

static void
bi_3d (void)
{
	if (qc3d)
		PR_ExecuteProgram (bi_rprogs, qc3d);
}

static void
bi_2d (void)
{
	if (qc2d)
		PR_ExecuteProgram (bi_rprogs, qc2d);
}

static SCR_Func bi_2dfuncs[] = {
	bi_2d,
	Con_DrawConsole,
	0,
};

static void
bi_refresh (progs_t *pr)
{
	con_realtime = Sys_DoubleTime () - basetime;
	con_frametime = con_realtime - old_conrealtime;
	old_conrealtime = con_realtime;
	bi_rprogs = pr;
	IN_ProcessEvents ();
	//GIB_Thread_Execute ();
	Cbuf_Execute_Stack (qwaq_cbuf);
	r_funcs->SCR_UpdateScreen (con_realtime, bi_3d, bi_2dfuncs);
	R_FLOAT (pr) = con_frametime;
}

static void
bi_refresh_2d (progs_t *pr)
{
	qc2d = P_FUNCTION (pr, 0);
}

static void
bi_refresh_3d (progs_t *pr)
{
	qc3d = P_FUNCTION (pr, 0);
}

static void
bi_shutdown_ (progs_t *pr)
{
	Sys_Shutdown ();
}

static builtin_t builtins[] = {
	{"printf",		bi_printf,		-1},
	{"refresh",		bi_refresh,		-1},
	{"refresh_2d",	bi_refresh_2d,	-1},
	{"refresh_3d",	bi_refresh_3d,	-1},
	{"shutdown",	bi_shutdown_,	-1},
	{0}
};

static void
bi_shutdown (void)
{
	S_Shutdown ();
	IN_Shutdown ();
	VID_Shutdown ();
}

void
BI_Init (progs_t *pr)
{
	byte       *basepal, *colormap;

	PR_RegisterBuiltins (pr, builtins);

	QFS_Init ("nq");
	PI_Init ();
	PI_RegisterPlugins (client_plugin_list);

	Sys_RegisterShutdown (bi_shutdown);

	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	basepal = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/palette.lmp"));
	if (!basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	colormap = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/colormap.lmp"));
	if (!colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");

	W_LoadWadFile ("gfx.wad");
	VID_Init (basepal, colormap);
	IN_Init (qwaq_cbuf);
	Mod_Init ();
	R_Init ();
	R_Progs_Init (pr);
	Key_Progs_Init (pr);
	S_Progs_Init (pr);

	Con_Init ("client");
	if (con_module) {
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = quit_f;
		con_module->data->console->cbuf = qwaq_cbuf;
	}
	Key_SetKeyDest (key_game);

	S_Init (0, &con_frametime);
	//CDAudio_Init ();
	Con_NewMap ();
	basetime = Sys_DoubleTime ();
}
