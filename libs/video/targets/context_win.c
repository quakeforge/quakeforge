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

#include "winquake.h"
#include <ddraw.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "context_win.h"
#include "r_shared.h"
#include "vid_internal.h"
#include "vid_sw.h"

HWND        win_mainwindow;
HDC         win_maindc;
HDC         win_dib_section;
int         win_using_ddraw;
int         win_palettized;
LPDIRECTDRAWSURFACE win_dd_frontbuffer;
LPDIRECTDRAWSURFACE win_dd_backbuffer;
RECT        win_src_rect;
RECT        win_dst_rect;
RECT        win_window_rect;
HDC         win_gdi;
int         win_canalttab = 0;
int         win_palettized;
sw_ctx_t   *win_sw_context;

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 3)

static cvar_t *vid_ddraw;

// Note that 0 is MODE_WINDOWED
static cvar_t *vid_mode;

// Note that 0 is MODE_WINDOWED
static cvar_t *_vid_default_mode;

// Note that 3 is MODE_FULLSCREEN_DEFAULT
static cvar_t *_vid_default_mode_win;
static cvar_t *vid_wait;
static cvar_t *vid_nopageflip;
static cvar_t *_vid_wait_override;
static cvar_t *vid_config_x;
static cvar_t *vid_config_y;
static cvar_t *vid_stretch_by_2;
static cvar_t *_windowed_mouse;
static cvar_t *vid_fullscreen_mode;
static cvar_t *vid_windowed_mode;
static cvar_t *block_switch;
static cvar_t *vid_window_x;
static cvar_t *vid_window_y;

//FIXME?int yeahimconsoled;

#define MAX_MODE_LIST	36
#define VID_ROW_SIZE	3

extern qboolean Minimized;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

int         DIBWidth, DIBHeight;
RECT        WindowRect;
DWORD       WindowStyle, ExWindowStyle;

int         window_center_x, window_center_y, window_width, window_height;
RECT        window_rect;

DEVMODE     win_gdevmode;
static qboolean startwindowed = 0, windowed_mode_set;
static qboolean vid_palettized;
static int  vid_fulldib_on_focus_mode;
static qboolean force_minimized, in_mode_set, force_mode_set;
static qboolean vid_mode_set;
static HICON hIcon;


int         vid_modenum = NO_MODE;
int         vid_testingmode, vid_realmode;
double      vid_testendtime;
int         vid_default = MODE_WINDOWED;
static int  windowed_default;

modestate_t modestate = MS_UNINIT;

byte        vid_curpal[256 * 3];

unsigned short d_8to16table[256];

int         mode;

typedef struct {
	modestate_t type;
	int         width;
	int         height;
	int         modenum;
	int         fullscreen;
	char        modedesc[13];
} vmode_t;

static vmode_t modelist[MAX_MODE_LIST];
static int  nummodes;

int         aPage;						// Current active display page
int         vPage;						// Current visible display page
int         waitVRT = true;				// True to wait for retrace on flip

static vmode_t badmode = {
	.modedesc = "Bad mode",
};

/*
=============================================================================

				DIRECTDRAW VIDEO DRIVER

=============================================================================
*/

LPDIRECTDRAW dd_Object = NULL;
HINSTANCE   hInstDDraw = NULL;

LPDIRECTDRAWCLIPPER dd_Clipper = NULL;

typedef     HRESULT (WINAPI *ddCreateProc_t) (GUID FAR *,
													 LPDIRECTDRAW FAR *,
													 IUnknown FAR *);
ddCreateProc_t ddCreate = NULL;

unsigned    ddpal[256];

byte       *vidbuf = NULL;


int         dd_window_width = 640;
int         dd_window_height = 480;

static void
DD_UpdateRects (int width, int height)
{
	POINT       p = { .x = 0, .y = 0 };
	// first we need to figure out where on the primary surface our window
	// lives
	ClientToScreen (win_mainwindow, &p);
	GetClientRect (win_mainwindow, &win_dst_rect);
	OffsetRect (&win_dst_rect, p.x, p.y);
	SetRect (&win_src_rect, 0, 0, width, height);
}


