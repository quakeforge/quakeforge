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
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_vid.h"

#include "d_iface.h"
#include "d_local.h"
#include "in_win.h"
#include "r_cvar.h"
#include "r_shared.h"
#include "vid_internal.h"

// true if the ddraw driver started up OK
qboolean    vid_usingddraw = false;

// compatibility
HWND        mainwindow = NULL;
qboolean    win_canalttab = false;

// main application window
HWND        hWndWinQuake = NULL;
HDC         maindc;
HGLRC       baseRC;

//FIXME?int yeahimconsoled;

static void *libgl_handle;
static HGLRC (GLAPIENTRY * qfwglCreateContext) (HDC);
static BOOL (GLAPIENTRY * qfwglDeleteContext) (HGLRC);
static HGLRC (GLAPIENTRY * qfwglGetCurrentContext) (void);
static HDC (GLAPIENTRY * qfwglGetCurrentDC) (void);
static BOOL (GLAPIENTRY * qfwglMakeCurrent) (HDC, HGLRC);

static void *(WINAPI * glGetProcAddress) (const char *symbol) = NULL;

static void (*choose_visual) (void);
static void (*create_context) (const byte *palette);

static void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void       *glfunc = NULL;

	if (glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = GetProcAddress (handle, name);
	return glfunc;
}

static void *
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void       *glfunc = NULL;

	Sys_MaskPrintf (SYS_VID, "DEBUG: Finding symbol %s ... ", name);

	glfunc = QFGL_GetProcAddress (libgl_handle, name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_VID, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_VID, "not found\n");

	if (crit) {
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...",
				   name);
	}
	return NULL;
}

static void
GL_EndRendering (void)
{
	if (!scr_skipupdate) {
		qfglFinish ();
		SwapBuffers (maindc);
	}
	// handle the mouse state when windowed if that's changed
	if (!vid_fullscreen->int_val) {
		if (!in_grab->int_val) {
//FIXME         if (windowed_mouse) {
//FIXME             IN_DeactivateMouse ();
//FIXME             IN_ShowMouse ();
//FIXME             windowed_mouse = false;
//FIXME         }
		} else {
//FIXME         windowed_mouse = true;
		}
	}
}

static void
wgl_choose_visual (void)
{
}

static void
wgl_create_context (const byte *palette)
{
	DWORD       lasterror;

	Sys_Printf ("maindc: %p\n", maindc);
	baseRC = qfwglCreateContext (maindc);
	if (!baseRC) {
		lasterror=GetLastError();
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\n"
				   "Make sure you are in 65535 color mode, and try running "
				   "with -window.\n"
				   "Error code: (%lx)", lasterror);
	}

	if (!qfwglMakeCurrent (maindc, baseRC)) {
		lasterror = GetLastError ();
		if (baseRC)
			qfwglDeleteContext (baseRC);
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		Sys_Error ("wglMakeCurrent failed (%lx)", lasterror);
	}

	viddef.init_gl ();
}

static void
wgl_load_gl (void)
{
	choose_visual = wgl_choose_visual;
	create_context = wgl_create_context;

	viddef.get_proc_address = QFGL_ProcAddress;
	viddef.end_rendering = GL_EndRendering;

	if (!(libgl_handle = LoadLibrary (gl_driver->string)))
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);
	glGetProcAddress =
		(void *) GetProcAddress (libgl_handle, "wglGetProcAddress");

	qfwglCreateContext = QFGL_ProcAddress ("wglCreateContext", true);
	qfwglDeleteContext = QFGL_ProcAddress ("wglDeleteContext", true);
	qfwglGetCurrentContext = QFGL_ProcAddress ("wglGetCurrentContext", true);
	qfwglGetCurrentDC = QFGL_ProcAddress ("wglGetCurrentDC", true);
	qfwglMakeCurrent = QFGL_ProcAddress ("wglMakeCurrent", true);
}


/*
=============================================================================

				DIRECTDRAW VIDEO DRIVER

=============================================================================
*/

LPDIRECTDRAW dd_Object = NULL;
HINSTANCE   hInstDDraw = NULL;

