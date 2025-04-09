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

#include "QF/backtrace.h"
#include "QF/cbuf.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/ruamoko.h"
#include "QF/screen.h"
#include "QF/sound.h"

#include "QF/input/event.h"
#include "QF/math/bitop.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"
#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/text.h"

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

static byte default_palette[256][3];
static byte default_colormap[64 * 256 + 1] = { [64 * 256] = 32 };
static progs_t *bi_rprogs;
static pr_func_t qc2d;
static pr_func_t qcevent;
static pr_ptr_t  qcevent_data;
static int event_handler_id;
static canvas_system_t canvas_sys;
static view_t screen_view;
static uint32_t canvas;

static void
bi_2d (void)
{
	if (qc2d)
		PR_ExecuteProgram (bi_rprogs, qc2d);
	Con_DrawConsole ();
	Canvas_Draw (canvas_sys);
}

static SCR_Func bi_2dfuncs[] = {
	bi_2d,
	0,
};

static void
vidsize_listener (void *data, const viddef_t *vdef)
{
	Canvas_SetLen (canvas_sys, canvas,
				   (view_pos_t) { vdef->width, vdef->height });
}

static void
bi_init_graphics (progs_t *pr, void *_res)
{
	VID_Init (default_palette[0], default_colormap);
	IN_Init ();
	Mod_Init ();
	int plitem_id = P_INT (pr, 0);
	plitem_t *plitem = nullptr;
	if (plitem_id) {
		plitem = Plist_GetItem (pr, plitem_id);
	}
	R_Init (plitem);
	Font_Init ();

	Con_Load ("client");
	__auto_type reg = ECS_NewRegistry ("qwaq gr");
	Canvas_InitSys (&canvas_sys, reg);
	if (con_module) {
		__auto_type cd = con_module->data->console;
		cd->realtime = &con_realtime;
		cd->frametime = &con_frametime;
		cd->quit = quit_f;
		cd->cbuf = qwaq_cbuf;
		cd->component_base = ECS_RegisterComponents (reg, cd->components,
													 cd->num_components);
		cd->canvas_sys = &canvas_sys;
	}
	ECS_CreateComponentPools (reg);

	canvas = Canvas_New (canvas_sys);
	screen_view = Canvas_GetRootView (canvas_sys, canvas);
	View_SetPos (screen_view, 0, 0);
	View_SetLen (screen_view, viddef.width, viddef.height);
	View_SetGravity (screen_view, grav_northwest);
	View_SetVisible (screen_view, 1);

	VID_OnVidResize_AddListener (vidsize_listener, 0);

	//Key_SetKeyDest (key_game);
	Con_Init ();
	VID_SendSize ();

	S_Init (0, &con_frametime);
	//CDAudio_Init ();
	Con_NewMap ();
	basetime = Sys_DoubleTime ();
}

static void
bi_newscene (progs_t *pr, void *_res)
{
	pr_ulong_t  scene_id = P_ULONG (pr, 0);
	SCR_NewScene (Scene_GetScene (pr, scene_id));
}

static void
bi_refresh (progs_t *pr, void *_res)
{
	qfFrameMark;
	con_realtime = Sys_DoubleTime () - basetime;
	con_frametime = con_realtime - old_conrealtime;
	old_conrealtime = con_realtime;
	bi_rprogs = pr;
	IN_ProcessEvents ();
	//GIB_Thread_Execute ();
	Cbuf_Execute_Stack (qwaq_cbuf);
	auto scene = Scene_GetScene (pr, P_ULONG (pr, 0));
	transform_t camera = nulltransform;
	if (scene && scene->camera != nullent) {
		entity_t ent = { .reg = scene->reg, .id = scene->camera,
						 .base = scene->base };
		if (Ent_HasComponent (ent.id, ent.base + scene_href, ent.reg)) {
			camera = Entity_Transform (ent);
		}
	}
	if (scene) {
		uint32_t c_animation = scene->base + scene_animation;
		uint32_t c_renderer = scene->base + scene_renderer;
		auto reg = scene->reg;
		auto animpool = reg->comp_pools + c_animation;
		auto rendpool = reg->comp_pools + c_renderer;
		Anim_Update (con_realtime, animpool, rendpool);
	}
	SCR_UpdateScreen (camera, con_realtime, bi_2dfuncs);
	R_FLOAT (pr) = con_frametime;
}

