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
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/input/event.h"

#include "compat.h"
#include "context_win.h"
#include "in_win.h"

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI * pDirectInputCreate) (HINSTANCE hinst, DWORD dwVersion,
									   LPDIRECTINPUT * lplpDirectInput,
									   LPUNKNOWN punkOuter);

// mouse local variables
unsigned uiWheelMessage = ~0u;
static unsigned  mouse_buttons;
static POINT current_pos;
static bool mouseinitialized;
static bool restore_spi;
static int  originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
static bool mouseparmsvalid, mouseactivatetoggle;
static bool mouseshowtoggle = 1;
static bool dinput_acquired;
static bool in_win_initialized;
static unsigned int mstate_di;

// misc locals
static LPDIRECTINPUT g_pdi;
static LPDIRECTINPUTDEVICE g_pMouse;

static HINSTANCE hInstDI;

static bool dinput;

static bool vid_wassuspended = false;
static bool win_in_game = false;

typedef struct win_device_s {
	const char *name;
	int         num_axes;
	int         num_buttons;
	in_axisinfo_t *axes;
	in_buttoninfo_t *buttons;
	void       *event_data;
	int         devid;
} win_device_t;

static int in_mouse_avail;

#define WIN_MOUSE_BUTTONS 32

static int win_driver_handle = -1;
static in_buttoninfo_t win_key_buttons[256];
static in_axisinfo_t win_mouse_axes[2];
static in_buttoninfo_t win_mouse_buttons[WIN_MOUSE_BUTTONS];
static const char *win_mouse_axis_names[] = {"M_X", "M_Y"};
static const char *win_mouse_button_names[] = {
	"M_BUTTON1",    "M_BUTTON2",  "M_BUTTON3",  "M_WHEEL_UP",
	"M_WHEEL_DOWN", "M_BUTTON6",  "M_BUTTON7",  "M_BUTTON8",
	"M_BUTTON9",    "M_BUTTON10", "M_BUTTON11", "M_BUTTON12",
	"M_BUTTON13",   "M_BUTTON14", "M_BUTTON15", "M_BUTTON16",
	"M_BUTTON17",   "M_BUTTON18", "M_BUTTON19", "M_BUTTON20",
	"M_BUTTON21",   "M_BUTTON22", "M_BUTTON23", "M_BUTTON24",
	"M_BUTTON25",   "M_BUTTON26", "M_BUTTON27", "M_BUTTON28",
	"M_BUTTON29",   "M_BUTTON30", "M_BUTTON31", "M_BUTTON32",
};

#define SIZE(x) (sizeof (x) / sizeof (x[0]))

