/*
	vid_sdl.c

	Video driver for Sam Lantinga's Simple DirectMedia Layer

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>
#include <SDL.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_sdl.h"
#include "d_iface.h"
#include "vid_internal.h"

#ifdef _WIN32	// FIXME: evil hack to get full DirectSound support with SDL
#include <windows.h>
#include <SDL_syswm.h>
HWND 		mainwindow;
#endif

// The original defaults
#define BASEWIDTH 320
#define BASEHEIGHT 200

SDL_Surface *sdl_screen = NULL;

static vid_internal_t vid_internal;

uint32_t    sdl_flags;

static void
VID_shutdown (void)
{
	SDL_Quit ();
}

void
VID_Init (byte *palette, byte *colormap)
{
	Sys_RegisterShutdown (VID_shutdown);

	vid_internal.gl_context = SDL_GL_Context;
	vid_internal.sw_context = SDL_SW_Context;
	// Load the SDL library
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
		Sys_Error ("VID: Couldn't load SDL: %s", SDL_GetError ());

	R_LoadModule (&vid_internal);

	viddef.numpages = 1;
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];

	// Set up display mode (width and height)
	VID_GetWindowSize (BASEWIDTH, BASEHEIGHT);

	// Set video width, height and flags
	sdl_flags = (SDL_SWSURFACE | SDL_HWPALETTE);
	if (vid_fullscreen->int_val) {
		sdl_flags |= SDL_FULLSCREEN;
#ifndef _WIN32      // Don't annoy Mesa/3dfx folks
		// doesn't hurt if not using a gl renderer
		// FIXME: Maybe this could be put in a different spot, but I don't
		// know where. Anyway, it's to work around a 3Dfx Glide bug.
		//      Cvar_SetValue (in_grab, 1); // Needs #include "QF/input.h"
		putenv ((char *)"MESA_GLX_FX=fullscreen");
	} else {
		putenv ((char *)"MESA_GLX_FX=window");
#endif
	}

	vid_internal.create_context ();

	VID_SDL_GammaCheck ();
	VID_InitGamma (palette);

	viddef.initialized = true;

	SDL_ShowCursor (0);		// hide the mouse pointer

#ifdef _WIN32
// FIXME: EVIL thing - but needed for win32 until
// SDL_sound works better - without this DirectSound fails.

//	SDL_GetWMInfo(&info);
//	mainwindow=info.window;
	mainwindow=GetActiveWindow();
#endif

	viddef.recalc_refdef = 1;				// force a surface cache flush
}

void
VID_Init_Cvars ()
{
	SDL_Init_Cvars ();
	SDL_GL_Init_Cvars ();
	SDL_SW_Init_Cvars ();
}

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	Uint8      *offset;

	if (!sdl_screen)
		return;
	if (x < 0)
		x = sdl_screen->w + x - 1;
	offset = (Uint8 *) sdl_screen->pixels + y * sdl_screen->pitch + x;
	while (height--) {
		memcpy (offset, pbitmap, width);
		offset += sdl_screen->pitch;
		pbitmap += width;
	}
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
	if (!sdl_screen)
		return;
	if (x < 0)
		x = sdl_screen->w + x - 1;
	SDL_UpdateRect (sdl_screen, x, y, width, height);
}

void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}
