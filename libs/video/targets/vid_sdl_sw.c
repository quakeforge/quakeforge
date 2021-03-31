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
#include "vid_sw.h"

#ifdef _WIN32	// FIXME: evil hack to get full DirectSound support with SDL
#include <windows.h>
#include <SDL_syswm.h>
#endif

// The original defaults
#define BASEWIDTH 320
#define BASEHEIGHT 200

byte       *VGA_pagebase;
int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;

// Define GLAPIENTRY to a useful value
#ifndef GLAPIENTRY
# ifdef _WIN32
#  include <windows.h>
#  define GLAPIENTRY WINAPI
#  undef LoadImage
# else
#  ifdef APIENTRY
#   define GLAPIENTRY APIENTRY
#  else
#   define GLAPIENTRY
#  endif
# endif
#endif

static byte cached_palette[256 * 3];
static int update_palette;

static void
sdl_update_palette (const byte *palette)
{
	SDL_Color   colors[256];
	int         i;

	for (i = 0; i < 256; ++i) {
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}
	SDL_SetColors (sdl_screen, colors, 0, 256);
}

static void
sdl_set_palette (const byte *palette)
{
	if (memcmp (cached_palette, palette, sizeof (cached_palette))) {
		memcpy (cached_palette, palette, sizeof (cached_palette));
		update_palette = 1;
	}
}

static void
sdl_init_buffers (void)
{
}

static void
sdl_set_vid_mode (sw_ctx_t *ctx)
{
	// Initialize display
	if (!(sdl_screen = SDL_SetVideoMode (viddef.width, viddef.height, 8,
									 sdl_flags)))
		Sys_Error ("VID: Couldn't set video mode: %s", SDL_GetError ());

	// now know everything we need to know about the buffer
	VGA_width = viddef.width;
	VGA_height = viddef.height;
	viddef.vid_internal->init_buffers = sdl_init_buffers;
	VGA_pagebase = viddef.buffer = sdl_screen->pixels;
	VGA_rowbytes = viddef.rowbytes = sdl_screen->pitch;
	viddef.conbuffer = viddef.buffer;
	viddef.conrowbytes = viddef.rowbytes;
	viddef.direct = 0;

	VID_InitBuffers ();		// allocate z buffer and surface cache
}

static void
sdl_sw_update (vrect_t *rects)
{
	static SDL_Rect *sdlrects;
	static int  num_sdlrects;
	int         i, n;
	vrect_t    *rect;

	if (update_palette) {
		update_palette = 0;
		sdl_update_palette (cached_palette);
	}
	// Two-pass system, since Quake doesn't do it the SDL way...

	// First, count the number of rectangles
	n = 0;
	for (rect = rects; rect; rect = rect->next)
		++n;

	if (n > num_sdlrects) {
		num_sdlrects = n;
		sdlrects = realloc (sdlrects, n * sizeof (SDL_Rect));
		if (!sdlrects)
			Sys_Error ("Out of memory!");
	}

	// Second, copy them to SDL rectangles and update
	i = 0;
	for (rect = rects; rect; rect = rect->next) {
		sdlrects[i].x = rect->x;
		sdlrects[i].y = rect->y;
		sdlrects[i].w = rect->width;
		sdlrects[i].h = rect->height;
		++i;
	}
	SDL_UpdateRects (sdl_screen, n, sdlrects);
}

sw_ctx_t *
SDL_SW_Context (void)
{
	sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
    ctx->set_palette = sdl_set_palette;
    ctx->create_context = sdl_set_vid_mode;
    ctx->update = sdl_sw_update;
    return ctx;
}

void
SDL_SW_Init_Cvars ()
{
}