static unsigned short scantokey[128] = {
//  0               1               2               3
//  4               5               6               7
//  8               9               A               B
//  C               D               E               F
	0,              QFK_ESCAPE,     '1',            '2',
	'3',            '4',            '5',            '6',
	'7',            '8',            '9',            '0',
	'-',            '=',            QFK_BACKSPACE,  QFK_TAB,	// 0
	'q',            'w',            'e',            'r',
	't',            'y',            'u',            'i',
	'o',            'p',            '[',            ']',
	QFK_RETURN,     QFK_LCTRL,      'a',            's',	// 1
	'd',            'f',            'g',            'h',
	'j',            'k',            'l',            ';',
	'\'',           '`',            QFK_LSHIFT,     '\\',
	'z',            'x',            'c',            'v',	// 2
	'b',            'n',            'm',            ',',
	'.',            '/',            QFK_RSHIFT,     QFK_KP_MULTIPLY,
	QFK_LALT,       ' ',            QFK_CAPSLOCK,   QFK_F1,
	QFK_F2,         QFK_F3,         QFK_F4,         QFK_F5,	// 3
	QFK_F6,         QFK_F7,         QFK_F8,         QFK_F9,
	QFK_F10,        QFK_PAUSE,      QFK_SCROLLOCK,  QFK_KP7,
	QFK_KP8,        QFK_KP9,        QFK_KP_MINUS,   QFK_KP4,
	QFK_KP5,        QFK_KP6,        QFK_KP_PLUS,    QFK_KP1,	// 4
	QFK_KP2,        QFK_KP3,        QFK_KP0,        QFK_KP_PERIOD,
	0,              0,              0,              QFK_F11,
	QFK_F12,        0,              0,              0,
	0,              0,              0,              0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static unsigned short shift_scantokey[128] = {
//  0               1               2               3
//  4               5               6               7
//  8               9               A               B
//  C               D               E               F
	0,              QFK_ESCAPE,     '!',            '@',		// 0
	'#',            '$',            '%',            '^',		// 0
	'&',            '*',            '(',            ')',		// 0
	'_',            '+',            QFK_BACKSPACE,  QFK_TAB,	// 0
	'Q',            'W',            'E',            'R',		// 1
	'T',            'Y',            'U',            'I',		// 1
	'O',            'P',            '{',            '}',		// 1
	QFK_RETURN,     QFK_LCTRL,      'A',            'S',		// 1
	'D',            'F',            'G',            'H',		// 2
	'J',            'K',            'L',            ':',		// 2
	'"',            '~',            QFK_LSHIFT,     '|',		// 2
	'Z',            'X',            'C',            'V',		// 2
	'B',            'N',            'M',            '<',		// 3
	'>',            '?',            QFK_RSHIFT,     QFK_KP_MULTIPLY,// 3
	QFK_LALT,       ' ',            QFK_CAPSLOCK,   QFK_F1,		// 3
	QFK_F2,         QFK_F3,         QFK_F4,         QFK_F5,		// 3
	QFK_F6,         QFK_F7,         QFK_F8,         QFK_F9,		// 4
	QFK_F10,        QFK_PAUSE,      QFK_SCROLLOCK,  QFK_KP7,	// 4
	QFK_KP8,        QFK_KP9,        QFK_KP_MINUS,   QFK_KP4,	// 4
	QFK_KP5,        QFK_KP6,        QFK_KP_PLUS,    QFK_KP1,	// 4
	QFK_KP2,        QFK_KP3,        QFK_KP0,        QFK_KP_PERIOD,//5
	0,              0,              0,              QFK_F11,	// 5
	QFK_F12,        0,              0,              0,			// 5
	0,              0,              0,              0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,				// 6
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,				// 7
};

static unsigned short ext_scantokey[128] = {
//  0               1               2               3
//  4               5               6               7
//  8               9               A               B
//  C               D               E               F
	0,              QFK_ESCAPE,     '1',            '2',
	'3',            '4',            '5',            '6',// 0
	'7',            '8',            '9',            '0',
	'-',            '=',            QFK_BACKSPACE,  QFK_TAB,
	'q',            'w',            'e',            'r',
	't',            'y',            'u',            'i',					// 1
	'o',            'p',            '[',            ']',
	QFK_KP_ENTER,     QFK_RCTRL,      'a',            's',
	'd',            'f',            'g',            'h',
	'j',            'k',            'l',            ';',					// 2
	'\'',            '`',            QFK_LSHIFT,    '\\',
	'z',            'x',            'c',            'v',
	'b',            'n',            'm',            ',',
	'.',            QFK_KP_DIVIDE,  QFK_RSHIFT,     '*',	// 3
	QFK_RALT,       ' ',            QFK_CAPSLOCK,   QFK_F1,
	QFK_F2,         QFK_F3,         QFK_F4,         QFK_F5,
	QFK_F6,         QFK_F7,         QFK_F8,         QFK_F9,
	QFK_F10,        QFK_NUMLOCK,    0,              QFK_HOME,	// 4
	QFK_UP,         QFK_PAGEUP,     '-',            QFK_LEFT,
	'5',            QFK_RIGHT,      '+',            QFK_END,
	QFK_DOWN,       QFK_PAGEDOWN,   QFK_INSERT,     QFK_DELETE,
	0,              0,              0,              QFK_F11,	// 5
	QFK_F12,        0,              0,              0,
	0,              0,              0,              0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static unsigned short shift_ext_scantokey[128] = {
//  0               1               2               3
//  4               5               6               7
//  8               9               A               B
//  C               D               E               F
	0,              QFK_ESCAPE,     '!',            '@',
	'#',            '$',            '%',            '^',
	'&',            '*',            '(',            ')',
	'_',            '+',            QFK_BACKSPACE,  QFK_ESCAPE,	// 0
	'Q',            'W',            'E',            'R',
	'T',            'Y',            'U',            'I',
	'O',            'P',            '{',            '}',
	QFK_KP_ENTER,   QFK_RCTRL,      'A',            'S',	// 1
	'D',            'F',            'G',            'H',
	'J',            'K',            'L',            ':',
	'"',            '~',            QFK_LSHIFT,     '|',
	'Z',            'X',            'C',            'V',	// 2
	'B',            'N',            'M',            '<',
	'>',            QFK_KP_DIVIDE,  QFK_RSHIFT,     '*',
	QFK_RALT,       ' ',            QFK_CAPSLOCK,   QFK_F1,
	QFK_F2,         QFK_F3,         QFK_F4,         QFK_F5,
	QFK_F6,         QFK_F7,         QFK_F8,         QFK_F9,
	QFK_F10,        QFK_NUMLOCK,    0,              QFK_HOME,	// 4
	QFK_UP,         QFK_PAGEUP,     '-',            QFK_LEFT,
	'5',            QFK_RIGHT,      '+',            QFK_END,
	QFK_DOWN,       QFK_PAGEDOWN,   QFK_INSERT,     QFK_DELETE,
	0,              0,              0,              QFK_F11,	// 5
	QFK_F12,        0,              0,              0,
	0,              0,              0,              0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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

	Sys_MaskPrintf (SYS_vid, "%08x %d %02x %02lx %04x %c\n",
					keycode, press, scan, shifts,
					key, uc > 32 && uc < 127 ? uc : '#');
	*k = key;
	*u = uc;
}

static win_device_t win_keyboard_device = {
	"core:keyboard",
	0, SIZE (win_key_buttons),
	0, win_key_buttons,
};

static win_device_t win_mouse_device = {
	"core:mouse",
	SIZE (win_mouse_axes), SIZE (win_mouse_buttons),
	win_mouse_axes, win_mouse_buttons,
};

static IE_mouse_event_t win_mouse;
static IE_key_event_t win_key;

static void
in_win_send_axis_event (int devid, in_axisinfo_t *axis)
{
	IE_event_t  event = {
		.type = ie_axis,
		.when = Sys_LongTime (),
		.axis = {
			.data = win_mouse_device.event_data,
			.devid = devid,
			.axis = axis->axis,
			.value = axis->value,
		},
	};
	IE_Send_Event (&event);
}

static int
in_win_send_mouse_event (IE_mouse_type type)
{
	IE_event_t  event = {
		.type = ie_mouse,
		.when = Sys_LongTime (),
		.mouse = win_mouse,
	};
	event.mouse.type = type;
	return IE_Send_Event (&event);
}

static int
in_win_send_key_event (void)
{
	IE_event_t  event = {
		.type = ie_key,
		.when = Sys_LongTime (),
		.key = win_key,
	};
	return IE_Send_Event (&event);
}

static void
in_win_send_button_event (int devid, in_buttoninfo_t *button, void *event_data)
{
	IE_event_t  event = {
		.type = ie_button,
		.when = Sys_LongTime (),
		.button = {
			.data = event_data,
			.devid = devid,
			.button = button->button,
			.state = button->state,
		},
	};
	IE_Send_Event (&event);
}

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
		ClipCursor (&win_rect);
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

			SetCursorPos (win_center_x, win_center_y);
			SetCapture (win_mainwindow);
			ClipCursor (&win_rect);
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

static bool
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

static int
IN_StartupMouse (void)
{
//  HDC         hdc;

	if (COM_CheckParm ("-nomouse"))
		return 0;

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

	mouse_buttons = WIN_MOUSE_BUTTONS;

	// if a fullscreen video mode was set before the mouse was initialized,
	// set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
	return 1;
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
				win_key.code = 0;
				for (i = 0; clipText[i]
							&& !strchr ("\n\r\b", clipText[i]); i++) {
					win_key.unicode = clipText[i];
					in_win_send_key_event ();
				}
			}
			GlobalUnlock (th);
		}
		CloseClipboard ();
	}
}
#if 0
static void
win_keydest_callback (keydest_t key_dest, void *data)
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
#endif