LPDIRECTDRAWSURFACE dd_FrontBuffer = NULL;
LPDIRECTDRAWSURFACE dd_BackBuffer = NULL;

LPDIRECTDRAWCLIPPER dd_Clipper = NULL;

typedef     HRESULT (WINAPI * DIRECTDRAWCREATEPROC) (GUID FAR *,
													 LPDIRECTDRAW FAR *,
													 IUnknown FAR *);
DIRECTDRAWCREATEPROC QDirectDrawCreate = NULL;

unsigned    ddpal[256];

byte       *vidbuf = NULL;


int         dd_window_width = 640;
int         dd_window_height = 480;
RECT        SrcRect;
RECT        DstRect;

static void
DD_UpdateRects (int width, int height)
{
	POINT       p;

	p.x = 0;
	p.y = 0;

	// first we need to figure out where on the primary surface our window
	// lives
	ClientToScreen (hWndWinQuake, &p);
	GetClientRect (hWndWinQuake, &DstRect);
	OffsetRect (&DstRect, p.x, p.y);
	SetRect (&SrcRect, 0, 0, width, height);
}


static void
VID_CreateDDrawDriver (int width, int height, const byte *palette,
					   void **buffer, int *rowbytes)
{
	HRESULT     hr;
	DDSURFACEDESC ddsd;

	vid_usingddraw = false;
	dd_window_width = width;
	dd_window_height = height;

	vidbuf = (byte *) malloc (width * height);
	buffer[0] = vidbuf;
	rowbytes[0] = width;

	if (!(hInstDDraw = LoadLibrary ("ddraw.dll")))
		return;
	if (!(QDirectDrawCreate =
		  (DIRECTDRAWCREATEPROC) GetProcAddress (hInstDDraw,
												 "DirectDrawCreate")))
		return;

	if (FAILED (hr = QDirectDrawCreate (NULL, &dd_Object, NULL)))
		return;
	if (FAILED (hr = dd_Object->lpVtbl->SetCooperativeLevel (dd_Object,
															 hWndWinQuake,
															 DDSCL_NORMAL)))
		return;

	// the primary surface in windowed mode is the full screen
	memset (&ddsd, 0, sizeof (ddsd));
	ddsd.dwSize = sizeof (ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;

	// ...and create it
	if (FAILED (hr = dd_Object->lpVtbl->CreateSurface (dd_Object, &ddsd,
													   &dd_FrontBuffer, NULL)))
		return;

	// not using a clipper will slow things down and switch aero off
	if (FAILED (hr = IDirectDraw_CreateClipper (dd_Object, 0, &dd_Clipper,
												NULL)))
		return;
	if (FAILED (hr = IDirectDrawClipper_SetHWnd (dd_Clipper, 0, hWndWinQuake)))
		return;
	if (FAILED (hr = IDirectDrawSurface_SetClipper (dd_FrontBuffer,
													dd_Clipper)))
		return;

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

	if (FAILED (hr = IDirectDraw_CreateSurface (dd_Object, &ddsd,
												&dd_BackBuffer, NULL)))
		return;

	// direct draw is working now
	vid_usingddraw = true;

	// create a palette
	VID_InitGamma (palette);
	viddef.set_palette (palette);

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

	hdcGDI = GetDC (hWndWinQuake);
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
	viddef.set_palette (palette);
}


static void
VID_UnloadAllDrivers (void)
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

	if (dd_FrontBuffer) {
		IDirectDrawSurface_Release (dd_FrontBuffer);
		dd_FrontBuffer = NULL;
	}

	if (dd_BackBuffer) {
		IDirectDrawSurface_Release (dd_BackBuffer);
		dd_BackBuffer = NULL;
	}

	if (dd_Object) {
		IDirectDraw_Release (dd_Object);
		dd_Object = NULL;
	}

	if (hInstDDraw) {
		FreeLibrary (hInstDDraw);
		hInstDDraw = NULL;
	}

	QDirectDrawCreate = NULL;

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
		// if hdcGDI exists then hWndWinQuake must also be valid
		ReleaseDC (hWndWinQuake, hdcGDI);
		hdcGDI = NULL;
	}
	// not using ddraw now
	vid_usingddraw = false;
}



// compatibility
qboolean    DDActive;

