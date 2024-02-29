/*
	vid_win.c

	Win32 vid component

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "context_win.h"
#include "vid_internal.h"
#include "vid_sw.h"

static vid_internal_t vid_internal;

static void
Win_VID_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_vid, "Win_VID_shutdown\n");
	Win_CloseDisplay ();
}

static void
Win_VID_SetPalette (byte *palette, byte *colormap)
{
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];
	if (vid_internal.set_colormap) {
		vid_internal.set_colormap (vid_internal.ctx, colormap);
	}

	VID_InitGamma (palette);
	viddef.vid_internal->set_palette (win_sw_context, palette);
}

static void
Win_VID_SetCursor (bool visible)
{
	win_cursor_visible = visible;
	SetCursor (visible ? win_arrow : 0);
}

static void
Win_VID_Init (byte *palette, byte *colormap)
{
	Sys_RegisterShutdown (Win_VID_shutdown, 0);

	vid_internal.gl_context = Win_GL_Context;
	vid_internal.sw_context = Win_SW_Context;
#ifdef HAVE_VULKAN
	vid_internal.vulkan_context = Win_Vulkan_Context;
#endif

	R_LoadModule (&vid_internal);

	viddef.numpages = 1;

	VID_GetWindowSize (640, 480);
	Win_OpenDisplay ();
	vid_internal.choose_visual (win_sw_context);
	Win_CreateWindow (viddef.width, viddef.height);
	vid_internal.create_context (win_sw_context);

	Win_VID_SetPalette (palette, colormap);

	Sys_MaskPrintf (SYS_vid, "Video mode %dx%d initialized.\n",
					viddef.width, viddef.height);

	viddef.initialized = true;
}

static void
Win_VID_Init_Cvars (void)
{
	Win_Init_Cvars ();
#ifdef HAVE_VULKAN
	Win_Vulkan_Init_Cvars ();
#endif
	Win_GL_Init_Cvars ();
	Win_SW_Init_Cvars ();
}

vid_system_t vid_system = {
	.init = Win_VID_Init,
	.set_palette = Win_VID_SetPalette,
	.init_cvars = Win_VID_Init_Cvars,
	.update_fullscreen = Win_UpdateFullscreen,
	.set_cursor = Win_VID_SetCursor,
};

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		Win_SetCaption (va (0, "%s: %s", PACKAGE_STRING, temp));
		free (temp);
	} else {
		Win_SetCaption (va (0, "%s", PACKAGE_STRING));
	}
}

bool
VID_SetGamma (double gamma)
{
	return Win_SetGamma (gamma);
}