static void
win_add_device (win_device_t *dev)
{
	for (int i = 0; i < dev->num_axes; i++) {
		dev->axes[i].axis = i;
	}
	for (int i = 0; i < dev->num_buttons; i++) {
		dev->buttons[i].button = i;
	}
	dev->devid = IN_AddDevice (win_driver_handle, dev, dev->name, dev->name);
}

static void
in_win_init (void *data)
{
	uiWheelMessage = RegisterWindowMessage ("MSWHEEL_ROLLMSG");

	win_add_device (&win_keyboard_device);

	if (IN_StartupMouse ()) {
		win_add_device (&win_mouse_device);
	}

	//Key_KeydestCallback (win_keydest_callback, 0);
	Cmd_AddCommand ("in_paste_buffer", in_paste_buffer_f,
					"Paste the contents of the C&P buffer to the console");
	in_win_initialized = true;
}

static const char *
in_win_get_axis_name (void *data, void *device, int axis_num)
{
	win_device_t *dev = device;
	const char *name = 0;

	if (dev == &win_keyboard_device) {
		// keyboards don't have axes...
	} else if (dev == &win_mouse_device) {
		if ((unsigned) axis_num < SIZE (win_mouse_axis_names)) {
			name = win_mouse_axis_names[axis_num];
		}
	}
	return name;
}

