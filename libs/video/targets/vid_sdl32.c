/*
	vid_sdl32.c

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
static const char rcsid[] = 
	"$Id$";

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
#include "QF/va.h"
#include "QF/vid.h"

//#include "compat.h"
#include "d_iface.h"
#include "d_local.h"

#ifdef WIN32
/* fixme: this is evil hack to get full DirectSound support with SDL */
#include <windows.h>
#include <SDL_syswm.h>
HWND 		mainwindow;
#endif

// static float oldin_grab = 0;

cvar_t     *vid_bitdepth;
extern viddef_t vid;					// global video state

int modestate; // fixme: just to avoid cross-comp errors - remove later

// The original defaults
#define    BASEWIDTH    320
#define    BASEHEIGHT   200

int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte       *VGA_pagebase;

static SDL_Surface *screen = NULL;
static SDL_Surface *rendersurface = NULL;

extern void VID_MakeColormaps (int fullbrights, byte *pal);


void
VID_SetPalette (unsigned char *palette)
{
	if (vid_bitdepth->int_val == 8)
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
	Con_CheckResize (); // Now that we have a window size, fix console

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;

	if (vid_bitdepth->int_val != 8 && vid_bitdepth->int_val != 16 &&
		vid_bitdepth->int_val != 32)
		Sys_Error("unsupported vid_bitdepth %i, must be 8, 16, or 32",
				  vid_bitdepth->int_val);

	// Set video width, height and flags
	flags = SDL_SWSURFACE;
	if (vid_bitdepth->int_val == 8)
		flags |= SDL_HWPALETTE;
	if (vid_fullscreen->int_val)
		flags |= SDL_FULLSCREEN;

	// Initialize display
	if (!(screen = SDL_SetVideoMode (vid.width, vid.height,
									 vid_bitdepth->int_val, flags)))
		Sys_Error ("VID: Couldn't set video mode: %s\n", SDL_GetError ());
	VID_InitGamma (palette);
	VID_SetPalette (palette);

	switch (vid_bitdepth->int_val) {
	case 8:
		r_pixbytes = 1;
		rendersurface = screen;
		break;
	case 16:
		r_pixbytes = 2;
		rendersurface =	SDL_CreateRGBSurface (SDL_SWSURFACE, vid.width,
											  vid.height, 16, 0xF800, 0x07E0,
											  0x001F, 0x0000);
		break;
	case 32:
		r_pixbytes = 4;
		if (bigendien)
			rendersurface = SDL_CreateRGBSurface
				(SDL_SWSURFACE, vid.width, vid.height, 32, 0xFF000000,
				 0x00FF0000, 0x0000FF00, 0x00000000);
		else
			rendersurface = SDL_CreateRGBSurface
				(SDL_SWSURFACE, vid.width, vid.height, 32, 0x000000FF,
				 0x0000FF00, 0x00FF0000, 0x00000000);
		break;
	default:
		Sys_Error ("VID_Init: unsupported bit depth\n");
	}

	// now we know everything we need to know about the buffer
	VGA_width = vid.conwidth = vid.width;
	VGA_height = vid.conheight = vid.height;
	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 1;
	if (vid_colormap)
		VID_MakeColormaps(256 - vid_colormap[16384], vid_basepal);
	else
		VID_MakeColormaps(224, vid_basepal);
	VGA_pagebase = vid.buffer = rendersurface->pixels;
	VGA_rowbytes = vid.rowbytes = rendersurface->pitch;
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.direct = rendersurface->pixels;

	// allocate z buffer and surface cache
	VID_InitBuffers ();

	// initialize the mouse
	SDL_ShowCursor (0);

#ifdef WIN32
        // fixme: EVIL thing - but needed for win32 until
        // SDL_sound works better - without this DirectSound fails.

//        SDL_GetWMInfo(&info);
//        mainwindow=info.window;
        mainwindow=GetActiveWindow();
#endif

	vid.initialized = true;
}

void
VID_UpdateFullscreen (cvar_t *vid_fullscreen)
{
	if (!vid.initialized)
		return;
	if ((vid_fullscreen->int_val && !(screen->flags & SDL_FULLSCREEN)) ||
		(!vid_fullscreen->int_val && screen->flags & SDL_FULLSCREEN))
		if (!SDL_WM_ToggleFullScreen (screen))
			Con_Printf ("VID_UpdateFullscreen: error setting fullscreen\n");
}

void
VID_Init_Cvars ()
{
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_NONE,
							   VID_UpdateFullscreen,
							   "Toggles fullscreen game mode");
	vid_bitdepth = Cvar_Get ("vid_bitdepth", "8", CVAR_ROM, NULL, "Sets "
							 "display bitdepth (supported modes: 8 16 32)");
}

void
VID_Shutdown (void)
{
	SDL_Quit ();
}

void
VID_Update (vrect_t *rects)
{
	while (rects) {
		if (vid_bitdepth->int_val != 8)
		{
			SDL_Rect    sdlrect;

			sdlrect.x = rects->x;
			sdlrect.y = rects->y;
			sdlrect.w = rects->width;
			sdlrect.h = rects->height;

			// blit internal framebuffer to display buffer
			SDL_BlitSurface(rendersurface, &sdlrect, screen, &sdlrect);
		}
		// update display
		SDL_UpdateRect (screen, rects->x, rects->y, rects->width, rects->height);
		rects = rects->pnext;
	}
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

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);
		SDL_WM_SetCaption (va ("%s %s: %s", PROGRAM, VERSION, temp), NULL);
		free (temp);
	} else
		SDL_WM_SetCaption (va ("%s %s", PROGRAM, VERSION), NULL);
}

qboolean
VID_SetGamma (double gamma)
{
	return false; //FIXME
}