static void
VID_CreateDDrawDriver (int width, int height, const byte *palette,
					   void **buffer, int *rowbytes)
{
	DDSURFACEDESC ddsd;

	win_using_ddraw = false;
	dd_window_width = width;
	dd_window_height = height;

	vidbuf = (byte *) malloc (width * height);
	buffer[0] = vidbuf;
	rowbytes[0] = width;

	if (!(hInstDDraw = LoadLibrary ("ddraw.dll"))) {
		return;
	}
	if (!(ddCreate = (ddCreateProc_t) GetProcAddress (hInstDDraw,
													  "DirectDrawCreate"))) {
		return;
	}

	if (FAILED (ddCreate (NULL, &dd_Object, NULL))) {
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
												  &win_dd_frontbuffer, NULL))) {
		return;
	}

	// not using a clipper will slow things down and switch aero off
	if (FAILED (IDirectDraw_CreateClipper (dd_Object, 0, &dd_Clipper, NULL))) {
		return;
	}
	if (FAILED (IDirectDrawClipper_SetHWnd (dd_Clipper, 0, win_mainwindow))) {
		return;
	}
	if (FAILED (IDirectDrawSurface_SetClipper (win_dd_frontbuffer,
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
										   &win_dd_backbuffer, NULL))) {
		return;
	}

	// direct draw is working now
	win_using_ddraw = true;

	// create initial rects
	DD_UpdateRects (dd_window_width, dd_window_height);
}


/*
=====================================================================

				GDI VIDEO DRIVER

=====================================================================
*/

// common bitmap definition
typedef struct dibinfo {
	BITMAPINFOHEADER header;
	RGBQUAD     acolors[256];
} dibinfo_t;


static HGDIOBJ previously_selected_GDI_obj = NULL;
HBITMAP     hDIBSection;
byte       *pDIBBase = NULL;
HDC         hdcDIBSection = NULL;
HDC         hdcGDI = NULL;


static void
VID_CreateGDIDriver (int width, int height, const byte *palette, void **buffer,
					 int *rowbytes)
{
	dibinfo_t   dibheader;
	BITMAPINFO *pbmiDIB = (BITMAPINFO *) & dibheader;
	int         i;

	hdcGDI = GetDC (win_mainwindow);
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
	hDIBSection = CreateDIBSection (hdcGDI,
									pbmiDIB,
									DIB_RGB_COLORS,
									(void **) &pDIBBase, NULL, 0);

	// set video buffers
	if (pbmiDIB->bmiHeader.biHeight > 0) {
		// bottom up
		buffer[0] = pDIBBase + (height - 1) * width;
		rowbytes[0] = -width;
	} else {
		// top down
		buffer[0] = pDIBBase;
		rowbytes[0] = width;
	}

	// clear the buffer
	memset (pDIBBase, 0xff, width * height);

	if ((hdcDIBSection = CreateCompatibleDC (hdcGDI)) == NULL)
		Sys_Error ("DIB_Init() - CreateCompatibleDC failed\n");

	if ((previously_selected_GDI_obj =
		 SelectObject (hdcDIBSection, hDIBSection)) == NULL)
		Sys_Error ("DIB_Init() - SelectObject failed\n");

	// create a palette
	VID_InitGamma (palette);
	viddef.vid_internal->set_palette (palette);
}

void
Win_CreateDriver (void)
{
	if (vid_ddraw->int_val) {
		VID_CreateDDrawDriver (DIBWidth, DIBHeight, viddef.palette,
							   &viddef.buffer, &viddef.rowbytes);
	}
	if (!win_using_ddraw) {
		// directdraw failed or was not requested
		//
		// if directdraw failed, it may be partially initialized, so make sure
		// the slate is clean
		Win_UnloadAllDrivers ();

		VID_CreateGDIDriver (DIBWidth, DIBHeight, viddef.palette,
							 &viddef.buffer, &viddef.rowbytes);
	}
}