static const char *
in_win_get_button_name (void *data, void *device, int button_num)
{
	win_device_t *dev = device;
	const char *name = 0;

	if (dev == &win_keyboard_device) {
		// FIXME
	} else if (dev == &win_mouse_device) {
		if ((unsigned) button_num < SIZE (win_mouse_button_names)) {
			name = win_mouse_button_names[button_num];
		}
	}
	return name;
}

static int
in_win_get_axis_num (void *data, void *device, const char *axis_name)
{
	win_device_t *dev = device;
	int         num = -1;

	if (dev == &win_keyboard_device) {
		// keyboards don't have axes...
	} else if (dev == &win_mouse_device) {
		for (size_t i = 0; i < SIZE (win_mouse_axis_names); i++) {
			if (strcasecmp (axis_name, win_mouse_axis_names[i]) == 0) {
				num = i;
				break;
			}
		}
	}
	return num;
}

static int
in_win_get_button_num (void *data, void *device, const char *button_name)
{
	win_device_t *dev = device;
	int         num = -1;

	if (dev == &win_keyboard_device) {
		// FIXME
	} else if (dev == &win_mouse_device) {
		for (size_t i = 0; i < SIZE (win_mouse_button_names); i++) {
			if (strcasecmp (button_name, win_mouse_button_names[i]) == 0) {
				num = i;
				break;
			}
		}
	}
	return num;
}

static void
in_win_shutdown (void *data)
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
in_win_set_device_event_data (void *device, void *event_data, void *data)
{
	win_device_t *dev = device;
	dev->event_data = event_data;
}

static void *
in_win_get_device_event_data (void *device, void *data)
{
	win_device_t *dev = device;
	return dev->event_data;
}

static void
event_motion (int dmx, int dmy, int mx, int my)
{
	win_mouse_axes[0].value = dmx;
	win_mouse_axes[1].value = dmy;

	win_mouse.shift = win_key.shift;
	win_mouse.x = mx;
	win_mouse.x = my;
	if (!in_win_send_mouse_event (ie_mousemove)) {
		in_win_send_axis_event (win_mouse_device.devid, &win_mouse_axes[0]);
		in_win_send_axis_event (win_mouse_device.devid, &win_mouse_axes[1]);
	}
}

