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
#include "in_win.h"
#include "vid_internal.h"
#include "vid_sw.h"

HWND        win_mainwindow;
HDC         win_maindc;
HCURSOR     win_arrow;
bool        win_cursor_visible;
int         win_palettized;
bool        win_minimized;
bool        win_focused;
bool        win_canalttab;
sw_ctx_t   *win_sw_context;

#define MODE_WINDOWED			0
#define MODE_SETTABLE_WINDOW	2
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 3)

#define WINDOW_CLASS PACKAGE_NAME "WindowClass"

int vid_ddraw;
static cvar_t vid_ddraw_cvar = {
	.name = "vid_ddraw",
	.description =
		"",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &vid_ddraw },
};

// Note that 0 is MODE_WINDOWED
static int vid_mode;
static cvar_t vid_mode_cvar = {
	.name = "vid_mode",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &vid_mode },
};

// Note that 0 is MODE_WINDOWED
int _vid_default_mode;
static cvar_t _vid_default_mode_cvar = {
	.name = "_vid_default_mode",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &_vid_default_mode },
};

// Note that 3 is MODE_FULLSCREEN_DEFAULT
static int _vid_default_mode_win;
static cvar_t _vid_default_mode_win_cvar = {
	.name = "_vid_default_mode_win",
	.description =
		"",
	.default_value = "3",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &_vid_default_mode_win },
};
int vid_wait;
static cvar_t vid_wait_cvar = {
	.name = "vid_wait",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &vid_wait },
};
int vid_nopageflip;
static cvar_t vid_nopageflip_cvar = {
	.name = "vid_nopageflip",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_nopageflip },
};
int _vid_wait_override;
static cvar_t _vid_wait_override_cvar = {
	.name = "_vid_wait_override",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &_vid_wait_override },
};
int vid_config_x;
static cvar_t vid_config_x_cvar = {
	.name = "vid_config_x",
	.description =
		"",
	.default_value = "800",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_config_x },
};
int vid_config_y;
static cvar_t vid_config_y_cvar = {
	.name = "vid_config_y",
	.description =
		"",
	.default_value = "600",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_config_y },
};
int vid_stretch_by_2;
static cvar_t vid_stretch_by_2_cvar = {
	.name = "vid_stretch_by_2",
	.description =
		"",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_stretch_by_2 },
};
static int _windowed_mouse;
static cvar_t _windowed_mouse_cvar = {
	.name = "_windowed_mouse",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &_windowed_mouse },
};
static int vid_fullscreen_mode;
static cvar_t vid_fullscreen_mode_cvar = {
	.name = "vid_fullscreen_mode",
	.description =
		"",
	.default_value = "3",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_fullscreen_mode },
};
static int vid_windowed_mode;
static cvar_t vid_windowed_mode_cvar = {
	.name = "vid_windowed_mode",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_windowed_mode },
};
int block_switch;
static cvar_t block_switch_cvar = {
	.name = "block_switch",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &block_switch },
};
static int vid_window_x;
static cvar_t vid_window_x_cvar = {
	.name = "vid_window_x",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_window_x },
};
static int vid_window_y;
static cvar_t vid_window_y_cvar = {
	.name = "vid_window_y",
	.description =
		"",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_window_y },
};

//FIXME?int yeahimconsoled;

#define MAX_MODE_LIST	36
#define VID_ROW_SIZE	3

static DWORD WindowStyle, ExWindowStyle;

int  win_center_x, win_center_y;
int  win_pos_x, win_pos_y;
int  win_len_x, win_len_y;
RECT win_rect;

DEVMODE     win_gdevmode;

int         vid_modenum = NO_MODE;
int         vid_testingmode, vid_realmode;
double      vid_testendtime;
int         vid_default = MODE_WINDOWED;

modestate_t modestate = MS_UNINIT;

byte        vid_curpal[256 * 3];

int         mode;

