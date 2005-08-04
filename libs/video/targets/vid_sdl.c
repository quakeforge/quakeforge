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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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

#include "d_iface.h"

#ifdef _WIN32	// FIXME: evil hack to get full DirectSound support with SDL
#include <windows.h>
#include <SDL_syswm.h>
HWND 		mainwindow;
#endif

// The original defaults
#define BASEWIDTH 320
#define BASEHEIGHT 200

byte       *VGA_pagebase;
int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;

SDL_Surface *screen = NULL;


void
VID_SetPalette (unsigned char *palette)
{
	SDL_Color   colors[256];
	int         i;

	for (i = 0; i < 256; ++i) {
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}
	SDL_SetColors (screen, colors, 0, 256);
}

static void
do_screen_buffer (void)
{
}

void
VID_Init (unsigned char *palette)
{
	Uint32      flags;

	// Load the SDL library
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
		Sys_Error ("VID: Couldn't load SDL: %s", SDL_GetError ());

	// Set up display mode (width and height)
	VID_GetWindowSize (BASEWIDTH, BASEHEIGHT);

	// Set video width, height and flags
	flags = (SDL_SWSURFACE | SDL_HWPALETTE);
	if (vid_fullscreen->int_val)
		flags |= SDL_FULLSCREEN;

	// Initialize display
	if (!(screen = SDL_SetVideoMode (vid.width, vid.height, 8, flags)))
		Sys_Error ("VID: Couldn't set video mode: %s", SDL_GetError ());
	VID_InitGamma (palette);
	VID_SetPalette (vid.palette);

	// now know everything we need to know about the buffer
	VGA_width = vid.conwidth = vid.width;
	VGA_height = vid.conheight = vid.height;
	Con_CheckResize (); // Now that we have a window size, fix console
	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 1;
	vid.colormap8 = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap8 + 2048));
	vid.do_screen_buffer = do_screen_buffer;
	VGA_pagebase = vid.buffer = screen->pixels;
	VGA_rowbytes = vid.rowbytes = screen->pitch;
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.direct = 0;

	VID_InitBuffers ();		// allocate z buffer and surface cache

	SDL_ShowCursor (0);		// hide the mouse pointer

#ifdef _WIN32
// FIXME: EVIL thing - but needed for win32 until
// SDL_sound works better - without this DirectSound fails.

//	SDL_GetWMInfo(&info);
//	mainwindow=info.window;
	mainwindow=GetActiveWindow();
#endif

	vid.initialized = true;
}

void
VID_Update (vrect_t *rects)
{
	SDL_Rect   *sdlrects;
	int         i, n;
	vrect_t    *rect;

	// Two-pass system, since Quake doesn't do it the SDL way...

	// First, count the number of rectangles
	n = 0;
	for (rect = rects; rect; rect = rect->pnext)
		++n;

	// Second, copy them to SDL rectangles and update
	if (!(sdlrects = (SDL_Rect *) calloc (1, n * sizeof (SDL_Rect))))
		Sys_Error ("Out of memory!");
	i = 0;
	for (rect = rects; rect; rect = rect->pnext) {
		sdlrects[i].x = rect->x;
		sdlrects[i].y = rect->y;
		sdlrects[i].w = rect->width;
		sdlrects[i].h = rect->height;
		++i;
	}
	SDL_UpdateRects (screen, n, sdlrects);
}

void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
	Uint8      *offset;

	if (!screen)
		return;
	if (x < 0)
		x = screen->w + x - 1;
	offset = (Uint8 *) screen->pixels + y * screen->pitch + x;
	while (height--) {
		memcpy (offset, pbitmap, width);
		offset += screen->pitch;
		pbitmap += width;
	}
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
	if (!screen)
		return;
	if (x < 0)
		x = screen->w + x - 1;
	SDL_UpdateRect (screen, x, y, width, height);
}

void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}
