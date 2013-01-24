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

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "in_win.h"
#include "r_cvar.h"
#include "win32/resources/resource.h"
#include "sbar.h"
#include "vid_internal.h"

extern const char *gl_renderer;

HWND		mainwindow;
qboolean	win_canalttab = false;
modestate_t modestate = MS_UNINIT;
RECT		window_rect;
DEVMODE		win_gdevmode;
int			window_center_x, window_center_y, window_x, window_y, window_width,
			window_height;

static void *libgl_handle;
static HGLRC (GLAPIENTRY *qfwglCreateContext) (HDC);
static BOOL (GLAPIENTRY *qfwglDeleteContext) (HGLRC);
static HGLRC (GLAPIENTRY *qfwglGetCurrentContext) (void);
static HDC (GLAPIENTRY *qfwglGetCurrentDC) (void);
static BOOL (GLAPIENTRY *qfwglMakeCurrent) (HDC, HGLRC);

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

static int		windowed_mouse;

static HICON	hIcon;
static RECT		WindowRect;
static DWORD	WindowStyle;

static qboolean	fullsbardraw = true;

static HDC		maindc;


static void GL_Init (void);
static void * (WINAPI *glGetProcAddress) (const char *symbol) = NULL;


static void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void   *glfunc = NULL;

	if (glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = GetProcAddress (handle, name);
	return glfunc;
}

static void *
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void    *glfunc = NULL;

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

//====================================

