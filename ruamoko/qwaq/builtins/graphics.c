/*
	graphics.c

	Basic game engine builtins

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/7/24

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
#include "QF/ruamoko.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/input/event.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "ruamoko/qwaq/qwaq.h"

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

static progs_t *bi_rprogs;
static pr_func_t qc2d;
static int event_handler_id;

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
bi_refresh (progs_t *pr, void *_res)
{
	con_realtime = Sys_DoubleTime () - basetime;
	con_frametime = con_realtime - old_conrealtime;
	old_conrealtime = con_realtime;
	bi_rprogs = pr;
	IN_ProcessEvents ();
	//GIB_Thread_Execute ();
	Cbuf_Execute_Stack (qwaq_cbuf);
	SCR_UpdateScreen (con_realtime, bi_2dfuncs);
	R_FLOAT (pr) = con_frametime;
}

static void
bi_refresh_2d (progs_t *pr, void *_res)
{
	qc2d = P_FUNCTION (pr, 0);
}

static void
bi_shutdown (progs_t *pr, void *_res)
{
	Sys_Shutdown ();
}

#define bi(x,n,np,params...) {#x, bi_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(refresh,    -1, 0),
	bi(refresh_2d, -1, 1, p(func)),
	bi(shutdown,   -1, 0),
	{0}
};

static int
event_handler (const IE_event_t *ie_event, void *_pr)
{
	return IN_Binding_HandleEvent (ie_event);
}

static void
BI_shutdown (void *data)
{
}

void
BI_Graphics_Init (progs_t *pr)
{
	qwaq_thread_t *thread = PR_Resources_Find (pr, "qwaq_thread");
	byte       *basepal, *colormap;

	PR_RegisterBuiltins (pr, builtins, 0);

	QFS_Init (thread->hunk, "nq");
	PI_Init ();
	PI_RegisterPlugins (client_plugin_list);

	Sys_RegisterShutdown (BI_shutdown, pr);

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
	IN_Init ();
	Mod_Init ();
	R_Init ();
	R_Progs_Init (pr);
	RUA_Game_Init (pr, thread->rua_security);
	S_Progs_Init (pr);

	event_handler_id = IE_Add_Handler (event_handler, pr);
	IE_Set_Focus (event_handler_id);

	Con_Init ("client");
	if (con_module) {
		con_module->data->console->realtime = &con_realtime;
		con_module->data->console->frametime = &con_frametime;
		con_module->data->console->quit = quit_f;
		con_module->data->console->cbuf = qwaq_cbuf;
	}
	//Key_SetKeyDest (key_game);

	S_Init (0, &con_frametime);
	//CDAudio_Init ();
	Con_NewMap ();
	basetime = Sys_DoubleTime ();
}