int         aPage;						// Current active display page
int         vPage;						// Current visible display page
int         waitVRT = true;				// True to wait for retrace on flip

static LONG (*event_handlers[WM_USER])(HWND, UINT, WPARAM, LPARAM);

bool
Win_AddEvent (UINT event, LONG (*event_handler)(HWND, UINT, WPARAM, LPARAM))
{
	if (event >= WM_USER) {
		Sys_MaskPrintf (SYS_vid, "event: %d, WM_USER: %d\n", event, WM_USER);
		return false;
	}

	if (event_handlers[event]) {
		return false;
	}

	event_handlers[event] = event_handler;
	return true;
}

bool
Win_RemoveEvent (UINT event)
{
	if (event >= WM_USER) {
		Sys_MaskPrintf (SYS_vid, "event: %d, WM_USER: %d\n", event, WM_USER);
		return false;
	}
	event_handlers[event] = 0;
	return true;
}

static LONG WINAPI
Win_EventHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg < WM_USER && event_handlers[uMsg]) {
		return event_handlers[uMsg] (hWnd, uMsg, wParam, lParam);
	}
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

static void
Win_UpdateWindowStatus ()
{
	win_rect.left = win_pos_x;
	win_rect.top = win_pos_y;
	win_rect.right = win_pos_x + win_len_x;
	win_rect.bottom = win_pos_y + win_len_y;
	win_center_x = (win_rect.left + win_rect.right) / 2;
	win_center_y = (win_rect.top + win_rect.bottom) / 2;
	VID_SetWindow (win_pos_x, win_pos_y, win_len_x, win_len_y);
	IN_UpdateClipCursor ();
}

static void
VID_InitModes (HINSTANCE hInstance)
{
	WNDCLASS    wc = {
		.style = CS_OWNDC,
		.lpfnWndProc = (WNDPROC) Win_EventHandler,
		.hInstance = hInstance,
		.lpszClassName = WINDOW_CLASS,
	};

	win_arrow = LoadCursor (NULL, IDC_ARROW);

//FIXME hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON2));

	/* Register the frame class */

	if (!RegisterClass (&wc))
		Sys_Error ("Couldn't register window class");
}

void
Win_OpenDisplay (void)
{
	global_hInstance = GetModuleHandle (0);
	VID_InitModes (global_hInstance);
}

void
Win_CloseDisplay (void)
{
	if (viddef.initialized) {
		PostMessage (HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM) win_mainwindow,
					 (LPARAM) 0);
		PostMessage (HWND_BROADCAST, WM_SYSCOLORCHANGE, (WPARAM) 0, (LPARAM) 0);

		vid_testingmode = 0;
		viddef.initialized = false;
	}
}

