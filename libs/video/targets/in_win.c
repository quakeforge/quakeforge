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

	$Id$
*/

// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dinput.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "compat.h"
#include "winquake.h"

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI * pDirectInputCreate) (HINSTANCE hinst, DWORD dwVersion,
									   LPDIRECTINPUT * lplpDirectInput,
									   LPUNKNOWN punkOuter);

// mouse public variables
unsigned int uiWheelMessage;

// mouse local variables
static int  mouse_buttons;
static int  mouse_oldbuttonstate;
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

// forward-referenced functions, joy
extern void JOY_Command(void);
extern void JOY_Init_Cvars(void);
extern void JOY_Init (void);
extern void JOY_AdvancedUpdate_f (void);
extern void JOY_Move (void);


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
			SetCapture (mainwindow);
			ClipCursor (&window_rect);
		}

		in_mouse_avail = true;
	}
}

void
IN_SetQuakeMouseState (void)
{
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
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

void
IN_RestoreOriginalMouseState (void)
{
	if (mouseactivatetoggle) {
		IN_DeactivateMouse ();
		mouseactivatetoggle = true;
	}
	// try to redraw the cursor so it gets reinitialized, because sometimes it
	// has garbage after the mode switch
	ShowCursor (TRUE);
	ShowCursor (FALSE);
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
			Con_Printf ("Couldn't load dinput.dll\n");
			return false;
		}
	}

	if (!pDirectInputCreate) {
		pDirectInputCreate =
			(void *) GetProcAddress (hInstDI, "DirectInputCreateA");

		if (!pDirectInputCreate) {
			Con_Printf ("Couldn't get DI proc addr\n");
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
		Con_Printf ("Couldn't open DI mouse device\n");
		return false;
	}
	// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat (g_pMouse, &df);

	if (FAILED (hr)) {
		Con_Printf ("Couldn't set DI mouse format\n");
		return false;
	}
	// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel (g_pMouse, mainwindow,
												 DISCL_EXCLUSIVE |
												 DISCL_FOREGROUND);

	if (FAILED (hr)) {
		Con_Printf ("Couldn't set DI coop level\n");
		return false;
	}

	// set the buffer size to DINPUT_BUFFERSIZE elements.
	// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty (g_pMouse, DIPROP_BUFFERSIZE,
										 &dipdw.diph);

	if (FAILED (hr)) {
		Con_Printf ("Couldn't set DI buffersize\n");
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
			Con_Printf ("DirectInput initialized\n");
		} else {
			Con_Printf ("DirectInput not initialized\n");
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

	mouse_buttons = 3;

	// if a fullscreen video mode was set before the mouse was initialized,
	// set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}

void
IN_LL_Init (void)
{
	uiWheelMessage = RegisterWindowMessage ("MSWHEEL_ROLLMSG");

	IN_StartupMouse ();
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

void
IN_MouseEvent (int mstate)
{
	int         i;

	if (in_mouse_avail && !dinput) {
		// perform button actions
		for (i = 0; i < mouse_buttons; i++) {
			if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) {
				Key_Event (M_BUTTON1 + i, -1, true);
			}

			if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
				Key_Event (M_BUTTON1 + i, -1, false);
			}
		}

		mouse_oldbuttonstate = mstate;
	}
}

void
IN_LL_Commands (void)
{
	int         mx, my;

//  HDC hdc;
	int         i;
	DIDEVICEOBJECTDATA od;
	DWORD       dwElements;
	HRESULT     hr;

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
				Key_Event (M_BUTTON1 + i, -1, true);
			}

			if (!(mstate_di & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
				Key_Event (M_BUTTON1 + i, -1, false);
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
IN_LL_SendKeyEvents (void)
{   
	MSG         msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
}
