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
int         win_palettized;
int         win_minimized;
int         win_canalttab = 0;
sw_ctx_t   *win_sw_context;

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 3)

cvar_t *vid_ddraw;

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

static DWORD WindowStyle, ExWindowStyle;

int  win_center_x, win_center_y;
RECT win_rect;

DEVMODE     win_gdevmode;
static qboolean startwindowed = 0;
//static qboolean windowed_mode_set;
//static int  vid_fulldib_on_focus_mode;
static qboolean force_minimized;
static qboolean in_mode_set;
static qboolean force_mode_set;
static qboolean vid_mode_set;
//static HICON hIcon;


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

static vmode_t modelist[MAX_MODE_LIST] = {
	{
		.type = MS_WINDOWED,
		.width = 320,
		.height = 240,
		.modedesc = "320x240",
		.modenum = MODE_WINDOWED,
		.fullscreen = 0,
	},
	{
		.type = MS_WINDOWED,
		.width = 640,
		.height = 480,
		.modedesc = "640x480",
		.modenum = MODE_WINDOWED + 1,
		.fullscreen = 0,
	},
	{
		.type = MS_WINDOWED,
		.width = 800,
		.height = 600,
		.modedesc = "800x600",
		.modenum = MODE_WINDOWED + 2,
		.fullscreen = 0,
	}
};
static int  nummodes;

int         aPage;						// Current active display page
int         vPage;						// Current visible display page
int         waitVRT = true;				// True to wait for retrace on flip

static vmode_t badmode = {
	.modedesc = "Bad mode",
};

static int  VID_SetMode (int modenum, const byte *palette);

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

#if 0
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
#endif

void
Win_UpdateWindowStatus (int window_x, int window_y)
{
	win_rect.left = window_x;
	win_rect.top = window_y;
	win_rect.right = window_x + viddef.width;
	win_rect.bottom = window_y + viddef.height;
	win_center_x = (win_rect.left + win_rect.right) / 2;
	win_center_y = (win_rect.top + win_rect.bottom) / 2;
	IN_UpdateClipCursor ();
}


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

#ifdef SPLASH_SCREEN
    if (hwnd_dialog)
        DestroyWindow (hwnd_dialog);
#endif
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
		Win_Activate (false, false);

//FIXME?        if (hwnd_dialog) DestroyWindow (hwnd_dialog);
		if (win_mainwindow)
			DestroyWindow (win_mainwindow);

		vid_testingmode = 0;
		viddef.initialized = false;
	}
}

void
Win_SetVidMode (int width, int height)
{
//FIXME SCR_StretchInit();

	force_mode_set = true;
	//VID_SetMode (vid_default, palette);
	force_mode_set = false;
	vid_realmode = vid_modenum;
}
#if 0
static void
VID_DestroyWindow (void)
{
	if (modestate == MS_FULLDIB)
		ChangeDisplaySettings (NULL, CDS_FULLSCREEN);

	Win_UnloadAllDrivers ();
}
#endif
static void
VID_CheckModedescFixup (int mode)
{
}

static qboolean
VID_SetWindowedMode (int modenum)
{
#if 0
	if (!windowed_mode_set) {
		if (COM_CheckParm ("-resetwinpos")) {
			Cvar_SetValue (vid_window_x, 0.0);
			Cvar_SetValue (vid_window_y, 0.0);
		}

		windowed_mode_set = true;
	}

	VID_CheckModedescFixup (modenum);
	VID_DestroyWindow ();

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX |
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPSIBLINGS |
		WS_CLIPCHILDREN | WS_THICKFRAME;

	// WindowStyle = WS_OVERLAPPEDWINDOW|WS_VISIBLE;
	ExWindowStyle = 0;

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
#endif
	return true;
}


static qboolean
VID_SetFullDIBMode (int modenum)
{
#if 0
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
		win_palettized = true;
	} else {
		win_palettized = false;
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
	IN_ClearStates ();

	Sys_Printf ("%s\n", VID_GetModeDescription (vid_modenum));

	in_mode_set = false;

	viddef.recalc_refdef = 1;

//FIXME SCR_StretchInit();
//FIXME SCR_StretchRefresh();
//FIXME SCR_CvarCheck();
	return true;
}


void
Win_CreateWindow (int width, int height)
{
	RECT rect = {
		.top = 0,
		.left = 0,
		.right = width,
		.bottom = height,
	};
	AdjustWindowRectEx (&rect, WindowStyle, FALSE, 0);
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
								   rect.right - rect.left,
								   rect.bottom - rect.top,
								   NULL, NULL, global_hInstance, NULL);

	if (!win_mainwindow)
		Sys_Error ("Couldn't create DIB window");

	// done
	vid_mode_set = true;
//FIXME if (firsttime) S_Init ();
	Win_UpdateWindowStatus (0, 0);		// FIXME right numbers?

	HDC         hdc = GetDC (NULL);
	if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
		win_palettized = true;
	} else {
		win_palettized = false;
	}
	ReleaseDC (NULL, hdc);

	//vid_modenum = modenum;
	//Cvar_SetValue (vid_mode, (float) vid_modenum);

	MSG         msg;
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
	IN_ClearStates ();

	Sys_Printf ("%s\n", VID_GetModeDescription (vid_modenum));

	in_mode_set = false;

	viddef.recalc_refdef = 1;
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

void
Win_Init_Cvars (void)
{
	vid_ddraw = Cvar_Get ("vid_ddraw", "1", CVAR_NONE, 0, "");
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_NONE, 0, "");
	vid_wait = Cvar_Get ("vid_wait", "0", CVAR_NONE, 0, "");
	vid_nopageflip =
		Cvar_Get ("vid_nopageflip", "0", CVAR_ARCHIVE, 0, "");
	_vid_wait_override =
		Cvar_Get ("_vid_wait_override", "0", CVAR_ARCHIVE, 0, "");
	_vid_default_mode =
		Cvar_Get ("_vid_default_mode", "0", CVAR_ARCHIVE, 0, "");
	_vid_default_mode_win =
		Cvar_Get ("_vid_default_mode_win", "3", CVAR_ARCHIVE, 0, "");
	vid_config_x =
		Cvar_Get ("vid_config_x", "800", CVAR_ARCHIVE, 0, "");
	vid_config_y =
		Cvar_Get ("vid_config_y", "600", CVAR_ARCHIVE, 0, "");
	vid_stretch_by_2 =
		Cvar_Get ("vid_stretch_by_2", "1", CVAR_ARCHIVE, 0, "");
	_windowed_mouse =
		Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, 0, "");
	vid_fullscreen_mode =
		Cvar_Get ("vid_fullscreen_mode", "3", CVAR_ARCHIVE, 0, "");
	vid_windowed_mode =
		Cvar_Get ("vid_windowed_mode", "0", CVAR_ARCHIVE, 0, "");
	block_switch =
		Cvar_Get ("block_switch", "0", CVAR_ARCHIVE, 0, "");
	vid_window_x =
		Cvar_Get ("vid_window_x", "0", CVAR_ARCHIVE, 0, "");
	vid_window_y =
		Cvar_Get ("vid_window_y", "0", CVAR_ARCHIVE, 0, "");

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

extern int win_force_link;
static __attribute__((used)) int *context_win_force_link = &win_force_link;
