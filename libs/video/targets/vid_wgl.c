/*
	vid_wgl.c

	Win32 WGL vid component

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

extern const char *gl_renderer;

HWND		mainwindow;
qboolean win_canalttab = false;
modestate_t modestate = MS_UNINIT;
RECT		window_rect;
DEVMODE	win_gdevmode;
int			window_center_x, window_center_y, window_x, window_y, window_width,
			window_height;

static HGLRC (GLAPIENTRY *qf_wglCreateContext) (HDC);
static BOOL (GLAPIENTRY *qf_wglDeleteContext) (HGLRC);
static HGLRC (GLAPIENTRY *qf_wglGetCurrentContext) (void);
static HDC (GLAPIENTRY *qf_wglGetCurrentDC) (void);
static BOOL (GLAPIENTRY *qf_wglMakeCurrent) (HDC, HGLRC);

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

typedef struct {
	modestate_t type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

static lmode_t lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

static int		nummodes;
static vmode_t	modelist[MAX_MODE_LIST];
static vmode_t	badmode;

static qboolean		windowed, leavecurrentmode;
static int	windowed_mouse;
static HICON hIcon;

static RECT		WindowRect;
static DWORD		WindowStyle;

static int			vid_modenum = NO_MODE;
static int			vid_realmode;
static int			vid_default = MODE_WINDOWED;
static int	windowed_default;
static qboolean	fullsbardraw = true;

static HDC         maindc;



static char       *VID_GetModeDescription (int mode);
static void        GL_Init (void);

static void * (WINAPI *glGetProcAddress) (const char *symbol) = NULL;

void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void       *glfunc = NULL;

	if (glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = GetProcAddress (handle, name);
	return glfunc;
}

void *
QFGL_LoadLibrary (void)
{
	void   *handle;

	if (!(handle = LoadLibrary (gl_driver->string)))
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);
	(FARPROC)glGetProcAddress = GetProcAddress (handle, "wglGetProcAddress");
	return handle;
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
VID_SetWindowedMode (int modenum)
{
	HDC			hdc;
	int			lastmodestate, width, height;
	RECT		rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the window
	mainwindow = CreateWindow ("QuakeForge", "GLQuakeWorld",
							   WindowStyle, rect.left, rect.top, width, height,
							   NULL, NULL, global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window (%lx)",GetLastError());

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

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	scr_width = vid.width = vid.conwidth;
	scr_height = vid.height = vid.conheight;

	vid.numpages = 2;

	if(hIcon) {
		SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
		SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
	}

	return true;
}

static qboolean
VID_SetFullDIBMode (int modenum)
{
	HDC			hdc;
	int			lastmodestate, width, height;
	RECT		rect;

	memset (&win_gdevmode,0,sizeof(win_gdevmode));

	if (!leavecurrentmode) {
		win_gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		win_gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		win_gdevmode.dmPelsWidth = modelist[modenum].width <<
			modelist[modenum].halfscreen;
		win_gdevmode.dmPelsHeight = modelist[modenum].height;
		win_gdevmode.dmSize = sizeof (win_gdevmode);

		if (ChangeDisplaySettings (&win_gdevmode, CDS_FULLSCREEN) !=
			DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode (%lx)",
					   GetLastError());
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	
	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

// FIXME: some drivers have broken FS popup window handling
// until I find way around it, or find some other cause for it
// this is way to avoid it

	if (COM_CheckParm ("-brokenpopup")) WindowStyle = 0;
	else WindowStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindow ("QuakeForge", "GLQuakeWorld",
		WindowStyle, rect.left, rect.top, width, height, NULL, NULL,
		global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window (%lx)",GetLastError());

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC (mainwindow);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC (mainwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	scr_width = vid.width = vid.conwidth;
	scr_height = vid.height = vid.conheight;

	vid.numpages = 2;

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
VID_SetMode (int modenum, unsigned char *palette)
{
	int			original_mode;
	qboolean	stat = 0;
	MSG			msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) || (!windowed && (modenum >= nummodes))) {
		Sys_Error ("Bad video mode");
	}

	// so Con_Printfs don't mess us up by forcing vid and snd updates

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED) {
		if (in_grab->int_val && key_dest == key_game) {
			stat = VID_SetWindowedMode (modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		} else {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode (modenum);
		}
	} else if (modelist[modenum].type == MS_FULLDIB) {
		stat = VID_SetFullDIBMode (modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	} else {
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
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
	VID_SetPalette (palette);
	vid_modenum = modenum;

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

	Con_Printf ("Video mode %s initialized.\n",
				VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

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

void
GL_EndRendering (void)
{
	if (!scr_skipupdate || block_drawing) {
		qfglFinish ();
		SwapBuffers (maindc);
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (!in_grab->int_val) {
			if (windowed_mouse) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !in_mouse_avail && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (in_mouse_avail && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
	if (fullsbardraw)
		Sbar_Changed ();
}

void
VID_Shutdown (void)
{
	HGLRC		hRC;
	HDC			hDC;
	int			i, temp[8192];

#ifdef SPLASH_SCREEN
	if(hwnd_dialog)
		DestroyWindow (hwnd_dialog);
#endif

	if (vid.initialized) {
		win_canalttab = false;
		hRC = qf_wglGetCurrentContext ();
		hDC = qf_wglGetCurrentDC ();

		qf_wglMakeCurrent (NULL, NULL);

		// LordHavoc: free textures before closing (may help NVIDIA)
		for (i = 0; i < 8192; i++)
			temp[i] = i + 1;
		qfglDeleteTextures (8192, temp);

		if (hRC)
			qf_wglDeleteContext (hRC);

		if (hDC && mainwindow)
			ReleaseDC (mainwindow, hDC);

		if (modestate == MS_FULLDIB)
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

	if (!modelist[vid_default].bpp)
		pfd.cColorBits = 16;
	else
		pfd.cColorBits = modelist[vid_default].bpp;

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

static int
VID_NumModes (void)
{
	return nummodes;
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
	char		*pinfo;
	vmode_t		*pv;
	static char	 temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode) {
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	} else {
		snprintf (temp, sizeof (temp), "Desktop resolution (%dx%d)",
				  modelist[MODE_FULLSCREEN_DEFAULT].width,
				  modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console
static char		*
VID_GetExtModeDescription (int mode)
{
	static char pinfo[40];
	vmode_t	   *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB) {
		if (!leavecurrentmode) {
			snprintf (pinfo, sizeof (pinfo), "%s fullscreen", pv->modedesc);
		} else {
			snprintf (pinfo, sizeof (pinfo), "Desktop resolution (%dx%d)",
					  modelist[MODE_FULLSCREEN_DEFAULT].width,
					  modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	} else {
		if (modestate == MS_WINDOWED)
			snprintf (pinfo, sizeof (pinfo), "%s windowed", pv->modedesc);
		else
			snprintf (pinfo, sizeof (pinfo), "windowed");
	}

	return pinfo;
}


static void
VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS	wc;

	// Register the frame class
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "QuakeForge";

	if (!RegisterClass (&wc))
		Sys_Error ("Couldn't register window class (%lx)", GetLastError());

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm ("-width"))
		modelist[0].width = atoi (com_argv[COM_CheckParm ("-width") + 1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm ("-height"))
		modelist[0].height = atoi (com_argv[COM_CheckParm ("-height") + 1]);
	else
		modelist[0].height = modelist[0].width * 240 / 320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	snprintf (modelist[0].modedesc, sizeof (modelist[0].modedesc), "%dx%d",
			  modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}

static void
VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE		devmode;
	int			i, modenum, originalnummodes, existingmode, numlowresmodes;
	int			j, bpp, done;
	BOOL		stat;

	// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do {
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) &&
			(nummodes <	MAX_MODE_LIST)) {
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
				DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				snprintf (modelist[nummodes].modedesc,
						  sizeof (modelist[nummodes].modedesc), "%ldx%ldx%ld",
						  devmode.dmPelsWidth, devmode.dmPelsHeight,
						  devmode.dmBitsPerPel);

				// if the width is more than twice the height, reduce it by
				// half because this is probably a dual-screen monitor
				if (!COM_CheckParm ("-noadjustaspect")) {
					if (modelist[nummodes].width >
						(modelist[nummodes].height << 1)) {
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						snprintf (modelist[nummodes].modedesc,
								  sizeof (modelist[nummodes].modedesc),
								  "%dx%dx%d", modelist[nummodes].width,
								  modelist[nummodes].height,
								  modelist[nummodes].bpp);
					}
				}

				for (i = originalnummodes, existingmode = 0; i < nummodes;
					 i++) {
					if ((modelist[nummodes].width == modelist[i].width) &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp)) {
						existingmode = 1;
						break;
					}
				}

				if (!existingmode) {
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

	// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof (lowresmodes) / sizeof (lowresmodes[0]);
	bpp = 16;
	done = 0;

	do {
		for (j = 0; (j < numlowresmodes) && (nummodes < MAX_MODE_LIST); j++) {
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
				DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				snprintf (modelist[nummodes].modedesc,
						  sizeof (modelist[nummodes].modedesc), "%ldx%ldx%ld",
						  devmode.dmPelsWidth, devmode.dmPelsHeight,
						  devmode.dmBitsPerPel);

				for (i = originalnummodes, existingmode = 0; i < nummodes;
					 i++) {
					if ((modelist[nummodes].width == modelist[i].width) &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp)) {
						existingmode = 1;
						break;
					}
				}

				if (!existingmode) {
					nummodes++;
				}
			}
		}
		switch (bpp) {
			case 16:
				bpp = 32;
				break;
			case 32:
				bpp = 24;
				break;
			case 24:
				done = 1;
				break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_Printf ("No fullscreen DIB modes found\n");
}

void
VID_Init (unsigned char *palette)
{
	int			i, existingmode;
	int			basenummodes, width, height=480, bpp, findbpp, done;
	HDC			hdc;
	DEVMODE		devmode;
	HGLRC		baseRC;
	DWORD		lasterror;

	GL_Pre_Init ();

	qf_wglCreateContext = QFGL_ProcAddress (libgl_handle, "wglCreateContext",
											true);
	qf_wglDeleteContext = QFGL_ProcAddress (libgl_handle, "wglDeleteContext",
											true);
	qf_wglGetCurrentContext = QFGL_ProcAddress (libgl_handle,
												"wglGetCurrentContext", true);
	qf_wglGetCurrentDC = QFGL_ProcAddress (libgl_handle, "wglGetCurrentDC",
										   true);
	qf_wglMakeCurrent = QFGL_ProcAddress (libgl_handle, "wglMakeCurrent",
										  true);

	memset (&devmode, 0, sizeof (devmode));

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm ("-window")) {
		hdc = GetDC (NULL);

		if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	} else {
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm ("-mode")) {
			vid_default = atoi (com_argv[COM_CheckParm ("-mode") + 1]);
		} else {
			if (COM_CheckParm ("-current")) {
				modelist[MODE_FULLSCREEN_DEFAULT].width =
					GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height =
					GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			} else {
				if (COM_CheckParm ("-width")) {
					width = atoi (com_argv[COM_CheckParm ("-width") + 1]);
				} else {
					width = 640;
				}

				if (COM_CheckParm ("-bpp")) {
					bpp = atoi (com_argv[COM_CheckParm ("-bpp") + 1]);
					findbpp = 0;
				} else {
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm ("-height"))
					height = atoi (com_argv[COM_CheckParm ("-height") + 1]);

				// if they want to force it, add the specified mode to the list
				if (COM_CheckParm ("-force") && (nummodes < MAX_MODE_LIST)) {
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					snprintf (modelist[nummodes].modedesc,
							  sizeof (modelist[nummodes].modedesc),
							  "%ldx%ldx%ld", devmode.dmPelsWidth,
							  devmode.dmPelsHeight, devmode.dmBitsPerPel);

					for (i = nummodes, existingmode = 0; i < nummodes; i++) {
						if ((modelist[nummodes].width == modelist[i].width) &&
							(modelist[nummodes].height == modelist[i].height)
							&& (modelist[nummodes].bpp == modelist[i].bpp)) {
							existingmode = 1;
							break;
						}
					}

					if (!existingmode) {
						nummodes++;
					}
				}

				done = 0;

				do {
					if (COM_CheckParm ("-height")) {
						height = atoi
							(com_argv[COM_CheckParm ("-height") + 1]);

						for (i = 1, vid_default = 0; i < nummodes; i++) {
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp)) {
								vid_default = i;
								done = 1;
								break;
							}
						}
					} else {
						for (i = 1, vid_default = 0; i < nummodes; i++) {
							if ((modelist[i].width == width)
								&& (modelist[i].bpp == bpp)) {
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done) {
						if (findbpp) {
							switch (bpp) {
								case 15:
									bpp = 16;
									break;
								case 16:
									bpp = 32;
									break;
								case 32:
									bpp = 24;
									break;
								case 24:
									done = 1;
									break;
							}
						} else {
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default) {
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = atoi (com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap8 = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap8 + 2048));

#ifdef SPLASH_SCREEN
	if(hwnd_dialog)
		DestroyWindow (hwnd_dialog);
#endif

	VID_SetMode (vid_default, palette);

	maindc = GetDC (mainwindow);
	bSetupPixelFormat (maindc);

	baseRC = qf_wglCreateContext (maindc);
	if (!baseRC) {
		lasterror=GetLastError();
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		Sys_Error("Could not initialize GL (wglCreateContext failed).\n\nMake "
				  "sure you in are 65535 color mode, and try running -window. "
				  "\nError code: (%lx)", lasterror);
	}

	if (!qf_wglMakeCurrent (maindc, baseRC)) {
		lasterror=GetLastError();
		if (baseRC)
			qf_wglDeleteContext (baseRC);
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		Sys_Error ("wglMakeCurrent failed (%lx)", lasterror);
	}

	GL_Init ();

	VID_InitGamma (palette);
	VID_Init8bitPalette ();
	VID_SetPalette (vid.palette);

	vid.initialized = true;

	vid_realmode = vid_modenum;

	strcpy (badmode.modedesc, "Bad mode");
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
					   (LPSTR) va ("%s %s: %s", PROGRAM, VERSION, temp));
		free (temp);
	} else {
		SetWindowText (mainwindow, (LPSTR) va ("%s %s", PROGRAM, VERSION));
	}
}

static WORD systemgammaramps[3][256];
static WORD currentgammaramps[3][256];

qboolean
VID_SetGamma (double gamma)
{
	int			i;
	HDC			hdc = GetDC (NULL);

	for (i = 0; i < 256; i++) {
		currentgammaramps[2][i] = currentgammaramps[1][i] =
			currentgammaramps[0][i] = gammatable[i] * 256;
	}

	i = SetDeviceGammaRamp (hdc, &currentgammaramps[0][0]);
	ReleaseDC (NULL, hdc);
	return i;
}

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
