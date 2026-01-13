/*
	vid_wl.c

	General Wayland video driver

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "context_wl.h"
#include "vid_internal.h"

static vid_internal_t vid_internal;

int 		VID_options_items = 1;

static void
WL_VID_Shutdown (void)
{
    Sys_MaskPrintf(SYS_wayland, "WL_VID_Shutdown\n");

    WL_CloseDisplay();

    if (vid_internal.unload) {
        vid_internal.unload(vid_internal.ctx);
    }
}

static void
WL_VID_SetPalette (byte *palette, byte *colormap)
{
    qfZoneScoped (true);
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];
	if (vid_internal.set_colormap) {
		vid_internal.set_colormap (vid_internal.ctx, colormap);
	}

	VID_InitGamma (palette);
	vid_internal.set_palette (vid_internal.ctx, viddef.palette);
}

static void
WL_VID_SetCursor (bool visible)
{
}

static void
WL_VID_Init (byte *palette, byte *colormap)
{
    qfZoneScoped (true);
	vid_internal.gl_context = WL_GL_Context;
	vid_internal.sw_context = WL_SW_Context;
#ifdef HAVE_VULKAN
	vid_internal.vulkan_context = WL_Vulkan_Context;
#endif

	R_LoadModule (&vid_internal);

	viddef.numpages = 2;

	srandom (getpid ());

	VID_GetWindowSize (640, 480);
	WL_OpenDisplay ();
	vid_internal.choose_visual (vid_internal.ctx);
    WL_CreateWindow (viddef.width, viddef.height);
    vid_internal.create_context (vid_internal.ctx);

    WL_VID_SetPalette (palette, colormap);
    
    Sys_MaskPrintf (SYS_vid, "Video mode %dx%d initialized.\n", viddef.width, viddef.height);

    viddef.initialized = true;
	viddef.recalc_refdef = 1;			// force a surface cache flush
}

static void
WL_VID_Init_Cvars (void)
{
    WL_Init_Cvars ();

#ifdef HAVE_VULKAN
	WL_Vulkan_Init_Cvars ();
#endif
}

vid_system_t vid_system = {
    .init = WL_VID_Init,
    .shutdown = WL_VID_Shutdown,
    .init_cvars = WL_VID_Init_Cvars,
    .update_fullscreen = WL_UpdateFullscreen,
    .set_palette = WL_VID_SetPalette,
    .set_cursor = WL_VID_SetCursor,
};

void
VID_SetCaption (const char *text)
{
}

bool
VID_SetGamma (double gamma)
{
    return false;
}