// not used any more
void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}

//static int VID_ForceUnlockedAndReturnState (void) {return 0;}
void
VID_ForceLockState (int lk)
{
}


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
static int  firstupdate = 1;
static qboolean vid_initialized = false, vid_palettized;
static int  vid_fulldib_on_focus_mode;
static qboolean force_minimized, in_mode_set, force_mode_set;
static int  windowed_mouse;
static qboolean palette_changed, vid_mode_set;
static HICON hIcon;

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 3)

cvar_t     *vid_ddraw;

// Note that 0 is MODE_WINDOWED
cvar_t     *vid_mode;

// Note that 0 is MODE_WINDOWED
cvar_t     *_vid_default_mode;

// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t     *_vid_default_mode_win;
cvar_t     *vid_wait;
cvar_t     *vid_nopageflip;
cvar_t     *_vid_wait_override;
cvar_t     *vid_config_x;
cvar_t     *vid_config_y;
cvar_t     *vid_stretch_by_2;
cvar_t     *_windowed_mouse;
cvar_t     *vid_fullscreen_mode;
cvar_t     *vid_windowed_mode;
cvar_t     *block_switch;
cvar_t     *vid_window_x;
cvar_t     *vid_window_y;


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

static vmode_t badmode;

static byte backingbuf[48 * 24];

void        VID_MenuDraw (void);
void        VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void        AppActivate (BOOL fActive, BOOL minimize);

static int  VID_SetMode (int modenum, const byte *palette);

// video commands
int         VID_NumModes (void);
vmode_t    *VID_GetModePtr (int modenum);
void        VID_TestMode_f (void);
void        VID_ForceMode_f (void);
void        VID_Minimize_f (void);
void        VID_Fullscreen_f (void);
void        VID_Windowed_f (void);
void        VID_DescribeModes_f (void);
void        VID_DescribeMode_f (void);
void        VID_NumModes_f (void);
void        VID_DescribeCurrentMode_f (void);
char       *VID_GetExtModeDescription (int mode);
char       *VID_GetModeDescription (int mode);
char       *VID_GetModeDescription2 (int mode);
char       *VID_GetModeDescriptionMemCheck (int mode);
void        VID_CheckModedescFixup (int mode);