static RECT window_rect;
void
Win_UpdateFullscreen (int fullscreen)
{
	if (fullscreen) {
		GetWindowRect (win_mainwindow, &window_rect);
		SetWindowLongW (win_mainwindow, GWL_STYLE, 0);
		HMONITOR monitor = MonitorFromWindow (win_mainwindow, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info = {
			.cbSize = sizeof (info),
		};
		GetMonitorInfoW (monitor, &info);
		SetWindowPos (win_mainwindow, HWND_TOP,
			info.rcMonitor.left,
			info.rcMonitor.top,
			info.rcMonitor.right - info.rcMonitor.left,
			info.rcMonitor.bottom - info.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow (win_mainwindow, SW_MAXIMIZE);
	} else {
		SetWindowLongW (win_mainwindow, GWL_STYLE, WindowStyle);
		SetWindowPos (win_mainwindow, HWND_NOTOPMOST,
			window_rect.left,
			window_rect.top,
			window_rect.right - window_rect.left,
			window_rect.bottom - window_rect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow (win_mainwindow, SW_NORMAL);
	}
}

static long
notify_create (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return 1;
}

static long
notify_destroy (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (win_mainwindow) {
		DestroyWindow (win_mainwindow);
		win_mainwindow = 0;
	}
	PostQuitMessage (0);
	return 1;
}

static long
notify_move (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_pos_x = (short) LOWORD (lParam);
	win_pos_y = (short) HIWORD (lParam);
	Sys_MaskPrintf (SYS_vid, "notify_move: %d %d\n", win_pos_x, win_pos_y);
	Win_UpdateWindowStatus ();
	return 1;
}

static long
notify_size (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_len_x = (short) LOWORD (lParam);
	win_len_y = (short) HIWORD (lParam);
	Sys_MaskPrintf (SYS_vid, "notify_size: %d %d\n", win_pos_x, win_pos_y);
	Win_UpdateWindowStatus ();
	return 1;
}

void
Win_CreateWindow (int width, int height)
{
	IN_Win_Preinit ();

	Win_AddEvent (WM_CREATE, notify_create);
	Win_AddEvent (WM_DESTROY, notify_destroy);
	Win_AddEvent (WM_MOVE, notify_move);
	Win_AddEvent (WM_SIZE, notify_size);


	RECT rect = {
		.top = win_pos_x = 0,
		.left = win_pos_y = 0,
		.right = win_len_x = width,
		.bottom = win_len_y = height,
	};
	WindowStyle = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX |
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	AdjustWindowRectEx (&rect, WindowStyle, FALSE, 0);
	// sound initialization has to go here, preceded by a windowed mode set,
	// so there's a window for DirectSound to work with but we're not yet
	// fullscreen so the "hardware already in use" dialog is visible if it
	// gets displayed
	// keep the window minimized until we're ready for the first real mode set
	win_mainwindow = CreateWindowExA (ExWindowStyle,
								   WINDOW_CLASS,
								   PACKAGE_STRING,
								   WindowStyle,
								   0, 0,
								   rect.right - rect.left,
								   rect.bottom - rect.top,
								   NULL, NULL, global_hInstance, NULL);

	if (!win_mainwindow)
		Sys_Error ("Couldn't create window");

	// done
	Win_UpdateWindowStatus ();

	HDC         hdc = GetDC (NULL);
	if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
		win_palettized = true;
	} else {
		win_palettized = false;
	}
	ReleaseDC (NULL, hdc);

	MSG         msg;
	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (win_mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);
	SetForegroundWindow (win_mainwindow);

	viddef.recalc_refdef = 1;
}

void
Win_SetCaption (const char *text)
{
	if (win_mainwindow) {
		SetWindowTextA (win_mainwindow, text);
	}
}

//static WORD systemgammaramps[3][256];
static WORD currentgammaramps[3][256];

bool
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
	Cvar_Register (&vid_ddraw_cvar, 0, 0);
	Cvar_Register (&vid_mode_cvar, 0, 0);
	Cvar_Register (&vid_wait_cvar, 0, 0);
	Cvar_Register (&vid_nopageflip_cvar, 0, 0);
	Cvar_Register (&_vid_wait_override_cvar, 0, 0);
	Cvar_Register (&_vid_default_mode_cvar, 0, 0);
	Cvar_Register (&_vid_default_mode_win_cvar, 0, 0);
	Cvar_Register (&vid_config_x_cvar, 0, 0);
	Cvar_Register (&vid_config_y_cvar, 0, 0);
	Cvar_Register (&vid_stretch_by_2_cvar, 0, 0);
	Cvar_Register (&_windowed_mouse_cvar, 0, 0);
	Cvar_Register (&vid_fullscreen_mode_cvar, 0, 0);
	Cvar_Register (&vid_windowed_mode_cvar, 0, 0);
	Cvar_Register (&block_switch_cvar, 0, 0);
	Cvar_Register (&vid_window_x_cvar, 0, 0);
	Cvar_Register (&vid_window_y_cvar, 0, 0);
}

extern int win_force_link;
static __attribute__((used)) int *context_win_force_link = &win_force_link;