void
Win_UnloadAllDrivers (void)
{
	// shut down ddraw
	if (vidbuf) {
		free (vidbuf);
		vidbuf = NULL;
	}

	if (dd_Clipper) {
		IDirectDrawClipper_Release (dd_Clipper);
		dd_Clipper = NULL;
	}

	if (win_dd_frontbuffer) {
		IDirectDrawSurface_Release (win_dd_frontbuffer);
		win_dd_frontbuffer = NULL;
	}

	if (win_dd_backbuffer) {
		IDirectDrawSurface_Release (win_dd_backbuffer);
		win_dd_backbuffer = NULL;
	}

	if (dd_Object) {
		IDirectDraw_Release (dd_Object);
		dd_Object = NULL;
	}

	if (hInstDDraw) {
		FreeLibrary (hInstDDraw);
		hInstDDraw = NULL;
	}

	ddCreate = NULL;

	// shut down gdi
	if (hdcDIBSection) {
		SelectObject (hdcDIBSection, previously_selected_GDI_obj);
		DeleteDC (hdcDIBSection);
		hdcDIBSection = NULL;
	}

	if (hDIBSection) {
		DeleteObject (hDIBSection);
		hDIBSection = NULL;
		pDIBBase = NULL;
	}

	if (hdcGDI) {
		// if hdcGDI exists then win_mainwindow must also be valid
		ReleaseDC (win_mainwindow, hdcGDI);
		hdcGDI = NULL;
	}
	// not using ddraw now
	win_using_ddraw = false;
}



// compatibility
qboolean    DDActive;

void        VID_MenuDraw (void);
void        VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void        AppActivate (BOOL fActive, BOOL minimize);

static int  VID_SetMode (int modenum, const byte *palette);


/*
================
VID_RememberWindowPos
================
*/
static void __attribute__ ((used))
VID_RememberWindowPos (void)
{
	RECT        rect;

	if (GetWindowRect (win_mainwindow, &rect)) {
		if ((rect.left < GetSystemMetrics (SM_CXSCREEN)) &&
			(rect.top < GetSystemMetrics (SM_CYSCREEN)) &&
			(rect.right > 0) && (rect.bottom > 0)) {
			Cvar_SetValue (vid_window_x, (float) rect.left);
			Cvar_SetValue (vid_window_y, (float) rect.top);
		}
	}
}


/*
================
VID_CheckWindowXY
================
*/
static void
VID_CheckWindowXY (void)
{
	if ((vid_window_x->int_val > (GetSystemMetrics (SM_CXSCREEN) - 160)) ||
		(vid_window_y->int_val > (GetSystemMetrics (SM_CYSCREEN) - 120)) ||
		(vid_window_x->int_val < 0) || (vid_window_y->int_val < 0)) {
		Cvar_SetValue (vid_window_x, 0.0);
		Cvar_SetValue (vid_window_y, 0.0);
	}
}


/*
================
VID_UpdateWindowStatus
================
*/
void
Win_UpdateWindowStatus (int window_x, int window_y)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;
	IN_UpdateClipCursor ();
}