/*
================
VID_RememberWindowPos
================
*/
static void __attribute__ ((used))
VID_RememberWindowPos (void)
{
	RECT        rect;

	if (GetWindowRect (hWndWinQuake, &rect)) {
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
VID_UpdateWindowStatus (int window_x, int window_y)
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

static void
WIN_OpenDisplay (void)
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
	hWndWinQuake = CreateWindowEx (ExWindowStyle,
								   "WinQuake",
								   "WinQuake",
								   WindowStyle,
								   0, 0,
								   WindowRect.right - WindowRect.left,
								   WindowRect.bottom - WindowRect.top,
								   NULL, NULL, global_hInstance, NULL);

	if (!hWndWinQuake)
		Sys_Error ("Couldn't create DIB window");

	// compatibility
	mainwindow = hWndWinQuake;

	// done
	vid_mode_set = true;
//FIXME if (firsttime) S_Init ();
}

static void
WIN_SetVidMode (unsigned width, unsigned height, const byte *palette)
{
//FIXME SCR_StretchInit();

	force_mode_set = true;
	VID_SetMode (vid_default, palette);
	force_mode_set = false;
	vid_realmode = vid_modenum;
	strcpy (badmode.modedesc, "Bad mode");
}

static void
VID_DestroyWindow (void)
{
	if (modestate == MS_FULLDIB)
		ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

	VID_UnloadAllDrivers ();
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
		SetWindowLong (hWndWinQuake, GWL_STYLE, WindowStyle | WS_VISIBLE);
		SetWindowLong (hWndWinQuake, GWL_EXSTYLE, ExWindowStyle);
	}

	if (!SetWindowPos (hWndWinQuake,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER | SWP_HIDEWINDOW)) {
		Sys_Error ("Couldn't resize DIB window");
	}

	// position and show the DIB window
	VID_CheckWindowXY ();
	SetWindowPos (hWndWinQuake, NULL, vid_window_x->int_val,
				  vid_window_y->int_val, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	if (force_minimized)
		ShowWindow (hWndWinQuake, SW_MINIMIZE);
	else
		ShowWindow (hWndWinQuake, SW_SHOWDEFAULT);

	UpdateWindow (hWndWinQuake);
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
	SendMessage (hWndWinQuake, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (hWndWinQuake, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

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

	SetWindowLong (hWndWinQuake, GWL_STYLE, WindowStyle | WS_VISIBLE);
	SetWindowLong (hWndWinQuake, GWL_EXSTYLE, ExWindowStyle);

	if (!SetWindowPos (hWndWinQuake,
					   NULL,
					   0, 0,
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top,
					   SWP_NOCOPYBITS | SWP_NOZORDER)) {
		Sys_Error ("Couldn't resize DIB window");
	}
	// position and show the DIB window
	SetWindowPos (hWndWinQuake, HWND_TOPMOST, 0, 0, 0, 0,
				  SWP_NOSIZE | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	ShowWindow (hWndWinQuake, SW_SHOWDEFAULT);
	UpdateWindow (hWndWinQuake);

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
	if (vid_initialized)
		VID_SetMode (0, vid_curpal);

	IN_DeactivateMouse ();
}

static void
win_init_bufers (void)
{
	// set the rest of the buffers we need (why not just use one single buffer
	// instead of all this crap? oh well, it's Quake...)
	viddef.direct = (byte *) viddef.buffer;
	viddef.conbuffer = viddef.buffer;

	// more crap for the console
	viddef.conrowbytes = viddef.rowbytes;
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


	VID_UpdateWindowStatus (0, 0);		// FIXME right numbers?
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
		SetForegroundWindow (hWndWinQuake);

	hdc = GetDC (NULL);

	if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE)
		vid_palettized = true;
	else
		vid_palettized = false;

	viddef.set_palette (palette);
	ReleaseDC (NULL, hdc);
	vid_modenum = modenum;
	Cvar_SetValue (vid_mode, (float) vid_modenum);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	if (!force_minimized) {
		SetWindowPos (hWndWinQuake, HWND_TOP, 0, 0, 0, 0,
					  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
					  SWP_NOCOPYBITS);

		SetForegroundWindow (hWndWinQuake);
	}
	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Sys_Printf ("%s\n", VID_GetModeDescription (vid_modenum));

	viddef.set_palette (palette);

	in_mode_set = false;

	viddef.recalc_refdef = 1;

//FIXME SCR_StretchInit();
//FIXME SCR_StretchRefresh();
//FIXME SCR_CvarCheck();
	return true;
}


static void
VID_SetPalette (const byte *palette)
{
	int         i;
	const byte *pal = palette;

	if (!Minimized) {
		if (vid_usingddraw) {
			// incoming palette is 3 component
			for (i = 0; i < 256; i++, pal += 3) {
				PALETTEENTRY *p = (PALETTEENTRY *) & ddpal[i];

				p->peRed = viddef.gammatable[pal[2]];
				p->peGreen = viddef.gammatable[pal[1]];
				p->peBlue = viddef.gammatable[pal[0]];
				p->peFlags = 255;
			}
		} else {
			RGBQUAD     colors[256];

			if (hdcDIBSection) {
				// incoming palette is 3 component
				for (i = 0; i < 256; i++, pal += 3) {
					PALETTEENTRY *p = (PALETTEENTRY *) & ddpal[i];

					colors[i].rgbRed = viddef.gammatable[pal[0]];
					colors[i].rgbGreen = viddef.gammatable[pal[1]];
					colors[i].rgbBlue = viddef.gammatable[pal[2]];
					colors[i].rgbReserved = 0;

					p->peRed = viddef.gammatable[pal[2]];
					p->peGreen = viddef.gammatable[pal[1]];
					p->peBlue = viddef.gammatable[pal[0]];
					p->peFlags = 255;
				}

				colors[0].rgbRed = 0;
				colors[0].rgbGreen = 0;
				colors[0].rgbBlue = 0;
				colors[255].rgbRed = 0xff;
				colors[255].rgbGreen = 0xff;
				colors[255].rgbBlue = 0xff;

				if (SetDIBColorTable (hdcDIBSection, 0, 256, colors) == 0) {
					Sys_Printf ("DIB_SetPalette() - SetDIBColorTable failed\n");
				}
			}
		}
	}

	memcpy (vid_curpal, palette, sizeof (vid_curpal));
}


#define CVAR_ORIGINAL CVAR_NONE			// FIXME
void
VID_Init_Cvars (void)
{
	gl_driver = Cvar_Get ("gl_driver", GL_DRIVER, CVAR_ROM, NULL,
						  "The OpenGL library to use. (path optional)");
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
}

static void
win_choose_visual (void)
{
}

static void
win_create_context (const byte *palette)
{
	HDC         hdc;

	// shutdown any old driver that was active
	VID_UnloadAllDrivers ();

	// because we have set the background brush for the window to NULL (to
	// avoid flickering when re-sizing the window on the desktop), we clear
	// the window to black when created, otherwise it will be empty while
	// Quake starts up.  This also prevents a screen flash to white when
	// switching drivers.  it still flashes, but at least it's black now
	hdc = GetDC (hWndWinQuake);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC (hWndWinQuake, hdc);

	// create the new driver
	vid_usingddraw = false;

	// attempt to create a direct draw driver
	if (vid_ddraw->int_val)
		VID_CreateDDrawDriver (DIBWidth, DIBHeight, palette, &viddef.buffer,
							   &viddef.rowbytes);

	// create a gdi driver if directdraw failed or if we preferred not to use
	// it
	if (!vid_usingddraw) {
		// because directdraw may have been partially created we must shut it
		// down again first
		VID_UnloadAllDrivers ();

		// now create the gdi driver
		VID_CreateGDIDriver (DIBWidth, DIBHeight, palette, &viddef.buffer,
							 &viddef.rowbytes);
	}
	// if ddraw failed to come up we disable the cvar too
	if (vid_ddraw->int_val && !vid_usingddraw)
		Cvar_Set (vid_ddraw, "0");

	viddef.do_screen_buffer = win_init_bufers;
	VID_InitBuffers ();
}

void
VID_Init (byte *palette, byte *colormap)
{
	Sys_RegisterShutdown (VID_shutdown);

	choose_visual = win_choose_visual;
	create_context = win_create_context;

	R_LoadModule (wgl_load_gl, VID_SetPalette);

	viddef.numpages = 1;
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];

	VID_GetWindowSize (640, 480);
	WIN_OpenDisplay ();
	choose_visual ();
	WIN_SetVidMode (viddef.width, viddef.height, palette);
	create_context (palette);

	VID_InitGamma (palette);
	viddef.set_palette (palette);

	vid_initialized = true;
}

#if 0
Cmd_AddCommand ("vid_testmode", VID_TestMode_f, "");
Cmd_AddCommand ("vid_nummodes", VID_NumModes_f, "");
Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f, "");
Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f, "");
Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f, "");
Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f, "");
Cmd_AddCommand ("vid_windowed", VID_Windowed_f, "");
Cmd_AddCommand ("vid_fullscreen", VID_Fullscreen_f, "");
Cmd_AddCommand ("vid_minimize", VID_Minimize_f, "");
#endif


