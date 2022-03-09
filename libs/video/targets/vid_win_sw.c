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

static cvar_t     *vid_bitdepth;

static LPDIRECTDRAW dd_Object;
static HINSTANCE   hInstDDraw;
static LPDIRECTDRAWSURFACE dd_frontbuffer;
static LPDIRECTDRAWSURFACE dd_backbuffer;
static RECT        src_rect;
static RECT        dst_rect;
static HDC         win_gdi;
static HDC         dib_section;
static HBITMAP     dib_bitmap;
static HGDIOBJ     previous_dib;
static int         using_ddraw;

static LPDIRECTDRAWCLIPPER dd_Clipper;

typedef     HRESULT (WINAPI *ddCreateProc_t) (GUID FAR *,
													 LPDIRECTDRAW FAR *,
													 IUnknown FAR *);
static ddCreateProc_t ddCreate;

static int         dd_window_width = 640;
static int         dd_window_height = 480;

static void
DD_UpdateRects (int width, int height)
{
	POINT       p = { .x = 0, .y = 0 };
	// first we need to figure out where on the primary surface our window
	// lives
	ClientToScreen (win_mainwindow, &p);
	GetClientRect (win_mainwindow, &dst_rect);
	OffsetRect (&dst_rect, p.x, p.y);
	SetRect (&src_rect, 0, 0, width, height);
}


