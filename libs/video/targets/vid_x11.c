/*
	vid_x11.c

	General X11 video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
#define _BSD

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

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "context_x11.h"
#include "d_iface.h"
#include "vid_internal.h"

static vid_internal_t vid_internal;

int 		VID_options_items = 1;

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported
}

static void
X11_VID_Shutdown (void)
{
	Sys_MaskPrintf (SYS_vid, "X11_VID_shutdown\n");
	X11_CloseDisplay ();
	if (vid_internal.unload) {
		vid_internal.unload (vid_internal.ctx);
	}
}

static void
X11_VID_SetPalette (byte *palette, byte *colormap)
{
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];
	if (vid_internal.set_colormap) {
		vid_internal.set_colormap (vid_internal.ctx, colormap);
	}

	VID_InitGamma (palette);
	vid_internal.set_palette (vid_internal.ctx, viddef.palette);
}

/*
	Set up color translation tables and the window.  Takes a 256-color 8-bit
	palette.  Palette data will go away after the call, so copy it if you'll
	need it later.
*/
static void
X11_VID_Init (byte *palette, byte *colormap)
{
	vid_internal.gl_context = X11_GL_Context;
	vid_internal.sw_context = X11_SW_Context;
#ifdef HAVE_VULKAN
	vid_internal.vulkan_context = X11_Vulkan_Context;
#endif

	R_LoadModule (&vid_internal);

	viddef.numpages = 2;

	srandom (getpid ());

	VID_GetWindowSize (640, 480);
	X11_OpenDisplay ();
	vid_internal.choose_visual (vid_internal.ctx);
	X11_SetVidMode (viddef.width, viddef.height);
	X11_CreateWindow (viddef.width, viddef.height);
	X11_CreateNullCursor ();	// hide mouse pointer
	vid_internal.create_context (vid_internal.ctx);

	X11_VID_SetPalette (palette, colormap);

	Sys_MaskPrintf (SYS_vid, "Video mode %dx%d initialized.\n",
					viddef.width, viddef.height);

	viddef.initialized = true;
	viddef.recalc_refdef = 1;			// force a surface cache flush
}

static void
X11_VID_Init_Cvars (void)
{
	X11_Init_Cvars ();
#ifdef HAVE_VULKAN
	X11_Vulkan_Init_Cvars ();
#endif
	X11_GL_Init_Cvars ();
	X11_SW_Init_Cvars ();
}

vid_system_t vid_system = {
	.init = X11_VID_Init,
	.shutdown = X11_VID_Shutdown,
	.init_cvars = X11_VID_Init_Cvars,
	.update_fullscreen = X11_UpdateFullscreen,
	.set_palette = X11_VID_SetPalette,
};

#if 0
static int  config_notify = 0;
static int  config_notify_width;
static int  config_notify_height;

static void
update ()
{
	/* If the window changes dimension, skip this frame. */
	if (config_notify) {
		fprintf (stderr, "config notify\n");
		config_notify = 0;
		viddef.width = config_notify_width & ~7;
		viddef.height = config_notify_height;

		VID_InitBuffers ();

		viddef.recalc_refdef = 1;			/* force a surface cache flush */
		Con_CheckResize ();
		return;
	}
}
#endif

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		X11_SetCaption (va (0, "%s: %s", PACKAGE_STRING, temp));
		free (temp);
	} else {
		X11_SetCaption (va (0, "%s", PACKAGE_STRING));
	}
}

bool
VID_SetGamma (double gamma)
{
	return X11_SetGamma (gamma);
}