static void
VID_shutdown (void)
{
	if (vid_initialized) {
		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

		PostMessage (HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM) hWndWinQuake,
					 (LPARAM) 0);
		PostMessage (HWND_BROADCAST, WM_SYSCOLORCHANGE, (WPARAM) 0, (LPARAM) 0);
		AppActivate (false, false);

		VID_DestroyWindow ();

//FIXME?        if (hwnd_dialog) DestroyWindow (hwnd_dialog);
		if (hWndWinQuake)
			DestroyWindow (hWndWinQuake);

		vid_testingmode = 0;
		vid_initialized = 0;
	}
}


/*
================
FlipScreen
================
*/
static void
FlipScreen (vrect_t *rects)
{
	int         numrects = 0;

	while (rects) {
		if (vid_usingddraw) {
			int         x, y;
			HRESULT     hr = S_OK;
			byte       *src = NULL;
			unsigned   *dst = NULL;

			if (dd_BackBuffer) {
				RECT        TheRect;
				RECT        sRect, dRect;
				DDSURFACEDESC ddsd;

				memset (&ddsd, 0, sizeof (ddsd));
				ddsd.dwSize = sizeof (DDSURFACEDESC);

				// lock the correct subrect
				TheRect.left = rects->x;
				TheRect.right = rects->x + rects->width;
				TheRect.top = rects->y;
				TheRect.bottom = rects->y + rects->height;

				if ((hr =
					 IDirectDrawSurface_Lock (dd_BackBuffer, &TheRect, &ddsd,
											  DDLOCK_WRITEONLY |
											  DDLOCK_SURFACEMEMORYPTR,
											  NULL)) == DDERR_WASSTILLDRAWING)
					return;

				src = (byte *) vidbuf + rects->y * viddef.rowbytes + rects->x;
				dst = (unsigned *) ddsd.lpSurface;

				// convert pitch to unsigned int addressable
				ddsd.lPitch >>= 2;

				// because we created a 32 bit backbuffer we need to copy from
				// the 8 bit memory buffer to it before flipping
				if (!(rects->width & 15)) {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						byte       *psrc = src;
						unsigned   *pdst = dst;

						for (x = 0; x < rects->width;
							 x += 16, psrc += 16, pdst += 16) {
							pdst[0] = ddpal[psrc[0]];
							pdst[1] = ddpal[psrc[1]];
							pdst[2] = ddpal[psrc[2]];
							pdst[3] = ddpal[psrc[3]];

							pdst[4] = ddpal[psrc[4]];
							pdst[5] = ddpal[psrc[5]];
							pdst[6] = ddpal[psrc[6]];
							pdst[7] = ddpal[psrc[7]];

							pdst[8] = ddpal[psrc[8]];
							pdst[9] = ddpal[psrc[9]];
							pdst[10] = ddpal[psrc[10]];
							pdst[11] = ddpal[psrc[11]];

							pdst[12] = ddpal[psrc[12]];
							pdst[13] = ddpal[psrc[13]];
							pdst[14] = ddpal[psrc[14]];
							pdst[15] = ddpal[psrc[15]];
						}
					}
				} else if (!(rects->width % 10)) {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						byte       *psrc = src;
						unsigned   *pdst = dst;

						for (x = 0; x < rects->width;
							 x += 10, psrc += 10, pdst += 10) {
							pdst[0] = ddpal[psrc[0]];
							pdst[1] = ddpal[psrc[1]];
							pdst[2] = ddpal[psrc[2]];
							pdst[3] = ddpal[psrc[3]];
							pdst[4] = ddpal[psrc[4]];

							pdst[5] = ddpal[psrc[5]];
							pdst[6] = ddpal[psrc[6]];
							pdst[7] = ddpal[psrc[7]];
							pdst[8] = ddpal[psrc[8]];
							pdst[9] = ddpal[psrc[9]];
						}
					}
				} else if (!(rects->width & 7)) {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						byte       *psrc = src;
						unsigned   *pdst = dst;

						for (x = 0; x < rects->width;
							 x += 8, psrc += 8, pdst += 8) {
							pdst[0] = ddpal[psrc[0]];
							pdst[1] = ddpal[psrc[1]];
							pdst[2] = ddpal[psrc[2]];
							pdst[3] = ddpal[psrc[3]];

							pdst[4] = ddpal[psrc[4]];
							pdst[5] = ddpal[psrc[5]];
							pdst[6] = ddpal[psrc[6]];
							pdst[7] = ddpal[psrc[7]];
						}
					}
				} else if (!(rects->width % 5)) {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						byte       *psrc = src;
						unsigned   *pdst = dst;

						for (x = 0; x < rects->width;
							 x += 5, psrc += 5, pdst += 5) {
							pdst[0] = ddpal[psrc[0]];
							pdst[1] = ddpal[psrc[1]];
							pdst[2] = ddpal[psrc[2]];
							pdst[3] = ddpal[psrc[3]];
							pdst[4] = ddpal[psrc[4]];
						}
					}
				} else if (!(rects->width & 3)) {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						byte       *psrc = src;
						unsigned   *pdst = dst;

						for (x = 0; x < rects->width;
							 x += 4, psrc += 4, pdst += 4) {
							pdst[0] = ddpal[psrc[0]];
							pdst[1] = ddpal[psrc[1]];
							pdst[2] = ddpal[psrc[2]];
							pdst[3] = ddpal[psrc[3]];
						}
					}
				} else {
					for (y = 0; y < rects->height;
						 y++, src += viddef.rowbytes, dst += ddsd.lPitch) {
						for (x = 0; x < rects->width; x++) {
							dst[x] = ddpal[src[x]];
						}
					}
				}

				IDirectDrawSurface_Unlock (dd_BackBuffer, NULL);

				// correctly offset source
				sRect.left = SrcRect.left + rects->x;
				sRect.right = SrcRect.left + rects->x + rects->width;
				sRect.top = SrcRect.top + rects->y;
				sRect.bottom = SrcRect.top + rects->y + rects->height;

				// correctly offset dest
				dRect.left = DstRect.left + rects->x;
				dRect.right = DstRect.left + rects->x + rects->width;
				dRect.top = DstRect.top + rects->y;
				dRect.bottom = DstRect.top + rects->y + rects->height;

				// copy to front buffer
				IDirectDrawSurface_Blt (dd_FrontBuffer, &dRect, dd_BackBuffer,
										&sRect, 0, NULL);
			}
		} else if (hdcDIBSection) {
			BitBlt (hdcGDI, rects->x, rects->y,
					rects->x + rects->width, rects->y + rects->height,
					hdcDIBSection, rects->x, rects->y, SRCCOPY);
		}

		numrects++;
		rects = rects->next;
	}
}