static void
VID_CreateDDrawDriver (int width, int height)
{
	DDSURFACEDESC ddsd;

	using_ddraw = false;
	dd_window_width = width;
	dd_window_height = height;

	viddef.buffer = malloc (width * height);
	viddef.rowbytes = width;

	if (!(hInstDDraw = LoadLibrary ("ddraw.dll"))) {
		return;
	}
	if (!(ddCreate = (ddCreateProc_t) GetProcAddress (hInstDDraw,
													  "DirectDrawCreate"))) {
		return;
	}

	if (FAILED (ddCreate (0, &dd_Object, 0))) {
		return;
	}
	if (FAILED (dd_Object->lpVtbl->SetCooperativeLevel (dd_Object,
														win_mainwindow,
														DDSCL_NORMAL))) {
		return;
	}

	// the primary surface in windowed mode is the full screen
	memset (&ddsd, 0, sizeof (ddsd));
	ddsd.dwSize = sizeof (ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;

	// ...and create it
	if (FAILED (dd_Object->lpVtbl->CreateSurface (dd_Object, &ddsd,
												  &dd_frontbuffer, 0))) {
		return;
	}

	// not using a clipper will slow things down and switch aero off
	if (FAILED (IDirectDraw_CreateClipper (dd_Object, 0, &dd_Clipper, 0))) {
		return;
	}
	if (FAILED (IDirectDrawClipper_SetHWnd (dd_Clipper, 0, win_mainwindow))) {
		return;
	}
	if (FAILED (IDirectDrawSurface_SetClipper (dd_frontbuffer,
											   dd_Clipper))) {
		return;
	}

	// the secondary surface is an offscreen surface that is the currect
	// dimensions
	// this will be blitted to the correct location on the primary surface
	// (which is the full screen) during our draw op
	memset (&ddsd, 0, sizeof (ddsd));
	ddsd.dwSize = sizeof (ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;

	if (FAILED (IDirectDraw_CreateSurface (dd_Object, &ddsd,
										   &dd_backbuffer, 0))) {
		return;
	}

	// direct draw is working now
	using_ddraw = true;

	// create initial rects
	DD_UpdateRects (dd_window_width, dd_window_height);
}

static void
VID_CreateGDIDriver (int width, int height, const byte *palette, void **buffer,
					 int *rowbytes)
{
	// common bitmap definition
	typedef struct dibinfo {
		BITMAPINFOHEADER header;
		RGBQUAD     acolors[256];
	} dibinfo_t;

	dibinfo_t   dibheader;
	BITMAPINFO *pbmiDIB = (BITMAPINFO *) & dibheader;
	int         i;
	byte       *dib_base = 0;

	win_gdi = GetDC (win_mainwindow);
	memset (&dibheader, 0, sizeof (dibheader));

	// fill in the bitmap info
	pbmiDIB->bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	pbmiDIB->bmiHeader.biWidth = width;
	pbmiDIB->bmiHeader.biHeight = height;
	pbmiDIB->bmiHeader.biPlanes = 1;
	pbmiDIB->bmiHeader.biCompression = BI_RGB;
	pbmiDIB->bmiHeader.biSizeImage = 0;
	pbmiDIB->bmiHeader.biXPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biYPelsPerMeter = 0;
	pbmiDIB->bmiHeader.biClrUsed = 256;
	pbmiDIB->bmiHeader.biClrImportant = 256;
	pbmiDIB->bmiHeader.biBitCount = 8;

	// fill in the palette
	for (i = 0; i < 256; i++) {
		// d_8to24table isn't filled in yet so this is just for testing
		dibheader.acolors[i].rgbRed = palette[i * 3];
		dibheader.acolors[i].rgbGreen = palette[i * 3 + 1];
		dibheader.acolors[i].rgbBlue = palette[i * 3 + 2];
	}

	// create the DIB section
	dib_bitmap = CreateDIBSection (win_gdi,
								   pbmiDIB,
								   DIB_RGB_COLORS,
								   (void **) &dib_base, 0, 0);

	// set video buffers
	if (pbmiDIB->bmiHeader.biHeight > 0) {
		// bottom up
		buffer[0] = dib_base + (height - 1) * width;
		rowbytes[0] = -width;
	} else {
		// top down
		buffer[0] = dib_base;
		rowbytes[0] = width;
	}

	// clear the buffer
	memset (dib_base, 0xff, width * height);

	if ((dib_section = CreateCompatibleDC (win_gdi)) == 0)
		Sys_Error ("DIB_Init() - CreateCompatibleDC failed\n");

	if ((previous_dib = SelectObject (dib_section, dib_bitmap)) == 0)
		Sys_Error ("DIB_Init() - SelectObject failed\n");

	// create a palette
	VID_InitGamma (palette);
	viddef.vid_internal->set_palette (viddef.vid_internal->data, palette);
}

void
Win_UnloadAllDrivers (void)
{
	// shut down ddraw
	if (viddef.buffer) {
		free (viddef.buffer);
		*(int *)0xdb = 0;
		viddef.buffer = 0;
	}

	if (dd_Clipper) {
		IDirectDrawClipper_Release (dd_Clipper);
		dd_Clipper = 0;
	}

	if (dd_frontbuffer) {
		IDirectDrawSurface_Release (dd_frontbuffer);
		dd_frontbuffer = 0;
	}

	if (dd_backbuffer) {
		IDirectDrawSurface_Release (dd_backbuffer);
		dd_backbuffer = 0;
	}

	if (dd_Object) {
		IDirectDraw_Release (dd_Object);
		dd_Object = 0;
	}

	if (hInstDDraw) {
		FreeLibrary (hInstDDraw);
		hInstDDraw = 0;
	}

	ddCreate = 0;

	// shut down gdi
	if (dib_section) {
		SelectObject (dib_section, previous_dib);
		DeleteDC (dib_section);
		dib_section = 0;
	}

	if (dib_bitmap) {
		DeleteObject (dib_bitmap);
		dib_bitmap = 0;
	}

	if (win_gdi) {
		// if win_gdi exists then win_mainwindow must also be valid
		ReleaseDC (win_mainwindow, win_gdi);
		win_gdi = 0;
	}
	// not using ddraw now
	using_ddraw = false;
}

static void
Win_CreateDriver (void)
{
	if (vid_ddraw->int_val) {
		VID_CreateDDrawDriver (viddef.width, viddef.height);
	}
	if (!using_ddraw) {
		// directdraw failed or was not requested
		//
		// if directdraw failed, it may be partially initialized, so make sure
		// the slate is clean
		Win_UnloadAllDrivers ();

		VID_CreateGDIDriver (viddef.width, viddef.height, viddef.palette,
							 &viddef.buffer, &viddef.rowbytes);
	}
}

static void
win_init_bufers (void *data)
{
	Win_UnloadAllDrivers ();
	Win_CreateDriver ();
}

static void
win_set_palette (sw_ctx_t *ctx, const byte *palette)
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
	if (!win_minimized && !using_ddraw && dib_section) {
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

		if (SetDIBColorTable (dib_section, 0, 256, colors) == 0) {
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

	if (IDirectDrawSurface_Lock (dd_backbuffer, &TheRect, &ddsd,
								 DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR,
								 0) == DDERR_WASSTILLDRAWING) {
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

	IDirectDrawSurface_Unlock (dd_backbuffer, 0);

	// correctly offset source
	sRect.left = src_rect.left + rect->x;
	sRect.right = src_rect.left + rect->x + rect->width;
	sRect.top = src_rect.top + rect->y;
	sRect.bottom = src_rect.top + rect->y + rect->height;

	// correctly offset dest
	dRect.left = dst_rect.left + rect->x;
	dRect.right = dst_rect.left + rect->x + rect->width;
	dRect.top = dst_rect.top + rect->y;
	dRect.bottom = dst_rect.top + rect->y + rect->height;

	// copy to front buffer
	IDirectDrawSurface_Blt (dd_frontbuffer, &dRect, dd_backbuffer,
							&sRect, 0, 0);
}

static void
win_sw_update (sw_ctx_t *ctx, vrect_t *rects)
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

	if (using_ddraw) {
		while (rects) {
			dd_blit_rect (rects);
			rects = rects->next;
		}
	} else if (dib_section) {
		while (rects) {
			BitBlt (win_gdi, rects->x, rects->y,
					rects->x + rects->width, rects->y + rects->height,
					dib_section, rects->x, rects->y, SRCCOPY);
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
	// because we have set the background brush for the window to 0 (to
	// avoid flickering when re-sizing the window on the desktop), we clear
	// the window to black when created, otherwise it will be empty while
	// Quake starts up.  This also prevents a screen flash to white when
	// switching drivers.  it still flashes, but at least it's black now
	HDC         hdc = GetDC (win_mainwindow);
	PatBlt (hdc, 0, 0, win_rect.right, win_rect.bottom, BLACKNESS);
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
	using_ddraw = false;

	viddef.vid_internal->init_buffers = win_init_bufers;
}

sw_ctx_t *
Win_SW_Context (void)
{
	sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
	ctx->pixbytes = 1;
	ctx->set_palette = win_set_palette;
	ctx->choose_visual = win_choose_visual;
	ctx->create_context = win_create_context;
	ctx->update = win_sw_update;
#if 0	//FIXME need to figure out 16 and 32 bit buffers
	switch (vid_bitdepth->int_val) {
		case 8:
			ctx->pixbytes = 1;
			break;
		case 16:
			ctx->pixbytes = 2;
			break;
		case 32:
			ctx->pixbytes = 4;
			break;
		default:
			Sys_Error ("X11_SW32_Context: unsupported bit depth");
	}
#endif
	return ctx;
}

void
Win_SW_Init_Cvars (void)
{
	vid_bitdepth = Cvar_Get ("vid_bitdepth", "8", CVAR_ROM, NULL, "Sets "
							 "display bitdepth (supported modes: 8 16 32)");
}