static void
event_button (unsigned buttons)
{
	unsigned    mask = win_mouse.buttons ^ buttons;

	if (!mask) {
		// no change
		return;
	}

	// FIXME this won't be right if multiple buttons change state
	int press = buttons & mask;

	for (int i = 0; i < WIN_MOUSE_BUTTONS; i++) {
		win_mouse_buttons[i].state = buttons & (1 << i);
	}

	win_mouse.buttons = buttons;
	if (!in_win_send_mouse_event (press ? ie_mousedown : ie_mouseup)) {
		for (int i = 0; i < WIN_MOUSE_BUTTONS; i++) {
			if (!(mask & (1 << i))) {
				continue;
			}
			in_win_send_button_event (win_mouse_device.devid,
									  &win_mouse_buttons[i],
									  win_mouse_device.event_data);
		}
	}
}

static void
event_key (LPARAM keydata, int pressed)
{
	int         extended = (keydata >> 24) & 1;
	// This assumes windows key codes are really only 7 bits (should be, since
	// they seem to be regular scan codes)
	int         scan = (keydata >> 16) & 0x7f;
	int         key = (extended << 7) | scan;
	MapKey (keydata, pressed, &win_key.code, &win_key.unicode);
	//FIXME windows key codes and x11 key code's don't match, so binding
	//configs are not cross-platform (is this actually a problem?)
	win_key_buttons[key].state = pressed;
	if (!pressed || !in_win_send_key_event ()) {
		in_win_send_button_event (win_keyboard_device.devid,
								  &win_key_buttons[key],
								  win_keyboard_device.event_data);
	}
}

static void
in_win_clear_states (void *data)
{
}

static void
in_win_process_events (void *data)
{
	MSG         msg;
	int         mx, my;
//  HDC hdc;
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

		event_button (mstate_di);
	} else {
		GetCursorPos (&current_pos);
		mx = current_pos.x - win_center_x;
		my = current_pos.y - win_center_y;
	}

	// if the mouse has moved, force it to the center, so there's room to move
	if (mx || my) {
		//FIXME abs pos
		event_motion (mx, my, 0, 0);
		SetCursorPos (win_center_x, win_center_y);
	}
}

static void
in_win_axis_info (void *data, void *device, in_axisinfo_t *axes, int *numaxes)
{
	win_device_t *dev = device;
	if (!axes) {
		*numaxes = dev->num_axes;
		return;
	}
	if (*numaxes > dev->num_axes) {
		*numaxes = dev->num_axes;
	}
	memcpy (axes, dev->axes, *numaxes * sizeof (in_axisinfo_t));
}

static void
in_win_button_info (void *data, void *device, in_buttoninfo_t *buttons,
					int *numbuttons)
{
	win_device_t *dev = device;
	if (!buttons) {
		*numbuttons = dev->num_buttons;
		return;
	}
	if (*numbuttons > dev->num_buttons) {
		*numbuttons = dev->num_buttons;
	}
	memcpy (buttons, dev->buttons, *numbuttons * sizeof (in_buttoninfo_t));
}

//==========================================================================

/*
	MAIN WINDOW
*/