/*
================
ClearAllStates
================
*/
static void
ClearAllStates (void)
{
	int         i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++) {
		Key_Event (i, 0, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}


/*
================
VID_CheckAdequateMem
================
*/
static qboolean
VID_CheckAdequateMem (int width, int height)
{
	// there will always be enough ;)
	return true;
}


static void
VID_InitModes (HINSTANCE hInstance)
{
	WNDCLASS    wc;
	HDC         hdc;

//FIXME hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON2));

	/* Register the frame class */
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "WinQuake";

	if (!RegisterClass (&wc))
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;
	modelist[0].width = 320;
	modelist[0].height = 240;
	strcpy (modelist[0].modedesc, "320x240");
	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].fullscreen = 0;

	modelist[1].type = MS_WINDOWED;
	modelist[1].width = 640;
	modelist[1].height = 480;
	strcpy (modelist[1].modedesc, "640x480");
	modelist[1].modenum = MODE_WINDOWED + 1;
	modelist[1].fullscreen = 0;

	modelist[2].type = MS_WINDOWED;
	modelist[2].width = 800;
	modelist[2].height = 600;
	strcpy (modelist[2].modedesc, "800x600");
	modelist[2].modenum = MODE_WINDOWED + 2;
	modelist[2].fullscreen = 0;

	// automatically stretch the default mode up if > 640x480 desktop
	// resolution
	hdc = GetDC (NULL);

	if ((GetDeviceCaps (hdc, HORZRES) > 800)
		&& !COM_CheckParm ("-noautostretch")) {
		vid_default = MODE_WINDOWED + 2;
	} else if ((GetDeviceCaps (hdc, HORZRES) > 640)
			   && !COM_CheckParm ("-noautostretch")) {
		vid_default = MODE_WINDOWED + 1;
	} else {
		vid_default = MODE_WINDOWED;
	}

	// always start at the lowest mode then switch to the higher one if
	// selected
	vid_default = MODE_WINDOWED;

	windowed_default = vid_default;
	ReleaseDC (NULL, hdc);
	nummodes = 3;						// reserve space for windowed mode
}


/*
=================
VID_GetDisplayModes
=================
*/
static void
VID_GetDisplayModes (void)
{
	DEVMODE     devmode;
	int         i, modenum, existingmode, originalnummodes, lowestres;
	BOOL        stat;

	// enumerate > 8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;
	lowestres = 99999;

	do {
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmPelsWidth <= MAXWIDTH)
			&& (devmode.dmPelsHeight <= MAXHEIGHT)
			&& (devmode.dmPelsWidth >= 320)
			&& (devmode.dmPelsHeight >= 240)
			&& (nummodes < MAX_MODE_LIST)) {
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
				DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].fullscreen = 1;
				sprintf (modelist[nummodes].modedesc, "%dx%d",
						 (int) devmode.dmPelsWidth,
						 (int) devmode.dmPelsHeight);

				// see is the mode already there
				// (same dimensions but different refresh rate)
				for (i = originalnummodes, existingmode = 0;
					 i < nummodes; i++) {
					if ((modelist[nummodes].width == modelist[i].width)
						&& (modelist[nummodes].height == modelist[i].height)) {
						existingmode = 1;
						break;
					}
				}

				// if it's not add it to the list
				if (!existingmode) {
					if (modelist[nummodes].width < lowestres)
						lowestres = modelist[nummodes].width;

					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

	if (nummodes != originalnummodes)
		vid_default = MODE_FULLSCREEN_DEFAULT;
	else
		Sys_Printf ("No fullscreen DIB modes found\n");
}

void
Win_OpenDisplay (void)
{
	VID_InitModes (global_hInstance);
	VID_GetDisplayModes ();

	vid_testingmode = 0;

	// if (COM_CheckParm("-startwindowed"))
	{
		startwindowed = 1;
		vid_default = windowed_default;
	}

//FIXME?    if (hwnd_dialog)
//FIXME?        DestroyWindow (hwnd_dialog);

	// sound initialization has to go here, preceded by a windowed mode set,
	// so there's a window for DirectSound to work with but we're not yet
	// fullscreen so the "hardware already in use" dialog is visible if it
	// gets displayed
	// keep the window minimized until we're ready for the first real mode set
	win_mainwindow = CreateWindowEx (ExWindowStyle,
								   "WinQuake",
								   "WinQuake",
								   WindowStyle,
								   0, 0,
								   WindowRect.right - WindowRect.left,
								   WindowRect.bottom - WindowRect.top,
								   NULL, NULL, global_hInstance, NULL);

	if (!win_mainwindow)
		Sys_Error ("Couldn't create DIB window");

	// done
	vid_mode_set = true;
//FIXME if (firsttime) S_Init ();
}

