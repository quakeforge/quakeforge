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

	$Id$
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <SDL.h>

#include "QF/cvar.h"
#include "d_local.h"
#include "host.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "vid.h"

// static float oldin_grab = 0;

cvar_t     *vid_fullscreen;
extern viddef_t vid;					// global video state
unsigned short d_8to16table[256];

#ifdef WIN32
// FIXME: this is evil...
#include <windows.h>
HWND        mainwindow;
#endif

int         modestate;					// FIXME: just to avoid cross-comp.

										// errors - remove later

// The original defaults
#define    BASEWIDTH    320
#define    BASEHEIGHT   200

int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte       *VGA_pagebase;

static SDL_Surface *screen = NULL;

void
VID_InitBuffers (int width, int height)
{
	int         tbuffersize, tcachesize;
	void       *vid_surfcache;

	// Calculate the sizes we want first
	tbuffersize = vid.width * vid.height * sizeof (*d_pzbuffer);
	tcachesize = D_SurfaceCacheForRes (width, height);

	// Free the old z-buffer
	if (d_pzbuffer) {
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	// Free the old surface cache
	vid_surfcache = D_SurfaceCacheAddress ();
	if (vid_surfcache) {
		D_FlushCaches ();
		free (vid_surfcache);
		vid_surfcache = NULL;
	}
	// Allocate the new z-buffer
	d_pzbuffer = calloc (tbuffersize, 1);
	if (!d_pzbuffer) {
		Sys_Error ("Not enough memory for video mode\n");
	}
	// Allocate the new surface cache; free the z-buffer if we fail
	vid_surfcache = calloc (tcachesize, 1);
	if (!vid_surfcache) {
		free (d_pzbuffer);
		d_pzbuffer = NULL;
		Sys_Error ("Not enough memory for video mode\n");
	}

	D_InitCaches (vid_surfcache, tcachesize);
}

void
VID_SetPalette (unsigned char *palette)
{
	int         i;
	SDL_Color   colors[256];

	for (i = 0; i < 256; ++i) {
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}
	SDL_SetColors (screen, colors, 0, 256);
}

void
VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}

void
VID_Init (unsigned char *palette)
{
	// Uint8 video_bpp;
	// Uint16 video_w, video_h;
	Uint32      flags;

	// Load the SDL library
	if (SDL_Init (SDL_INIT_VIDEO) < 0)	// |SDL_INIT_AUDIO|SDL_INIT_CDROM) <
		// 0)
		Sys_Error ("VID: Couldn't load SDL: %s", SDL_GetError ());

	// Set up display mode (width and height)
	VID_GetWindowSize (BASEWIDTH, BASEHEIGHT);
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;

	// Set video width, height and flags
	flags = (SDL_SWSURFACE | SDL_HWPALETTE);
	if (vid_fullscreen->int_val)
		flags |= SDL_FULLSCREEN;

	// Initialize display
	if (!(screen = SDL_SetVideoMode (vid.width, vid.height, 8, flags)))
		Sys_Error ("VID: Couldn't set video mode: %s\n", SDL_GetError ());
	VID_SetPalette (palette);
	VID_SetCaption ("");

	// now know everything we need to know about the buffer
	VGA_width = vid.conwidth = vid.width;
	VGA_height = vid.conheight = vid.height;
	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
	VGA_pagebase = vid.buffer = screen->pixels;
	VGA_rowbytes = vid.rowbytes = screen->pitch;
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.direct = 0;

	// allocate z buffer and surface cache
	VID_InitBuffers (vid.width, vid.height);

	// initialize the mouse
	SDL_ShowCursor (0);

#ifdef WIN32
	// FIXME: EVIL thing - but needed for win32 until we get
	// SDL_sound ready - without this DirectSound fails.
	// could replace this with SDL_SysWMInfo
	mainwindow = GetActiveWindow ();
#endif
}

void
VID_Init_Cvars ()
{
	vid_fullscreen =
		Cvar_Get ("vid_fullscreen", "0", CVAR_ROM, NULL,
				  "Toggles fullscreen game mode");
}

void
VID_Shutdown (void)
{
	SDL_Quit ();
}

void
VID_Update (vrect_t *rects)
{
	SDL_Rect   *sdlrects;
	int         n, i;
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

/*
================
D_BeginDirectRect
================
*/
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

/*
================
D_EndDirectRect
================
*/
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

void
VID_SetCaption (char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		SDL_WM_SetCaption (va ("%s %s: %s", PROGRAM, VERSION, temp), NULL);
		free (temp);
	} else {
		SDL_WM_SetCaption (va ("%s %s", PROGRAM, VERSION), NULL);
	}
}

void
VID_HandlePause (qboolean paused)
{
}