/*
  fActive - True if app is activating
  If the application is activating, then swap the system into SYSPAL_NOSTATIC
  mode so that our palettes will display correctly.
*/
void
Win_Activate (BOOL active, BOOL minimize)
{
	static BOOL sound_active;

	win_minimized = minimize;

	// enable/disable sound on focus gain/loss
	if (!active && sound_active) {
		S_BlockSound ();
		sound_active = false;
	} else if (active && !sound_active) {
		S_UnblockSound ();
		sound_active = true;
	}

	if (active) {
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
		else if ((modestate == MS_WINDOWED) && in_grab
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
		} else if ((modestate == MS_WINDOWED) && in_grab) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

static void
in_win_send_focus_event (int gain)
{
	IE_event_t  event = {
		.type = gain ? ie_app_gain_focus : ie_app_lose_focus,
		.when = Sys_LongTime (),
	};
	IE_Send_Event (&event);
}

static LONG
event_focusin (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	in_win_send_focus_event (1);
	return 1;
}

static LONG
event_focusout (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (modestate == MS_FULLDIB) {
		ShowWindow (win_mainwindow, SW_SHOWMINNOACTIVE);
	}
	in_win_send_focus_event (0);
	return 1;
}

static LONG
event_keyup (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	event_key (lParam, 0);
	return 1;
}

static LONG
event_keydown (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	event_key (lParam, 1);
	return 1;
}

static LONG
event_syschar (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// absorb Alt-Space
	return 1;
}

static LONG
event_mouse (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// this is complicated because Win32 seems to pack multiple mouse
	// events into one update sometimes, so we always check all states and
	// look for events
	unsigned    temp = 0;

	if (wParam & MK_LBUTTON)
		temp |= 1;
	if (wParam & MK_RBUTTON)
		temp |= 2;
	if (wParam & MK_MBUTTON)
		temp |= 4;
	event_button (temp);
	return 1;
}

static LONG
event_mousewheel (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// JACK: This is the mouse wheel with the Intellimouse
	// It's delta is either positive or neg, and we generate the proper
	// Event.
	unsigned    temp = win_mouse.buttons & ~((1 << 3) | (1 << 4));;
	if ((short) HIWORD (wParam) > 0) {
		event_button (temp | (1 << 3));
	} else {
		event_button (temp | (1 << 4));
	}
	event_button (temp);
	return 1;
}

static LONG
event_close (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (MessageBox (win_mainwindow,
					"Are you sure you want to quit?", "Confirm Exit",
					MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES) {
		Sys_Quit ();
	}
	return 1;
}

static long
event_activate (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         fActive = LOWORD (wParam);
	int         fMinimized = (BOOL) HIWORD (wParam);
	Win_Activate (!(fActive == WA_INACTIVE), fMinimized);
	// fix leftover Alt from any Alt-Tab or the like that switched us away
	if (in_win_initialized) {
		IN_ClearStates ();
	}
	return 1;
}

void
IN_Win_Preinit (void)
{
	Win_AddEvent (WM_SETFOCUS, event_focusin);
	Win_AddEvent (WM_SETFOCUS, event_focusout);

	Win_AddEvent (WM_KEYDOWN, event_keydown);
	Win_AddEvent (WM_SYSKEYDOWN, event_keydown);
	Win_AddEvent (WM_KEYUP, event_keyup);
	Win_AddEvent (WM_SYSKEYUP, event_keyup);
	Win_AddEvent (WM_SYSCHAR, event_syschar);

	Win_AddEvent (WM_LBUTTONDOWN, event_mouse);
	Win_AddEvent (WM_LBUTTONUP, event_mouse);
	Win_AddEvent (WM_RBUTTONDOWN, event_mouse);
	Win_AddEvent (WM_RBUTTONUP, event_mouse);
	Win_AddEvent (WM_MBUTTONDOWN, event_mouse);
	Win_AddEvent (WM_MBUTTONUP, event_mouse);
	Win_AddEvent (WM_MOUSEMOVE, event_mouse);

	Win_AddEvent (WM_MOUSEWHEEL, event_mousewheel);

	Win_AddEvent (WM_CLOSE, event_close);

	Win_AddEvent (WM_ACTIVATE, event_activate);
}

static in_driver_t in_win_driver = {
	.init = in_win_init,
	.shutdown = in_win_shutdown,
	.set_device_event_data = in_win_set_device_event_data,
	.get_device_event_data = in_win_get_device_event_data,
	.process_events = in_win_process_events,
	.clear_states = in_win_clear_states,
	//.grab_input = in_win_grab_input,

	.axis_info = in_win_axis_info,
	.button_info = in_win_button_info,

	.get_axis_name = in_win_get_axis_name,
	.get_button_name = in_win_get_button_name,
	.get_axis_num = in_win_get_axis_num,
	.get_button_num = in_win_get_button_num,
};

static void __attribute__((constructor))
in_win_register_driver (void)
{
	win_driver_handle = IN_RegisterDriver (&in_win_driver, 0);
}


int win_force_link;
