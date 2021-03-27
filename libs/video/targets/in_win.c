/*
	in_win.c

	windows 95 mouse stuff

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
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef __MINGW32__
# define INITGUID
#endif

#include "winquake.h"	// must come first due to nasties in windows headers
#include <dinput.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "compat.h"
#include "context_win.h"
#include "in_win.h"

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI * pDirectInputCreate) (HINSTANCE hinst, DWORD dwVersion,
									   LPDIRECTINPUT * lplpDirectInput,
									   LPUNKNOWN punkOuter);

// mouse local variables
static unsigned uiWheelMessage;
static unsigned  mouse_buttons;
static unsigned  mouse_oldbuttonstate;
static POINT current_pos;
static float mx_accum, my_accum;
static qboolean mouseinitialized;
static qboolean restore_spi;
static int  originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
static qboolean mouseparmsvalid, mouseactivatetoggle;
static qboolean mouseshowtoggle = 1;
static qboolean dinput_acquired;
static unsigned int mstate_di;

// misc locals
static LPDIRECTINPUT g_pdi;
static LPDIRECTINPUTDEVICE g_pMouse;

static HINSTANCE hInstDI;

static qboolean dinput;

static qboolean vid_wassuspended = false;
static qboolean win_in_game = false;

typedef struct MYDATA {
	LONG        lX;						// X axis goes here
	LONG        lY;						// Y axis goes here
	LONG        lZ;						// Z axis goes here
	BYTE        bButtonA;				// One button goes here
	BYTE        bButtonB;				// Another button goes here
	BYTE        bButtonC;				// Another button goes here
	BYTE        bButtonD;				// Another button goes here
} MYDATA;

static DIOBJECTDATAFORMAT rgodf[] = {
	{&GUID_XAxis, FIELD_OFFSET (MYDATA, lX), DIDFT_AXIS | DIDFT_ANYINSTANCE,
	 0,},
	{&GUID_YAxis, FIELD_OFFSET (MYDATA, lY), DIDFT_AXIS | DIDFT_ANYINSTANCE,
	 0,},
	{&GUID_ZAxis, FIELD_OFFSET (MYDATA, lZ),
	 0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0,},
	{0, FIELD_OFFSET (MYDATA, bButtonA), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
	{0, FIELD_OFFSET (MYDATA, bButtonB), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
	{0, FIELD_OFFSET (MYDATA, bButtonC),
	 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
	{0, FIELD_OFFSET (MYDATA, bButtonD),
	 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT df = {
	sizeof (DIDATAFORMAT),				// this structure
	sizeof (DIOBJECTDATAFORMAT),		// size of object data format
	DIDF_RELAXIS,						// absolute axis coordinates
	sizeof (MYDATA),					// device data size
	NUM_OBJECTS,						// number of objects
	rgodf,								// and here they are
};

void
IN_UpdateClipCursor (void)
{
	if (mouseinitialized && in_mouse_avail && !dinput) {
		ClipCursor (&window_rect);
	}
}

void
IN_ShowMouse (void)
{
	if (!mouseshowtoggle) {
		ShowCursor (TRUE);
		mouseshowtoggle = 1;
	}
}

void
IN_HideMouse (void)
{
	if (mouseshowtoggle) {
		ShowCursor (FALSE);
		mouseshowtoggle = 0;
	}
}

void
IN_ActivateMouse (void)
{
	mouseactivatetoggle = true;

	if (mouseinitialized) {
		if (dinput) {
			if (g_pMouse) {
				if (!dinput_acquired) {
					IDirectInputDevice_Acquire (g_pMouse);
					dinput_acquired = true;
				}
			} else {
				return;
			}
		} else {
			if (mouseparmsvalid)
				restore_spi =
					SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

			SetCursorPos (window_center_x, window_center_y);
			SetCapture (win_mainwindow);
			ClipCursor (&window_rect);
		}

		in_mouse_avail = true;
	}
}

void
IN_DeactivateMouse (void)
{

	mouseactivatetoggle = false;

	if (mouseinitialized) {
		if (dinput) {
			if (g_pMouse) {
				if (dinput_acquired) {
					IDirectInputDevice_Unacquire (g_pMouse);
					dinput_acquired = false;
				}
			}
		} else {
			if (restore_spi)
				SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

			ClipCursor (NULL);
			ReleaseCapture ();
		}

		in_mouse_avail = false;
	}
}

static qboolean
IN_InitDInput (void)
{
	HRESULT     hr;
	DIPROPDWORD dipdw = {
		{
			sizeof (DIPROPDWORD),			// diph.dwSize
			sizeof (DIPROPHEADER),			// diph.dwHeaderSize
			0,								// diph.dwObj
			DIPH_DEVICE,					// diph.dwHow
		},
		DINPUT_BUFFERSIZE,				// dwData
	};

	if (!hInstDI) {
		hInstDI = LoadLibrary ("dinput.dll");

		if (hInstDI == NULL) {
			Sys_Printf ("Couldn't load dinput.dll\n");
			return false;
		}
	}

	if (!pDirectInputCreate) {
		pDirectInputCreate =
			(void *) GetProcAddress (hInstDI, "DirectInputCreateA");

		if (!pDirectInputCreate) {
			Sys_Printf ("Couldn't get DI proc addr\n");
			return false;
		}
	}
	// register with DirectInput and get an IDirectInput to play with.
	hr = iDirectInputCreate (global_hInstance, DIRECTINPUT_VERSION, &g_pdi,
							 NULL);

	if (FAILED (hr))
		return false;
	// obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice (g_pdi, &GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED (hr)) {
		Sys_Printf ("Couldn't open DI mouse device\n");
		return false;
	}
	// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat (g_pMouse, &df);

	if (FAILED (hr)) {
		Sys_Printf ("Couldn't set DI mouse format\n");
		return false;
	}
	// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel (g_pMouse, win_mainwindow,
												 DISCL_EXCLUSIVE |
												 DISCL_FOREGROUND);

	if (FAILED (hr)) {
		Sys_Printf ("Couldn't set DI coop level\n");
		return false;
	}

	// set the buffer size to DINPUT_BUFFERSIZE elements.
	// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty (g_pMouse, DIPROP_BUFFERSIZE,
										 &dipdw.diph);

	if (FAILED (hr)) {
		Sys_Printf ("Couldn't set DI buffersize\n");
		return false;
	}

	return true;
}

static void
IN_StartupMouse (void)
{
//  HDC         hdc;

	if (COM_CheckParm ("-nomouse"))
		return;

	mouseinitialized = true;

	if (COM_CheckParm ("-dinput")) {
		dinput = IN_InitDInput ();

		if (dinput) {
			Sys_Printf ("DirectInput initialized\n");
		} else {
			Sys_Printf ("DirectInput not initialized\n");
		}
	}

	if (!dinput) {
		mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0,
												originalmouseparms, 0);

		if (mouseparmsvalid) {
			if (COM_CheckParm ("-noforcemspd"))
				newmouseparms[2] = originalmouseparms[2];

			if (COM_CheckParm ("-noforcemaccel")) {
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
			}

			if (COM_CheckParm ("-noforcemparms")) {
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
				newmouseparms[2] = originalmouseparms[2];
			}
		}
	}

	mouse_buttons = 32;

	// if a fullscreen video mode was set before the mouse was initialized,
	// set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}

static void
in_paste_buffer_f (void)
{
	HANDLE      th;
	char       *clipText;
	int         i;

	if (OpenClipboard (NULL)) {
		th = GetClipboardData (CF_TEXT);
		if (th) {
			clipText = GlobalLock (th);
			if (clipText) {
				for (i = 0; clipText[i]
							&& !strchr ("\n\r\b", clipText[i]); i++) {
					Key_Event (QFK_UNKNOWN, clipText[i], 1);
					Key_Event (QFK_UNKNOWN, 0, 0);
				}
			}
			GlobalUnlock (th);
		}
		CloseClipboard ();
	}
}

static void
win_keydest_callback (keydest_t key_dest)
{
	win_in_game = key_dest == key_game;
	if (win_in_game) {
		IN_ActivateMouse ();
		IN_HideMouse ();
	} else {
		IN_DeactivateMouse ();
		IN_ShowMouse ();
	}
}

void
IN_LL_Init (void)
{
	uiWheelMessage = RegisterWindowMessage ("MSWHEEL_ROLLMSG");

	IN_StartupMouse ();

	Key_KeydestCallback (win_keydest_callback);
	Cmd_AddCommand ("in_paste_buffer", in_paste_buffer_f,
					"Paste the contents of the C&P buffer to the console");
}

void
IN_LL_Init_Cvars (void)
{
}

void
IN_LL_Shutdown (void)
{

	IN_DeactivateMouse ();
	IN_ShowMouse ();

	if (g_pMouse) {
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
	}

	if (g_pdi) {
		IDirectInput_Release (g_pdi);
		g_pdi = NULL;
	}
}

static void
IN_MouseEvent (unsigned mstate)
{
	unsigned    i;

	if (in_mouse_avail && !dinput) {
		// perform button actions
		for (i = 0; i < mouse_buttons; i++) {
			if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) {
				Key_Event (QFM_BUTTON1 + i, -1, true);
			}

			if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
				Key_Event (QFM_BUTTON1 + i, -1, false);
			}
		}

		mouse_oldbuttonstate = mstate;
	}
}

void
IN_LL_Grab_Input (int grab)
{
}

void
IN_LL_ClearStates (void)
{
	if (in_mouse_avail) {
		mx_accum = 0;
		my_accum = 0;
		mouse_oldbuttonstate = 0;
	}
}

void
IN_LL_ProcessEvents (void)
{
	MSG         msg;
	int         mx, my;
//  HDC hdc;
	unsigned    i;
	DIDEVICEOBJECTDATA od;
	DWORD       dwElements;
	HRESULT     hr;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
		scr_skipupdate = 0;
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	if (!in_mouse_avail)
		return;

	if (dinput) {
		mx = 0;
		my = 0;

		for (;;) {
			dwElements = 1;

			hr = IDirectInputDevice_GetDeviceData (g_pMouse,
												   sizeof (DIDEVICEOBJECTDATA),
												   &od, &dwElements, 0);

			if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED)) {
				dinput_acquired = true;
				IDirectInputDevice_Acquire (g_pMouse);
				break;
			}

			/* Unable to read data or no data available */
			if (FAILED (hr) || dwElements == 0) {
				break;
			}

			/* Look at the element to see what happened */
			switch (od.dwOfs) {
				case DIMOFS_X:
					mx += od.dwData;
					break;

				case DIMOFS_Y:
					my += od.dwData;
					break;

				case DIMOFS_BUTTON0:
					if (od.dwData & 0x80)
						mstate_di |= 1;
					else
						mstate_di &= ~1;
					break;

				case DIMOFS_BUTTON1:
					if (od.dwData & 0x80)
						mstate_di |= (1 << 1);
					else
						mstate_di &= ~(1 << 1);
					break;

				case DIMOFS_BUTTON2:
					if (od.dwData & 0x80)
						mstate_di |= (1 << 2);
					else
						mstate_di &= ~(1 << 2);
					break;
			}
		}

		// perform button actions
		for (i = 0; i < mouse_buttons; i++) {
			if ((mstate_di & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) {
				Key_Event (QFM_BUTTON1 + i, -1, true);
			}

			if (!(mstate_di & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
				Key_Event (QFM_BUTTON1 + i, -1, false);
			}
		}

		mouse_oldbuttonstate = mstate_di;
	} else {
		GetCursorPos (&current_pos);
		mx = current_pos.x - window_center_x + mx_accum;
		my = current_pos.y - window_center_y + my_accum;
		mx_accum = 0;
		my_accum = 0;
	}

	in_mouse_x = mx;
	in_mouse_y = my;

	// if the mouse has moved, force it to the center, so there's room to move
	if (mx || my) {
		SetCursorPos (window_center_x, window_center_y);
	}
}