void
Win_CloseDisplay (void)
{
	if (viddef.initialized) {
		if (modestate == MS_FULLDIB) {
			ChangeDisplaySettings (NULL, CDS_FULLSCREEN);
		}

		PostMessage (HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM) win_mainwindow,
					 (LPARAM) 0);
		PostMessage (HWND_BROADCAST, WM_SYSCOLORCHANGE, (WPARAM) 0, (LPARAM) 0);
		AppActivate (false, false);

//FIXME?        if (hwnd_dialog) DestroyWindow (hwnd_dialog);
		if (win_mainwindow)
			DestroyWindow (win_mainwindow);

		vid_testingmode = 0;
		viddef.initialized = false;
	}
}

void
Win_SetVidMode (int width, int height, const byte *palette)
{
//FIXME SCR_StretchInit();

	force_mode_set = true;
	VID_SetMode (vid_default, palette);
	force_mode_set = false;
	vid_realmode = vid_modenum;
}

static void
VID_DestroyWindow (void)
{
	if (modestate == MS_FULLDIB)
		ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

	Win_UnloadAllDrivers ();
}

static void
VID_CheckModedescFixup (int mode)
{
}

static qboolean
VID_SetWindowedMode (int modenum)
{
	if (!windowed_mode_set) {
		if (COM_CheckParm ("-resetwinpos")) {
			Cvar_SetValue (vid_window_x, 0.0);
			Cvar_SetValue (vid_window_y, 0.0);
		}

		windowed_mode_set = true;
	}

	VID_CheckModedescFixup (modenum);
	VID_DestroyWindow ();

	WindowRect.top = WindowRect.left = 0;
	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;
	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX |
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPSIBLINGS |
		WS_CLIPCHILDREN | WS_THICKFRAME;

	// WindowStyle = WS_OVERLAPPEDWINDOW|WS_VISIBLE;
	ExWindowStyle = 0;
	AdjustWindowRectEx (&WindowRect, WindowStyle, FALSE, 0);

	// the first time we're called to set the mode, create the window we'll use
	// for the rest of the session
	if (!vid_mode_set) {
	} else {
		SetWindowLong (win_mainwindow, GWL_STYLE, WindowStyle | WS_VISIBLE);
		SetWindowLong (win_mainwindow, GWL_EXSTYLE, ExWindowStyle);
	}

	if (!SetWindowPos (win_mainwindow,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER | SWP_HIDEWINDOW)) {
		Sys_Error ("Couldn't resize DIB window");
	}

	// position and show the DIB window
	VID_CheckWindowXY ();
	SetWindowPos (win_mainwindow, NULL, vid_window_x->int_val,
				  vid_window_y->int_val, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	if (force_minimized)
		ShowWindow (win_mainwindow, SW_MINIMIZE);
	else
		ShowWindow (win_mainwindow, SW_SHOWDEFAULT);

	UpdateWindow (win_mainwindow);
	modestate = MS_WINDOWED;
	vid_fulldib_on_focus_mode = 0;

	viddef.numpages = 1;

//  viddef.height = viddef.conheight = DIBHeight;
//  viddef.width = viddef.conwidth = DIBWidth;

	viddef.height = viddef.conheight = DIBHeight;
	viddef.width = viddef.conwidth = DIBWidth;
//FIXME?    if (!yeahimconsoled){
//FIXME?        viddef.vconheight = DIBHeight;
//FIXME?        viddef.vconwidth = DIBWidth;
//FIXME?    }
	SendMessage (win_mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (win_mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}


static qboolean
VID_SetFullDIBMode (int modenum)
{
	VID_DestroyWindow ();

	win_gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	win_gdevmode.dmPelsWidth = modelist[modenum].width;
	win_gdevmode.dmPelsHeight = modelist[modenum].height;
	win_gdevmode.dmSize = sizeof (win_gdevmode);

	if (ChangeDisplaySettings (&win_gdevmode, CDS_FULLSCREEN) !=
		DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen DIB mode");

	modestate = MS_FULLDIB;
	vid_fulldib_on_focus_mode = modenum;
	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	ExWindowStyle = 0;

	AdjustWindowRectEx (&WindowRect, WindowStyle, FALSE, 0);

	SetWindowLong (win_mainwindow, GWL_STYLE, WindowStyle | WS_VISIBLE);
	SetWindowLong (win_mainwindow, GWL_EXSTYLE, ExWindowStyle);

	if (!SetWindowPos (win_mainwindow,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER)) {
		Sys_Error ("Couldn't resize DIB window");
	}
	// position and show the DIB window
	SetWindowPos (win_mainwindow, HWND_TOPMOST, 0, 0, 0, 0,
				  SWP_NOSIZE | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	ShowWindow (win_mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (win_mainwindow);

	viddef.numpages = 1;

#ifdef SCALED2D
	viddef.height = viddef.conheight = DIBHeight;
	viddef.width = viddef.conwidth = DIBWidth;
	// viddef.vconwidth = 320;
	// viddef.vconheight = 200;
//FIXME?    if (!yeahimconsoled){
//FIXME?        viddef.vconheight = DIBHeight;
//FIXME?        viddef.vconwidth = DIBWidth;
//FIXME?    }
#else
	viddef.height = viddef.conheight = DIBHeight;
	viddef.width = viddef.conwidth = DIBWidth;
#endif

	return true;
}

static void
VID_RestoreOldMode (int original_mode)
{
	static qboolean inerror = false;

	if (inerror)
		return;

	in_mode_set = false;
	inerror = true;
	// make sure mode set happens (video mode changes)
	vid_modenum = original_mode - 1;

	if (!VID_SetMode (original_mode, vid_curpal)) {
		vid_modenum = MODE_WINDOWED - 1;

		if (!VID_SetMode (windowed_default, vid_curpal))
			Sys_Error ("Can't set any video mode");
	}

	inerror = false;
}


static void __attribute__ ((used))
VID_SetDefaultMode (void)
{
	if (viddef.initialized)
		VID_SetMode (0, vid_curpal);

	IN_DeactivateMouse ();
}

static vmode_t *
VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}

static char *
VID_GetModeDescription (int mode)
{
	char       *pinfo;
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;
	return pinfo;
}

static int
VID_SetMode (int modenum, const byte *palette)
{
	int         original_mode;			// FIXME, temp;
	qboolean    stat;
	MSG         msg;
	HDC         hdc;

	while ((modenum >= nummodes) || (modenum < 0)) {
		if (vid_modenum == NO_MODE) {
			if (modenum == vid_default) {
				modenum = windowed_default;
			} else {
				modenum = vid_default;
			}

			Cvar_SetValue (vid_mode, (float) modenum);
		} else {
			Cvar_SetValue (vid_mode, (float) vid_modenum);
			return 0;
		}
	}

	if (!force_mode_set && (modenum == vid_modenum))
		return true;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
//FIXME?    temp = scr_disabled_for_loading;
//FIXME?    scr_disabled_for_loading = true;
	in_mode_set = true;
//FIXME CDAudio_Pause ();
//FIXME S_ClearBuffer ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED) {
		if (_windowed_mouse->int_val) {
			stat = VID_SetWindowedMode (modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		} else {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode (modenum);
		}
	} else {
		stat = VID_SetFullDIBMode (modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}

	window_width = viddef.width;
	window_height = viddef.height;


	Win_UpdateWindowStatus (0, 0);		// FIXME right numbers?
//FIXME CDAudio_Resume ();
//FIXME?    scr_disabled_for_loading = temp;

	if (!stat) {
		VID_RestoreOldMode (original_mode);
		return false;
	}

	// now we try to make sure we get the focus on the mode switch, because
	// sometimes in some systems we don't.  We grab the foreground, then
	// finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put
	// ourselves at the top of the z order, then grab the foreground again,
	// Who knows if it helps, but it probably doesn't hurt
	if (!force_minimized)
		SetForegroundWindow (win_mainwindow);

	hdc = GetDC (NULL);
	if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
		vid_palettized = true;
	} else {
		vid_palettized = false;
	}
	ReleaseDC (NULL, hdc);

	vid_modenum = modenum;
	Cvar_SetValue (vid_mode, (float) vid_modenum);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	if (!force_minimized) {
		SetWindowPos (win_mainwindow, HWND_TOP, 0, 0, 0, 0,
					  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
					  SWP_NOCOPYBITS);

		SetForegroundWindow (win_mainwindow);
	}
	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Sys_Printf ("%s\n", VID_GetModeDescription (vid_modenum));

	in_mode_set = false;

	viddef.recalc_refdef = 1;

//FIXME SCR_StretchInit();
//FIXME SCR_StretchRefresh();
//FIXME SCR_CvarCheck();
	return true;
}




//==========================================================================


/*
================
VID_HandlePause
================
*/
static void __attribute__ ((used))
VID_HandlePause (qboolean pause)
{
	if ((modestate == MS_WINDOWED) && _windowed_mouse->int_val) {
		if (pause) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		} else {
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

typedef struct {
	int         modenum;
	char       *desc;
	int         iscur;
	int         width;
} modedesc_t;

#define MAX_COLUMN_SIZE		5
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 6)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

//static modedesc_t modedescs[MAX_MODEDESCS];

static int
VID_NumModes (void)
{
	return nummodes;
}

static char * __attribute__((used))
VID_GetModeDescriptionMemCheck (int mode)
{
	char       *pinfo;
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;

	if (VID_CheckAdequateMem (pv->width, pv->height)) {
		return pinfo;
	} else {
		return NULL;
	}
}


// Tacks on "windowed" or "fullscreen"
static const char * __attribute__((used))
VID_GetModeDescription2 (int mode)
{
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLSCREEN) {
		return va (0, "%s fullscreen", pv->modedesc);
	} else if (modelist[mode].type == MS_FULLDIB) {
		return va (0, "%s fullscreen", pv->modedesc);
	} else {
		return va (0, "%s windowed", pv->modedesc);
	}
}


// KJB: Added this to return the mode driver name in description for console
static char *
VID_GetExtModeDescription (int mode)
{
	static char pinfo[40];
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLDIB) {
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	} else {
		sprintf (pinfo, "%s windowed", pv->modedesc);
	}

	return pinfo;
}
static void
VID_DescribeCurrentMode_f (void)
{
	Sys_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}

static void
VID_NumModes_f (void)
{
	if (nummodes == 1)
		Sys_Printf ("%d video mode is available\n", nummodes);
	else
		Sys_Printf ("%d video modes are available\n", nummodes);
}


static void
VID_DescribeMode_f (void)
{
	int         modenum;

	modenum = atoi (Cmd_Argv (1));
	Sys_Printf ("%s\n", VID_GetExtModeDescription (modenum));
}


static void
VID_DescribeModes_f (void)
{
	int         i, lnummodes;
	char       *pinfo;
	qboolean    na;
	vmode_t    *pv;

	na = false;
	lnummodes = VID_NumModes ();

	for (i = 0; i < lnummodes; i++) {
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);

		if (VID_CheckAdequateMem (pv->width, pv->height)) {
			Sys_Printf ("%2d: %s\n", i, pinfo);
		} else {
			Sys_Printf ("**: %s\n", pinfo);
			na = true;
		}
	}

	if (na) {
		Sys_Printf ("\n[**: not enough system RAM for mode]\n");
	}
}

static void
VID_TestMode_f (void)
{
	int         modenum;
	double      testduration;

	if (!vid_testingmode) {
		modenum = atoi (Cmd_Argv (1));

		if (VID_SetMode (modenum, vid_curpal)) {
			vid_testingmode = 1;
			testduration = atof (Cmd_Argv (2));

			if (testduration == 0)
				testduration = 5.0;

			vid_testendtime = Sys_DoubleTime () + testduration;
		}
	}
}

static void
VID_Windowed_f (void)
{
	VID_SetMode (vid_windowed_mode->int_val, vid_curpal);
}

static void
VID_Fullscreen_f (void)
{
	VID_SetMode (vid_fullscreen_mode->int_val, vid_curpal);
}

static void
VID_Minimize_f (void)
{
	// we only support minimizing windows; if you're fullscreen,
	// switch to windowed first
	if (modestate == MS_WINDOWED)
		ShowWindow (win_mainwindow, SW_MINIMIZE);
}

static void
VID_ForceMode_f (void)
{
	int         modenum;

	if (!vid_testingmode) {
		modenum = atoi (Cmd_Argv (1));
		force_mode_set = 1;
		VID_SetMode (modenum, vid_curpal);
		force_mode_set = 0;
	}
}

void
Win_SetCaption (const char *text)
{
	if (win_mainwindow) {
		SetWindowText (win_mainwindow, text);
	}
}

//static WORD systemgammaramps[3][256];
static WORD currentgammaramps[3][256];

qboolean
Win_SetGamma (double gamma)
{
	int         i;
	HDC         hdc = GetDC (NULL);

	for (i = 0; i < 256; i++) {
		currentgammaramps[2][i] = currentgammaramps[1][i] =
			currentgammaramps[0][i] = viddef.gammatable[i] * 256;
	}

	i = SetDeviceGammaRamp (hdc, &currentgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
	return i;
}

#if 0
static void
VID_SaveGamma (void)
{
	HDC         hdc = GetDC (NULL);

	GetDeviceGammaRamp (hdc, &systemgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
}

static void
VID_RestoreGamma (void)
{
	HDC         hdc = GetDC (NULL);

	SetDeviceGammaRamp (hdc, &systemgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
}
#endif

#define CVAR_ORIGINAL CVAR_NONE			// FIXME
void
Win_Init_Cvars (void)
{
	vid_ddraw = Cvar_Get ("vid_ddraw", "1", CVAR_ORIGINAL, 0, "");
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_ORIGINAL, 0, "");
	vid_wait = Cvar_Get ("vid_wait", "0", CVAR_ORIGINAL, 0, "");
	vid_nopageflip =
		Cvar_Get ("vid_nopageflip", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	_vid_wait_override =
		Cvar_Get ("_vid_wait_override", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0,
				  "");
	_vid_default_mode =
		Cvar_Get ("_vid_default_mode", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0,
				  "");
	_vid_default_mode_win =
		Cvar_Get ("_vid_default_mode_win", "3", CVAR_ARCHIVE | CVAR_ORIGINAL, 0,
				  "");
	vid_config_x =
		Cvar_Get ("vid_config_x", "800", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	vid_config_y =
		Cvar_Get ("vid_config_y", "600", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	vid_stretch_by_2 =
		Cvar_Get ("vid_stretch_by_2", "1", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	_windowed_mouse =
		Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	vid_fullscreen_mode =
		Cvar_Get ("vid_fullscreen_mode", "3", CVAR_ARCHIVE | CVAR_ORIGINAL, 0,
				  "");
	vid_windowed_mode =
		Cvar_Get ("vid_windowed_mode", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0,
				  "");
	block_switch =
		Cvar_Get ("block_switch", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	vid_window_x =
		Cvar_Get ("vid_window_x", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");
	vid_window_y =
		Cvar_Get ("vid_window_y", "0", CVAR_ARCHIVE | CVAR_ORIGINAL, 0, "");

	Cmd_AddCommand ("vid_testmode", VID_TestMode_f, "");
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f, "");
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f, "");
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f, "");
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f, "");
	Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f, "");
	Cmd_AddCommand ("vid_windowed", VID_Windowed_f, "");
	Cmd_AddCommand ("vid_fullscreen", VID_Fullscreen_f, "");
	Cmd_AddCommand ("vid_minimize", VID_Minimize_f, "");
}
