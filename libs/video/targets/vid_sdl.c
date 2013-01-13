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

byte       *VGA_pagebase;
int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;

SDL_Surface *screen = NULL;

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

static void (*set_vid_mode) (Uint32 flags);

static void (GLAPIENTRY *qfglFinish) (void);
static int use_gl_procaddress = 0;
static cvar_t  *gl_driver;

static byte cached_palette[256 * 3];
static int update_palette;

static void *
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void    *glfunc = NULL;

	Sys_MaskPrintf (SYS_VID, "DEBUG: Finding symbol %s ... ", name);

	glfunc = SDL_GL_GetProcAddress (name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_VID, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_VID, "not found\n");

	if (crit) {
		if (strncmp ("fxMesa", name, 6) == 0) {
			Sys_Printf ("This target requires a special version of Mesa with "
						"support for Glide and SVGAlib.\n");
			Sys_Printf ("If you are in X, try using a GLX or SGL target.\n");
		}
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...",
				   name);
	}
	return NULL;
}

static void
sdlgl_set_vid_mode (Uint32 flags)
{
	int         i, j;

	flags |= SDL_OPENGL;

	// Setup GL Attributes
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
//	SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 0);	// Try for 0, 8
//	SDL_GL_SetAttribute (SDL_GL_STEREO, 1);			// Someday...

	for (i = 0; i < 5; i++) {
		int k;
		int color[5] = {32, 24, 16, 15, 0};
		int rgba[5][4] = {
			{8, 8, 8, 0},
			{8, 8, 8, 8},
			{5, 6, 5, 0},
			{5, 5, 5, 0},
			{5, 5, 5, 1},
		};

		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, rgba[i][0]);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, rgba[i][1]);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, rgba[i][2]);
		SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, rgba[i][3]);

		for (j = 0; j < 5; j++) {
			for (k = 32; k >= 16; k -= 8) {
				SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, k);
				if ((screen = SDL_SetVideoMode (viddef.width, viddef.height,
												color[j], flags)))
					goto success;
			}
		}
	}

	Sys_Error ("Couldn't set video mode: %s", SDL_GetError ());
	SDL_Quit ();

success:
	viddef.numpages = 2;

	viddef.init_gl ();
}

static void
sdlgl_end_rendering (void)
{
	qfglFinish ();
	SDL_GL_SwapBuffers ();
}

static void
sdl_load_gl (void)
{
	viddef.get_proc_address = QFGL_ProcAddress;
	viddef.end_rendering = sdlgl_end_rendering;
	set_vid_mode = sdlgl_set_vid_mode;

	if (SDL_GL_LoadLibrary (gl_driver->string) != 0)
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);

	use_gl_procaddress = 1;

	qfglFinish = QFGL_ProcAddress ("glFinish", true);
}

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
	SDL_SetColors (screen, colors, 0, 256);
}

static void
VID_SetPalette (const byte *palette)
{
	if (memcmp (cached_palette, palette, sizeof (cached_palette))) {
		memcpy (cached_palette, palette, sizeof (cached_palette));
		update_palette = 1;
	}
}

static void
do_screen_buffer (void)
{
}

static void
sdl_set_vid_mode (Uint32 flags)
{
	// Initialize display
	if (!(screen = SDL_SetVideoMode (viddef.width, viddef.height, 8, flags)))
		Sys_Error ("VID: Couldn't set video mode: %s", SDL_GetError ());

	// now know everything we need to know about the buffer
	VGA_width = viddef.width;
	VGA_height = viddef.height;
	viddef.do_screen_buffer = do_screen_buffer;
	VGA_pagebase = viddef.buffer = screen->pixels;
	VGA_rowbytes = viddef.rowbytes = screen->pitch;
	viddef.conbuffer = viddef.buffer;
	viddef.conrowbytes = viddef.rowbytes;
	viddef.direct = 0;

	VID_InitBuffers ();		// allocate z buffer and surface cache
}

void
VID_Init (byte *palette, byte *colormap)
{
	Uint32      flags;

	viddef.load_gl = sdl_load_gl;
	viddef.set_palette = VID_SetPalette;
	set_vid_mode = sdl_set_vid_mode;

	// Load the SDL library
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
		Sys_Error ("VID: Couldn't load SDL: %s", SDL_GetError ());

	R_LoadModule ();

	viddef.numpages = 1;
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];

	// Set up display mode (width and height)
	VID_GetWindowSize (BASEWIDTH, BASEHEIGHT);

	// Set video width, height and flags
	flags = (SDL_SWSURFACE | SDL_HWPALETTE);
	if (vid_fullscreen->int_val) {
		flags |= SDL_FULLSCREEN;
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

	set_vid_mode (flags);

	VID_SDL_GammaCheck ();
	VID_InitGamma (palette);
	viddef.set_palette (viddef.palette);

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
	gl_driver = Cvar_Get ("gl_driver", GL_DRIVER, CVAR_ROM, NULL,
						  "The OpenGL library to use. (path optional)");
}

void
VID_Update (vrect_t *rects)
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
	SDL_UpdateRects (screen, n, sdlrects);
}

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
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