//==========================================================================

static unsigned short scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', QFK_BACKSPACE, 9,	// 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', 13, QFK_LCTRL, 'a', 's',	// 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', QFK_LSHIFT, '\\', 'z', 'x', 'c', 'v',	// 2
	'b', 'n', 'm', ',', '.', '/', QFK_RSHIFT, QFK_KP_MULTIPLY,
	QFK_LALT, ' ', QFK_CAPSLOCK, QFK_F1, QFK_F2, QFK_F3, QFK_F4, QFK_F5,	// 3
	QFK_F6, QFK_F7, QFK_F8, QFK_F9, QFK_F10, QFK_PAUSE, QFK_SCROLLOCK, QFK_KP7,
	QFK_KP8, QFK_KP9, QFK_KP_MINUS, QFK_KP4, QFK_KP5, QFK_KP6, QFK_KP_PLUS, QFK_KP1,	// 4
	QFK_KP2, QFK_KP3, QFK_KP0, QFK_KP_PERIOD, 0, 0, 0, QFK_F11,
	QFK_F12, 0, 0, 0, 0, 0, 0, 0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned short shift_scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', QFK_BACKSPACE, 9,	// 0
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', 13, QFK_LCTRL, 'A', 'S',	// 1
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', QFK_LSHIFT, '|', 'Z', 'X', 'C', 'V',	// 2
	'B', 'N', 'M', '<', '>', '?', QFK_RSHIFT, QFK_KP_MULTIPLY,
	QFK_LALT, ' ', QFK_CAPSLOCK, QFK_F1, QFK_F2, QFK_F3, QFK_F4, QFK_F5,	// 3
	QFK_F6, QFK_F7, QFK_F8, QFK_F9, QFK_F10, QFK_PAUSE, QFK_SCROLLOCK, QFK_KP7,
	QFK_KP8, QFK_KP9, QFK_KP_MINUS, QFK_KP4, QFK_KP5, QFK_KP6, QFK_KP_PLUS, QFK_KP1,	// 4
	QFK_KP2, QFK_KP3, QFK_KP0, QFK_KP_PERIOD, 0, 0, 0, QFK_F11,
	QFK_F12, 0, 0, 0, 0, 0, 0, 0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned short ext_scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',					// 0
	'7', '8', '9', '0', '-', '=', QFK_BACKSPACE, 9,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',					// 1
	'o', 'p', '[', ']', QFK_KP_ENTER, QFK_RCTRL, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',					// 2
	'\'', '`', QFK_LSHIFT, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', QFK_KP_DIVIDE, QFK_RSHIFT, '*',	// 3
	QFK_RALT, ' ', QFK_CAPSLOCK, QFK_F1, QFK_F2, QFK_F3, QFK_F4, QFK_F5,
	QFK_F6, QFK_F7, QFK_F8, QFK_F9, QFK_F10, QFK_NUMLOCK, 0, QFK_HOME,	// 4
	QFK_UP, QFK_PAGEUP, '-', QFK_LEFT, '5', QFK_RIGHT, '+', QFK_END,
	QFK_DOWN, QFK_PAGEDOWN, QFK_INSERT, QFK_DELETE, 0, 0, 0, QFK_F11,	// 5
	QFK_F12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned short shift_ext_scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', QFK_BACKSPACE, 9,	// 0
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', QFK_KP_ENTER, QFK_RCTRL, 'A', 'S',	// 1
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', QFK_LSHIFT, '|', 'Z', 'X', 'C', 'V',	// 2
	'B', 'N', 'M', '<', '>', QFK_KP_DIVIDE, QFK_RSHIFT, '*',
	QFK_RALT, ' ', QFK_CAPSLOCK, QFK_F1, QFK_F2, QFK_F3, QFK_F4, QFK_F5,	// 3
	QFK_F6, QFK_F7, QFK_F8, QFK_F9, QFK_F10, QFK_NUMLOCK, 0, QFK_HOME,
	QFK_UP, QFK_PAGEUP, '-', QFK_LEFT, '5', QFK_RIGHT, '+', QFK_END,	// 4
	QFK_DOWN, QFK_PAGEDOWN, QFK_INSERT, QFK_DELETE, 0, 0, 0, QFK_F11,
	QFK_F12, 0, 0, 0, 0, 0, 0, 0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