static void
CenterWindow (HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	int			CenterX, CenterY;

	CenterX = (GetSystemMetrics (SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics (SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;							// dual screens
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

static qboolean
VID_SetWindowedMode ( void )
{
	HDC			hdc;
	int			width, height;
	RECT		rect;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the window
	mainwindow = CreateWindow ("QuakeForge", PACKAGE_NAME,
							   WindowStyle, rect.left, rect.top, width, height,
							   NULL, NULL, global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window (%lx)", GetLastError ());

	// Center and show the window
	CenterWindow (mainwindow, WindowRect.right - WindowRect.left,
				  WindowRect.bottom - WindowRect.top, false);

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	modestate = MS_WINDOWED;

	// because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC (mainwindow);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC (mainwindow, hdc);

	viddef.numpages = 2;

	if(hIcon) {
		SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
		SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
	}

	return true;
}

static qboolean
VID_SetFullDIBMode ( void )
{
	HDC			hdc;
	int			width, height;
	RECT		rect;

	if (ChangeDisplaySettings (&win_gdevmode, CDS_FULLSCREEN)
		!= DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen DIB mode (%lx)", GetLastError());

	// FIXME: some drivers have broken FS popup window handling until I find a
	// way around it, or find some other cause for it this is a way to avoid it
	if (COM_CheckParm ("-brokenpopup"))
		WindowStyle = 0;
	else
		WindowStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindow ("QuakeForge", "GLQuakeWorld", WindowStyle,
							   rect.left, rect.top, width, height, NULL, NULL,
							   global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window (%lx)",GetLastError());

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be empty
	// while Quake starts up.
	hdc = GetDC (mainwindow);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC (mainwindow, hdc);

	viddef.numpages = 2;

	// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	if (hIcon) {
		SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
		SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
	}

	return true;
}

static int
VID_SetMode (unsigned char *palette)
{
	qboolean	stat = 0;
	MSG			msg;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	CDAudio_Pause ();

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = viddef.width;
	WindowRect.bottom = viddef.height;

	window_width = viddef.width;
	window_height = viddef.height;

	// Set either the fullscreen or windowed mode
	if (!vid_fullscreen->int_val) {
		stat = VID_SetWindowedMode ();
	} else {
		stat = VID_SetFullDIBMode ();
	}

	VID_UpdateWindowStatus (window_x, window_y);

	CDAudio_Resume ();

	if (!stat) {
		Sys_Error ("Couldn't set video mode");
	}

	// Now we try to make sure we get the focus on the mode switch, because
	// sometimes in some systems we don't.  We grab the foreground, then
	// finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put
	// ourselves at the top of the z order, then grab the foreground again,
	// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	viddef.set_palette (palette);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	IN_ClearStates ();

	Sys_Printf ("Video mode %ix%i initialized.\n",
				viddef.width, viddef.height);

	viddef.set_palette (palette);

	viddef.recalc_refdef = 1;

	return true;
}

void
VID_UpdateWindowStatus (int w_x, int w_y)
{
	window_rect.left = window_x = w_x;
	window_rect.top = window_y = w_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

static void
GL_Init (void)
{
	GL_Init_Common ();
	if (strnicmp (gl_renderer, "PowerVR", 7) == 0)
		fullsbardraw = true;
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
			if (windowed_mouse) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
		}
	}
}

void
VID_Shutdown (void)
{
	HGLRC		hRC;
	HDC			hDC;
	int			i;
	GLuint      temp[8192];

#ifdef SPLASH_SCREEN
	if(hwnd_dialog)
		DestroyWindow (hwnd_dialog);
#endif

	if (viddef.initialized) {
		win_canalttab = false;
		hRC = qfwglGetCurrentContext ();
		hDC = qfwglGetCurrentDC ();

		qfwglMakeCurrent (NULL, NULL);

		// LordHavoc: free textures before closing (may help NVIDIA)
		for (i = 0; i < 8192; i++)
			temp[i] = i + 1;
		qfglDeleteTextures (8192, temp);

		if (hRC)
			qfwglDeleteContext (hRC);

		if (hDC && mainwindow)
			ReleaseDC (mainwindow, hDC);

		if (vid_fullscreen->int_val)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);

		AppActivate (false, false);
	}
}

//==========================================================================

static BOOL
bSetupPixelFormat (HDC hDC)
{
	PIXELFORMATDESCRIPTOR pfd ;
	int		pixelformat;

	memset (&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;

	pfd.cColorBits = win_gdevmode.dmBitsPerPel ;

	pfd.cDepthBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;

	if ((pixelformat = ChoosePixelFormat (hDC, &pfd)) == 0) {
		MessageBox (NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat (hDC, pixelformat, &pfd) == FALSE) {
		MessageBox (NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}
	return TRUE;
}

static void
wgl_load_gl (void)
{
	viddef.get_proc_address = QFGL_ProcAddress;
	viddef.end_rendering = GL_EndRendering;

	if (!(libgl_handle = LoadLibrary (gl_driver->string)))
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);
	glGetProcAddress =(void*)GetProcAddress(libgl_handle, "wglGetProcAddress");

	qfwglCreateContext = QFGL_ProcAddress ("wglCreateContext", true);
	qfwglDeleteContext = QFGL_ProcAddress ("wglDeleteContext", true);
	qfwglGetCurrentContext = QFGL_ProcAddress ("wglGetCurrentContext", true);
	qfwglGetCurrentDC = QFGL_ProcAddress ("wglGetCurrentDC", true);
	qfwglMakeCurrent = QFGL_ProcAddress ("wglMakeCurrent", true);
}

void
VID_Init (byte *palette, byte *colormap)
{
	BOOL		stat;
	WORD		bpp, vid_mode;
	HDC			hdc;
	HGLRC		baseRC;
	DWORD		lasterror;
	WNDCLASS	wc;

	R_LoadModule (wgl_load_gl, 0);	//FIXME VID_SetPalette

	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ROM | CVAR_ARCHIVE,
							   NULL, "Run WGL client at fullscreen");

	memset (&win_gdevmode, 0, sizeof (win_gdevmode));

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));


	// Register the frame class
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = global_hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "QuakeForge";

	if (!RegisterClass (&wc))
		Sys_Error ("Couldn't register window class (%lx)", GetLastError ());

	VID_GetWindowSize (640, 480);

	if (!vid_fullscreen->int_val) {
		hdc = GetDC (NULL);

		if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);
	} else {
		if (COM_CheckParm ("-bpp")) {
			bpp = atoi (com_argv[COM_CheckParm ("-bpp") + 1]);
		} else {
			bpp = 16;
		}
		vid_mode = 0;
		do {
			stat = EnumDisplaySettings (NULL, vid_mode, &win_gdevmode);

			if ((win_gdevmode.dmBitsPerPel == bpp)
				&& (win_gdevmode.dmPelsWidth == viddef.width)
				&& (win_gdevmode.dmPelsHeight == viddef.height)) {
				win_gdevmode.dmFields = (DM_BITSPERPEL | DM_PELSWIDTH
										 | DM_PELSHEIGHT);

				if (ChangeDisplaySettings (&win_gdevmode,
										   CDS_TEST | CDS_FULLSCREEN)
					== DISP_CHANGE_SUCCESSFUL) {
					break;
				}
			}

			vid_mode++;
		} while (stat);
		if (!stat)
			Sys_Error ("Couldn't get requested resolution (%i, %i, %i)",
					   viddef.width, viddef.height, bpp);
	}

	viddef.maxwarpwidth = WARP_WIDTH;
	viddef.maxwarpheight = WARP_HEIGHT;
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];

#ifdef SPLASH_SCREEN
	if(hwnd_dialog)
		DestroyWindow (hwnd_dialog);
#endif

	VID_SetMode (palette);

	maindc = GetDC (mainwindow);
	bSetupPixelFormat (maindc);

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

	GL_Init ();

	VID_InitGamma (palette);
	viddef.set_palette (viddef.palette);

	viddef.initialized = true;

	win_canalttab = true;

	if (COM_CheckParm ("-nofullsbar"))
		fullsbardraw = false;
}

void
VID_Init_Cvars ()
{
}

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char *temp = strdup (text);

		SetWindowText (mainwindow,
					   (LPSTR) va ("%s: %s", PACKAGE_STRING, temp));
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
	int			i;
	HDC			hdc = GetDC (NULL);

	for (i = 0; i < 256; i++) {
		currentgammaramps[2][i] = currentgammaramps[1][i] =
			currentgammaramps[0][i] = viddef.gammatable[i] * 256;
	}

	i = SetDeviceGammaRamp (hdc, &currentgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
	return i;
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
VID_Update (vrect_t *rects)
{
	//FIXME sw
}

#if 0
static void
VID_SaveGamma (void)
{
	HDC			hdc = GetDC (NULL);

	GetDeviceGammaRamp (hdc, &systemgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
}

static void
VID_RestoreGamma (void)
{
	HDC			hdc = GetDC (NULL);

	SetDeviceGammaRamp (hdc, &systemgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
}
#endif