static void
bi_refresh_2d (progs_t *pr, void *_res)
{
	qc2d = P_FUNCTION (pr, 0);
}

static void
bi_setpalette (progs_t *pr, void *_res)
{
	byte       *palette = (byte *) P_GPOINTER (pr, 0);
	byte       *colormap = (byte *) P_GPOINTER (pr, 1);
	VID_SetPalette (palette, colormap);
}

static void
bi_setevents (progs_t *pr, void *_res)
{
	qcevent = P_FUNCTION (pr, 0);
	qcevent_data = P_POINTER (pr, 1);
}

static void
bi_setctxcbuf (progs_t *pr, void *_res)
{
	IMT_SetContextCbuf (P_INT (pr, 0), qwaq_cbuf);
}

#define bi(x,n,np,params...) {#x, bi_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(init_graphics, -1, 1, p(ptr)),
	bi(newscene,      -1, 1, p(long)),
	bi(refresh,       -1, 1, p(long)),
	bi(refresh_2d,    -1, 1, p(func)),
	bi(setpalette,    -1, 2, p(ptr), p(ptr)),
	bi(setevents,     -1, 2, p(func), p(ptr)),
	bi(setctxcbuf,    -1, 1, p(int)),
	{0}
};

static int
event_handler (const IE_event_t *ie_event, void *_pr)
{
	if (qcevent) {
		progs_t    *pr = _pr;
		int num_params = sizeof (IE_event_t) / sizeof (pr_type_t);
		num_params = (num_params + 3) / 4 + 2;

		PR_PushFrame (pr);
		auto params = PR_SaveParams (pr);
		PR_SetupParams (pr, num_params, 2);
		auto event = &P_PACKED (pr, IE_event_t, 2);
		P_POINTER (pr, 0) = PR_SetPointer (pr, event);
		P_POINTER (pr, 1) = qcevent_data;
		*event = *ie_event;
		PR_ExecuteProgram (pr, qcevent);
		int ret = R_INT (pr);
		PR_RestoreParams (pr, params);
		PR_PopFrame (pr);
		return ret;
	}
	return IN_Binding_HandleEvent (ie_event);
}

static void
BI_shutdown (void *data)
{
	printf ("BI_shutdown\n");
	ECS_DelRegistry (canvas_sys.reg);
	ColorCache_Shutdown ();
}

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
		// fullbrights, but avoid copying the source row to itself
		if (i != 31) {
			memcpy (colors[i][224], colors[31][224], 32 * 3);
		}
	}
	size_t mark = Hunk_LowMark (nullptr);
	auto cmap = ConvertImage (&tex, default_palette[0], "cmap");
	// the colormap has an extra byte indicating the number of fullbright
	// entries, but that byte is not in the image, so don't try to copy it,
	// thus the - 1
	memcpy (default_colormap, cmap->data, sizeof (default_colormap) - 1);
	Hunk_FreeToLowMark (nullptr, mark);
}

static const char *bi_dirconf = R"(
{
	QF = {
		Path = "QF";
	};
	qwaq = {
		Inherit = (QF);
		Path = "qwaq";
	};
	qwaq:* = {
		Inherit = (qwaq);
		Path = "qwaq/$gamedir";
	};
}
)";

void
BI_Graphics_Init (progs_t *pr)
{
	qwaq_thread_t *thread = PR_Resources_Find (pr, "qwaq_thread");

	PR_RegisterBuiltins (pr, builtins, 0);
	BT_Init (this_program);

	QFS_SetConfig (PL_GetPropertyList (bi_dirconf, nullptr));
	QFS_Init (thread->hunk, "qwaq");
	PI_Init ();
	PI_RegisterPlugins (client_plugin_list);

	Sys_RegisterShutdown (BI_shutdown, pr);

	R_Progs_Init (pr);
	RUA_Game_Init (pr, thread->rua_security);
	S_Progs_Init (pr);

	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	generate_palette ();
	generate_colormap ();

	event_handler_id = IE_Add_Handler (event_handler, pr);
	IE_Set_Focus (event_handler_id);
}