#define ROTL(x,n) (((x)<<(n))|(x)>>(32-n))

/*
	MapKey

	Map from windows to quake keynums
*/
static void
MapKey (unsigned int keycode, int press, int *k, int *u)
{
	int         extended;
	int         scan;
	int         key;
	int         uc;
	unsigned long mask = ~1L;
	static unsigned long shifts;

	extended = (keycode >> 24) & 1;
	scan = (keycode >> 16) & 255;

	if (scan > 127) {
		*u = *k = 0;
		return;
	}

	if (extended)
		key = ext_scantokey[scan];
	else
		key = scantokey[scan];

	if (shifts & 0x03) {
		if (extended)
			uc = shift_ext_scantokey[scan];
		else
			uc = shift_scantokey[scan];
	} else {
		if (extended)
			uc = ext_scantokey[scan];
		else
			uc = scantokey[scan];
	}

	if (uc > 255)
		uc = 0;

	switch (key) {
		case QFK_RSHIFT:
			shifts &= mask;
			shifts |= press;
			break;
		case QFK_LSHIFT:
			shifts &= ROTL(mask, 1);
			shifts |= ROTL(press, 1);
			break;
		case QFK_RCTRL:
			shifts &= ROTL(mask, 2);
			shifts |= ROTL(press, 2);
			break;
		case QFK_LCTRL:
			shifts &= ROTL(mask, 3);
			shifts |= ROTL(press, 3);
			break;
		default:
			break;
	}

	Sys_MaskPrintf (SYS_VID, "%08x %d %02x %02lx %04x %c\n",
					keycode, press, scan, shifts,
					key, uc > 32 && uc < 127 ? uc : '#');
	*k = key;
	*u = uc;
}

