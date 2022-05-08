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
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/ruamoko.h"
#include "QF/screen.h"
#include "QF/sound.h"

#include "QF/input/event.h"
#include "QF/math/bitop.h"

#include "QF/plugin/console.h"

#include "rua_internal.h"

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
bi_newscene (progs_t *pr, void *_res)
{
	pr_ulong_t  scene_id = P_ULONG (pr, 0);
	SCR_NewScene (Scene_GetScene (pr, scene_id));
}

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
	SCR_UpdateScreen (0, con_realtime, bi_2dfuncs);
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
	bi(newscene,   -1, 1, p(long)),
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

static byte default_palette[256][3];

static byte *
write_raw_rgb (byte *dst, byte r, byte g, byte b)
{
	*dst++ = r;
	*dst++ = g;
	*dst++ = b;
	return dst;
}

static byte *
write_rgb (byte *dst, byte r, byte g, byte b)
{
#define shift(x) (((x) << 2) | (((x) & 0x3f) >> 4))
	*dst++ = shift(r);
	*dst++ = shift(g);
	*dst++ = shift(b);
	return dst;
#undef shift
}

static byte *
write_grey (byte *dst, byte grey)
{
	return write_rgb (dst, grey, grey, grey);
}

static byte *
genererate_irgb (byte *dst, byte lo, byte melo, byte mehi, byte hi)
{
	for (int i = 0; i < 16; i++) {
		byte        l = i & 8 ? melo : lo;
		byte        h = i & 8 ? hi : mehi;
		byte        r = i & 4 ? h : l;
		byte        g = i & 2 ? h : l;
		byte        b = i & 1 ? h : l;
		if (i == 6) {	// make dim yellow brown
			g = melo;
		}
		dst = write_rgb (dst, r, g, b);
	}
	return dst;
}

static byte *
write_run (byte *dst, byte *hue, byte ch, const byte *levels)
{
	byte        rgb[3] = {
		*hue & 4 ? levels[4] : levels[0],
		*hue & 2 ? levels[4] : levels[0],
		*hue & 1 ? levels[4] : levels[0],
	};
	byte        chan = 2 - BITOP_LOG2 (ch);
	int         ind = *hue & ch ? 4 : 0;
	int         dir = *hue & ch ? -1 : 1;

	for (int i = 0; i < 4; i++, ind += dir) {
		rgb[chan] = levels[ind];
		dst = write_rgb (dst, rgb[0], rgb[1], rgb[2]);
	}

	*hue ^= ch;
	return dst;
}

static byte *
write_cycle (byte *dst, byte lo, byte melo, byte me, byte mehi, byte hi)
{
	const byte  levels[] = { lo, melo, me, mehi, hi };
	byte        hue = 1;
	dst = write_run (dst, &hue, 4, levels);
	dst = write_run (dst, &hue, 1, levels);
	dst = write_run (dst, &hue, 2, levels);

	dst = write_run (dst, &hue, 4, levels);
	dst = write_run (dst, &hue, 1, levels);
	dst = write_run (dst, &hue, 2, levels);
	return dst;
}

static void
generate_palette (void)
{
	const byte  grey[] = {
		0,  5,  8,  11, 14, 17, 20, 24,
		28, 32, 36, 40, 45, 50, 56, 63,
	};
	byte       *dst = default_palette[0];

	dst = genererate_irgb (dst, 0, 21, 42, 63);

	for (int i = 0; i < 16; i++) {
		dst = write_grey (dst, grey[i]);
	}

	dst = write_cycle (dst, 11, 12, 13, 15, 16);
	dst = write_cycle (dst,  8, 10, 12, 14, 16);
	dst = write_cycle (dst,  0,  4,  8, 12, 16);

	dst = write_cycle (dst, 20, 22, 24, 26, 28);
	dst = write_cycle (dst, 14, 17, 21, 24, 28);
	dst = write_cycle (dst,  0,  7, 14, 21, 28);

	dst = write_cycle (dst, 45, 49, 54, 58, 63);
	dst = write_cycle (dst, 31, 39, 47, 55, 63);
	dst = write_cycle (dst,  0, 16, 31, 47, 63);

	// std is black, but want vis trans and some phosphors
	dst = write_raw_rgb (dst,  40,  40,  40);
	dst = write_raw_rgb (dst,  40, 127,  40);
	dst = write_raw_rgb (dst,  51, 255,  51);
	dst = write_raw_rgb (dst, 102, 255, 102);
	dst = write_raw_rgb (dst, 127, 102,   0);
	dst = write_raw_rgb (dst, 255, 176,   0);
	dst = write_raw_rgb (dst, 255, 204,   0);

	dst = write_raw_rgb (dst, 176, 160, 176);
}

static byte default_colormap[64 * 256 + 1] = { [64 * 256] = 32 };

static void
generate_colormap (void)
{
	byte        colors[64][256][3];
	tex_t       tex = {
		.width = 256,
		.height = 64,
		.format = tex_rgb,
		.data = colors[0][0],
	};

	// baseline colors
	for (int i = 0; i < 256; i++) {
		VectorCopy (default_palette[i], colors[31][i]);
		VectorCopy (default_palette[i], colors[32][i]);
	}
	for (int i = 0; i < 64; i++) {
		// main colors
		if (i < 31 || i > 32) {
			float       scale = i < 32 ? 2 - i / 31.0 : 1 - (i - 32) / 31.0;
			for (int j = 0; j < 224; j++) {
				for (int k = 0; k < 3; k++) {
					colors[i][j][k] = bound (0, scale * colors[31][j][k], 255);
				}
			}
		}
		// fullbrights
		memcpy (colors[i][224], colors[31][224], 32 * 3);
	}
	tex_t     *cmap = ConvertImage (&tex, default_palette[0]);
	memcpy (default_colormap, cmap->data, sizeof (default_colormap));
	free (cmap);
}

void
BI_Graphics_Init (progs_t *pr)
{
	qwaq_thread_t *thread = PR_Resources_Find (pr, "qwaq_thread");

	PR_RegisterBuiltins (pr, builtins, 0);

	QFS_Init (thread->hunk, "nq");
	PI_Init ();
	PI_RegisterPlugins (client_plugin_list);

	Sys_RegisterShutdown (BI_shutdown, pr);

	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	generate_palette ();
	generate_colormap ();

	W_LoadWadFile ("gfx.wad");
	VID_Init (default_palette[0], default_colormap);
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
