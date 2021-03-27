/*
	vid_win.c

	Win32 SW vid component

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

#include "winquake.h"
#include <ddraw.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_win.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

typedef union {
	byte        bgra[4];
	uint32_t    value;
} win_palette_t;

static win_palette_t st2d_8to32table[256];
static byte     current_palette[768];
static int      palette_changed;

static void
win_init_bufers (void)
{
	// set the rest of the buffers we need (why not just use one single buffer
	// instead of all this crap? oh well, it's Quake...)
	viddef.direct = viddef.buffer;
	viddef.conbuffer = viddef.buffer;

	// more crap for the console
	viddef.conrowbytes = viddef.rowbytes;
}

static void
win_set_palette (const byte *palette)
{
	palette_changed = 1;
	if (palette != current_palette) {
		memcpy (current_palette, palette, sizeof (current_palette));
	}
	for (int i = 0; i < 256; i++) {
		const byte *pal = palette + 3 * i;
		st2d_8to32table[i].bgra[0] = viddef.gammatable[pal[2]];
		st2d_8to32table[i].bgra[1] = viddef.gammatable[pal[1]];
		st2d_8to32table[i].bgra[2] = viddef.gammatable[pal[0]];
		st2d_8to32table[i].bgra[3] = 255;
	}
	if (!Minimized && !win_using_ddraw && win_dib_section) {
		RGBQUAD     colors[256];
		memcpy (colors, st2d_8to32table, sizeof (colors));
		for (int i = 0; i < 256; i++) {
			colors[i].rgbReserved = 0;
		}
		colors[0].rgbRed = 0;
		colors[0].rgbGreen = 0;
		colors[0].rgbBlue = 0;
		colors[255].rgbRed = 0xff;
		colors[255].rgbGreen = 0xff;
		colors[255].rgbBlue = 0xff;

		if (SetDIBColorTable (win_dib_section, 0, 256, colors) == 0) {
			Sys_Printf ("win_set_palette() - SetDIBColorTable failed\n");
		}
	}
}

static void
dd_blit_rect (vrect_t *rect)
{
	RECT        TheRect;
	RECT        sRect, dRect;
	DDSURFACEDESC ddsd;

	memset (&ddsd, 0, sizeof (ddsd));
	ddsd.dwSize = sizeof (DDSURFACEDESC);

	// lock the correct subrect
	TheRect.left = rect->x;
	TheRect.right = rect->x + rect->width;
	TheRect.top = rect->y;
	TheRect.bottom = rect->y + rect->height;

	if (IDirectDrawSurface_Lock (win_dd_backbuffer, &TheRect, &ddsd,
								 DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR,
								 NULL) == DDERR_WASSTILLDRAWING) {
		return;
	}

	// convert pitch to 32-bit addressable
	ddsd.lPitch >>= 2;

	byte       *src = viddef.buffer + rect->y * viddef.rowbytes + rect->x;
	unsigned   *dst = ddsd.lpSurface;
	for (int y = rect->height; y-- > 0; ) {
		for (int x = rect->width; x-- > 0; ) {
			*dst++ = st2d_8to32table[*src++].value;
		}
		src += viddef.rowbytes - rect->width;
		dst += ddsd.lPitch - rect->width;
	}

	IDirectDrawSurface_Unlock (win_dd_backbuffer, NULL);

	// correctly offset source
	sRect.left = win_src_rect.left + rect->x;
	sRect.right = win_src_rect.left + rect->x + rect->width;
	sRect.top = win_src_rect.top + rect->y;
	sRect.bottom = win_src_rect.top + rect->y + rect->height;

	// correctly offset dest
	dRect.left = win_dst_rect.left + rect->x;
	dRect.right = win_dst_rect.left + rect->x + rect->width;
	dRect.top = win_dst_rect.top + rect->y;
	dRect.bottom = win_dst_rect.top + rect->y + rect->height;

	// copy to front buffer
	IDirectDrawSurface_Blt (win_dd_frontbuffer, &dRect, win_dd_backbuffer,
							&sRect, 0, NULL);
}

static void
win_sw_update (vrect_t *rects)
{
	vrect_t     full_rect;
	if (!win_palettized && palette_changed) {
		palette_changed = false;
		full_rect.x = 0;
		full_rect.y = 0;
		full_rect.width = viddef.width;
		full_rect.height = viddef.height;
		full_rect.next = 0;
		rects = &full_rect;
	}

	if (win_using_ddraw) {
		while (rects) {
			dd_blit_rect (rects);
			rects = rects->next;
		}
	} else if (win_dib_section) {
		while (rects) {
			BitBlt (win_gdi, rects->x, rects->y,
					rects->x + rects->width, rects->y + rects->height,
					win_dib_section, rects->x, rects->y, SRCCOPY);
			rects = rects->next;
		}
	}
}


static void
win_choose_visual (sw_ctx_t *ctx)
{
}

static void
win_set_background (void)
{
	// because we have set the background brush for the window to NULL (to
	// avoid flickering when re-sizing the window on the desktop), we clear
	// the window to black when created, otherwise it will be empty while
	// Quake starts up.  This also prevents a screen flash to white when
	// switching drivers.  it still flashes, but at least it's black now
	HDC         hdc = GetDC (win_mainwindow);
	PatBlt (hdc, 0, 0, win_window_rect.right, win_window_rect.bottom,
			BLACKNESS);
	ReleaseDC (win_mainwindow, hdc);
}

static void
win_create_context (sw_ctx_t *ctx)
{
	// shutdown any old driver that was active
	Win_UnloadAllDrivers ();

	win_sw_context = ctx;

	win_set_background ();

	// create the new driver
	win_using_ddraw = false;

	Win_CreateDriver ();

	viddef.vid_internal->do_screen_buffer = win_init_bufers;
	VID_InitBuffers ();
}

sw_ctx_t *
Win_SW_Context (void)
{
	sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
	ctx->set_palette = win_set_palette;
	ctx->choose_visual = win_choose_visual;
	ctx->create_context = win_create_context;
	ctx->update = win_sw_update;
	return ctx;
}

void
Win_SW_Init_Cvars (void)
{
}