/*
	MAIN WINDOW
*/

/*
  AppActivate

  fActive - True if app is activating
  If the application is activating, then swap the system into SYSPAL_NOSTATIC
  mode so that our palettes will display correctly.
*/
void
AppActivate (BOOL fActive, BOOL minimize)
{
	static BOOL sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active) {
		S_BlockSound ();
		sound_active = false;
	} else if (ActiveApp && !sound_active) {
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive) {
		if (modestate == MS_FULLDIB) {
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (win_canalttab && vid_wassuspended) {
				vid_wassuspended = false;

				if (ChangeDisplaySettings (&win_gdevmode, CDS_FULLSCREEN) !=
					DISP_CHANGE_SUCCESSFUL) {
					IN_ShowMouse ();
					Sys_Error ("Couldn't set fullscreen DIB mode\n"
							   "(try upgrading your video drivers)\n (%lx)",
							   GetLastError());
				}
				ShowWindow (win_mainwindow, SW_SHOWNORMAL);

				// Fix for alt-tab bug in NVidia drivers
				MoveWindow(win_mainwindow, 0, 0, win_gdevmode.dmPelsWidth,
						   win_gdevmode.dmPelsHeight, false);
			}
		}
		else if ((modestate == MS_WINDOWED) && in_grab->int_val
				 && win_in_game) {
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	} else {
		if (modestate == MS_FULLDIB) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (win_canalttab) {
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		} else if ((modestate == MS_WINDOWED) && in_grab->int_val) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

/* main window procedure */
LONG WINAPI
MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG        lRet = 1;
	int         fActive, fMinimized, temp;
	int         key, unicode;

	if (uMsg == uiWheelMessage)
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg) {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow (win_mainwindow, SW_SHOWMINNOACTIVE);
			break;
		case WM_CREATE:
			break;

		case WM_MOVE:
			Win_UpdateWindowStatus ((int) LOWORD (lParam),
									(int) HIWORD (lParam));
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			MapKey (lParam, 1, &key, &unicode);
			Key_Event (key, unicode, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			MapKey (lParam, 0, &key, &unicode);
			Key_Event (key, unicode, false);
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;

		// this is complicated because Win32 seems to pack multiple mouse
		// events into one update sometimes, so we always check all states and
		// look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;
			if (wParam & MK_RBUTTON)
				temp |= 2;
			if (wParam & MK_MBUTTON)
				temp |= 4;
			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// It's delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD (wParam) > 0) {
				Key_Event (QFM_WHEEL_UP, -1, true);
				Key_Event (QFM_WHEEL_UP, -1, false);
			} else {
				Key_Event (QFM_WHEEL_DOWN, -1, true);
				Key_Event (QFM_WHEEL_DOWN, -1, false);
			}
			break;

		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox
				(win_mainwindow,
				 "Are you sure you want to quit?", "Confirm Exit",
				 MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES) {
				Sys_Quit ();
			}
			break;

		case WM_ACTIVATE:
			fActive = LOWORD (wParam);
			fMinimized = (BOOL) HIWORD (wParam);
			AppActivate (!(fActive == WA_INACTIVE), fMinimized);
			// fix leftover Alt from any Alt-Tab or the like that switched us
			// away
			IN_ClearStates ();
			break;

		case WM_DESTROY:
			if (win_mainwindow)
				DestroyWindow (win_mainwindow);
			PostQuitMessage (0);
			break;

		case MM_MCINOTIFY:
			//FIXME lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}