void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int         i, j, reps, repshift;
	vrect_t     rect;

	if (!vid_initialized)
		return;

	if (viddef.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	if (!viddef.direct)
		return;

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			memcpy (&backingbuf[(i + j) * 24],
					viddef.direct + x + ((y << repshift) + i +
										 j) * viddef.rowbytes, width);

			memcpy (viddef.direct + x +
					((y << repshift) + i + j) * viddef.rowbytes,
					&pbitmap[(i >> repshift) * width], width);
		}
	}

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.next = NULL;

	FlipScreen (&rect);
}


void
D_EndDirectRect (int x, int y, int width, int height)
{
	int         i, j, reps, repshift;
	vrect_t     rect;

	if (!vid_initialized)
		return;

	if (viddef.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	if (!viddef.direct)
		return;

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			memcpy (viddef.direct + x +
					((y << repshift) + i + j) * viddef.rowbytes,
					&backingbuf[(i + j) * 24], width);
		}
	}

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.next = NULL;

	FlipScreen (&rect);
}


void
VID_Update (vrect_t *rects)
{
	vrect_t     rect;
	RECT        trect;

	if (!vid_palettized && palette_changed) {
		palette_changed = false;
		rect.x = 0;
		rect.y = 0;
		rect.width = viddef.width;
		rect.height = viddef.height;
		rect.next = NULL;
		rects = &rect;
	}

	if (firstupdate) {
		if (modestate == MS_WINDOWED) {
			GetWindowRect (hWndWinQuake, &trect);

			if ((trect.left != vid_window_x->int_val) ||
				(trect.top != vid_window_y->int_val)) {
				if (COM_CheckParm ("-resetwinpos")) {
					Cvar_SetValue (vid_window_x, 0.0);
					Cvar_SetValue (vid_window_y, 0.0);
				}

				VID_CheckWindowXY ();
				SetWindowPos (hWndWinQuake, NULL, vid_window_x->int_val,
							  vid_window_y->int_val, 0, 0,
							  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW |
							  SWP_DRAWFRAME);
			}
		}

		if ((_vid_default_mode_win->int_val != vid_default) &&
			(!startwindowed
			 || (_vid_default_mode_win->int_val < MODE_FULLSCREEN_DEFAULT))) {
			firstupdate = 0;

			if (COM_CheckParm ("-resetwinpos")) {
				Cvar_SetValue (vid_window_x, 0.0);
				Cvar_SetValue (vid_window_y, 0.0);
			}

			if ((_vid_default_mode_win->int_val < 0) ||
				(_vid_default_mode_win->int_val >= nummodes)) {
				Cvar_SetValue (_vid_default_mode_win, windowed_default);
			}

			Cvar_SetValue (vid_mode, _vid_default_mode_win->int_val);
		}
	}
	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	// check for a driver change
	if ((vid_ddraw->int_val && !vid_usingddraw)
		|| (!vid_ddraw->int_val && vid_usingddraw)) {
		// reset the mode
		force_mode_set = true;
		VID_SetMode (vid_mode->int_val, vid_curpal);
		force_mode_set = false;

		// store back
		if (vid_usingddraw)
			Sys_Printf ("loaded DirectDraw driver\n");
		else
			Sys_Printf ("loaded GDI driver\n");
	}

	if (vid_testingmode) {
		if (Sys_DoubleTime () >= vid_testendtime) {
			VID_SetMode (vid_realmode, vid_curpal);
			vid_testingmode = 0;
		}
	} else {
		if (vid_mode->int_val != vid_realmode) {
			VID_SetMode (vid_mode->int_val, vid_curpal);
			Cvar_SetValue (vid_mode, (float) vid_modenum);
			// so if mode set fails, we don't keep on
			// trying to set that mode
			vid_realmode = vid_modenum;
		}
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (_windowed_mouse->int_val != windowed_mouse) {
			if (_windowed_mouse->int_val) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			windowed_mouse = _windowed_mouse->int_val;
		}
	}
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

/*
=================
VID_NumModes
=================
*/
int
VID_NumModes (void)
{
	return nummodes;
}


/*
=================
VID_GetModePtr
=================
*/
vmode_t    *
VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_CheckModedescFixup
=================
*/
void
VID_CheckModedescFixup (int mode)
{
}


/*
=================
VID_GetModeDescriptionMemCheck
=================
*/
char       *
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


/*
=================
VID_GetModeDescription
=================
*/
char       *
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


/*
=================
VID_GetModeDescription2

Tacks on "windowed" or "fullscreen"
=================
*/
char       *
VID_GetModeDescription2 (int mode)
{
	static char pinfo[40];
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	VID_CheckModedescFixup (mode);
	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLSCREEN) {
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	} else if (modelist[mode].type == MS_FULLDIB) {
		sprintf (pinfo, "%s fullscreen", pv->modedesc);
	} else {
		sprintf (pinfo, "%s windowed", pv->modedesc);
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char       *
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


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void
VID_DescribeCurrentMode_f (void)
{
	Sys_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void
VID_NumModes_f (void)
{
	if (nummodes == 1)
		Sys_Printf ("%d video mode is available\n", nummodes);
	else
		Sys_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void
VID_DescribeMode_f (void)
{
	int         modenum;

	modenum = atoi (Cmd_Argv (1));
	Sys_Printf ("%s\n", VID_GetExtModeDescription (modenum));
}


/*
=================
VID_DescribeModes_f
=================
*/
void
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


/*
=================
VID_TestMode_f
=================
*/
void
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


/*
=================
VID_Windowed_f
=================
*/
void
VID_Windowed_f (void)
{
	VID_SetMode (vid_windowed_mode->int_val, vid_curpal);
}


/*
=================
VID_Fullscreen_f
=================
*/
void
VID_Fullscreen_f (void)
{
	VID_SetMode (vid_fullscreen_mode->int_val, vid_curpal);
}


/*
=================
VID_Minimize_f
=================
*/
void
VID_Minimize_f (void)
{
	// we only support minimizing windows; if you're fullscreen,
	// switch to windowed first
	if (modestate == MS_WINDOWED)
		ShowWindow (hWndWinQuake, SW_MINIMIZE);
}



/*
=================
VID_ForceMode_f
=================
*/
void
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
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		SetWindowText (mainwindow, (LPSTR) va ("%s: %s", PACKAGE_STRING, temp));
		free (temp);
	} else {
		SetWindowText (mainwindow, (LPSTR) va ("%s", PACKAGE_STRING));
	}
}

//static WORD systemgammaramps[3][256];
static WORD currentgammaramps[3][256];

qboolean
VID_SetGamma (double gamma)
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
