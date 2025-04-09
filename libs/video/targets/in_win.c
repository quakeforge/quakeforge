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
#include "vid_internal.h"

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI * pDirectInputCreate) (HINSTANCE hinst, DWORD dwVersion,
									   LPDIRECTINPUT * lplpDirectInput,
									   LPUNKNOWN punkOuter);

// mouse local variables
unsigned uiWheelMessage = ~0u;
//static unsigned  mouse_buttons;
static POINT current_pos;
static bool mouseinitialized;
static bool restore_spi;
static int  originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
static bool mouseparmsvalid, mouseactivatetoggle;
static bool dinput_acquired;
static bool in_win_initialized;
static unsigned int mstate_di;

// misc locals
static LPDIRECTINPUT g_pdi;
static LPDIRECTINPUTDEVICE g_pMouse;

//static HINSTANCE hInstDI;

static bool dinput;

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
static in_buttoninfo_t win_key_buttons[512];
static int win_key_scancode;
static bool win_mouse_enter = true;
static bool win_input_grabbed;
static bool win_warped_mouse;
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

static unsigned short scantokey[512] = {
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
[0x130] =
	0,				0,				0,				0,
	0,				0,				0,				QFK_PRINT,
[0x140] =
	0,				0,				0,				0,
	0,				0,				0,				QFK_HOME,
	QFK_UP,			QFK_PAGEUP,		0,				QFK_LEFT,
	0,				QFK_RIGHT,		0,				QFK_END,
[0x150] =
	QFK_DOWN,		QFK_PAGEDOWN,	QFK_INSERT,		QFK_DELETE,
};

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

void
IN_UpdateClipCursor (void)
{
	if (mouseinitialized && in_mouse_avail && !dinput) {
		ClipCursor (&win_rect);
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
	win_add_device (&win_keyboard_device);
	win_add_device (&win_mouse_device);

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

static LONG
event_key (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	bool        pressed = !(HIWORD(lParam) & KF_UP);
	int         scancode = HIWORD (lParam) & (KF_EXTENDED | 0xff);
	unsigned    vkey = (UINT) wParam;
	bool        translated = lParam & (1 << 29);

	if (!scancode) {
		scancode = MapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);
	}
	unsigned    mask = 0;
	switch (vkey) {
		case VK_SHIFT: mask = ies_shift; break;
		case VK_CONTROL: mask = ies_control; break;
		case VK_MENU: mask = ies_alt; break;
	}
	if (pressed) {
		win_key.shift |= mask;
	} else {
		win_key.shift &= ~mask;
	}
	win_key.code = scantokey[scancode];
	Sys_MaskPrintf (SYS_input, "key: %3x %3d\n", scancode, win_key.code);
	win_key.unicode = 0;
	win_key_buttons[scancode].state = pressed;
	win_key_scancode = scancode;
	if (!pressed || !translated) {
		// always handle releases, but not translated presses (handled by
		// event_char())
		if (!(pressed && in_win_send_key_event ())) {
			in_win_send_button_event (win_keyboard_device.devid,
									  &win_key_buttons[scancode],
									  win_keyboard_device.event_data);
		}
	}
	return 0;
}

static LONG
event_char (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_key.unicode = wParam;
	if (!in_win_send_key_event ()) {
		// WM_CHAR events occur only for presses
		in_win_send_button_event (win_keyboard_device.devid,
								  &win_key_buttons[win_key_scancode],
								  win_keyboard_device.event_data);
	}
	return 0;
}

static void
in_win_clear_states (void *data)
{
}

static void
in_win_grab_input (void *data, int grab)
{
	if (!win_mainwindow) {
		return;
	}

	if ((win_input_grabbed = grab)) {
		SetCapture (win_mainwindow);
		ClipCursor (&win_rect);
	} else {
		ClipCursor (NULL);
		ReleaseCapture ();
	}
}

static void
in_win_process_events (void *data)
{
	MSG         msg, cmsg;
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
		if (msg.message == WM_KEYDOWN
			&& PeekMessage (&cmsg, NULL, 0, 0, PM_NOREMOVE)
			&& (cmsg.message == WM_CHAR || cmsg.message == WM_SYSCHAR)) {
			msg.lParam |= 1u << 29;
		}
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

		//event_button (mstate_di);
	} else {
		GetCursorPos (&current_pos);
		mx = current_pos.x - win_center_x;
		my = current_pos.y - win_center_y;
	}

	// if the mouse has moved, force it to the center, so there's room to move
	if (mx || my) {
		//FIXME abs pos
//		event_motion (mx, my, 0, 0);
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
event_leave (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_mouse_enter = true;
	return 0;
}

static LONG
event_focusin (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_focused = true;
	in_win_send_focus_event (1);
	return 0;
}

static LONG
event_focusout (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	win_focused = false;
	in_win_send_focus_event (0);
	return 0;
}

static LONG
event_syschar (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// absorb Alt-Space
	return 0;
}

static void
event_button (bool press, int but, int x, int y)
{
	win_mouse_buttons[but].state = press;

	win_mouse.shift = win_key.shift;
	win_mouse.x = x;
	win_mouse.y = y;
	if (press) {
		win_mouse.buttons |= 1 << but;
	} else {
		win_mouse.buttons &= ~(1 << but);
	}
	if (!in_win_send_mouse_event (press ? ie_mousedown : ie_mouseup)) {
		in_win_send_button_event (win_mouse_device.devid,
								  &win_mouse_buttons[but],
								  win_mouse_device.event_data);
	}
}

static LONG
event_button_left (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         x = LOWORD (lParam);
	int         y = HIWORD (lParam);
	event_button (uMsg != WM_LBUTTONUP, 0, x, y);
	return 0;
}

static LONG
event_button_right (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         x = LOWORD (lParam);
	int         y = HIWORD (lParam);
	event_button (uMsg != WM_RBUTTONUP, 2, x, y);
	return 0;
}

static LONG
event_button_mid (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         x = LOWORD (lParam);
	int         y = HIWORD (lParam);
	event_button (uMsg != WM_MBUTTONUP, 1, x, y);
	return 0;
}

static LONG
event_button_X (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         x = LOWORD (lParam);
	int         y = HIWORD (lParam);
	int         but = HIWORD (wParam) + 7;
	event_button (uMsg != WM_XBUTTONUP, but, x, y);
	return 0;
}

static LONG
event_mouse (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// this is complicated because Win32 seems to pack multiple mouse
	// events into one update sometimes, so we always check all states and
	// look for events
	int         x = (short) LOWORD (lParam);
	int         y = (short) HIWORD (lParam);

	if (win_mouse_enter) {
		win_mouse_enter = false;
		TRACKMOUSEEVENT track = {
			.cbSize = sizeof (track),
			.dwFlags = TME_LEAVE,
			.hwndTrack = win_mainwindow,
		};
		TrackMouseEvent (&track);

		win_mouse.x = x;
		win_mouse.y = y;

		if (!win_cursor_visible) {
			SetCursor (0);
		}
	}

	if (win_input_grabbed) {
		if (!win_warped_mouse) {
			int         center_x = viddef.width / 2;
			int         center_y = viddef.height / 2;
			unsigned    dist_x = abs (center_x - x);
			unsigned    dist_y = abs (center_y - y);

			win_mouse_axes[0].value = x - win_mouse.x;
			win_mouse_axes[1].value = y - win_mouse.y;
			if (dist_x > viddef.width / 4 || dist_y > viddef.height / 4) {
				SetCursorPos (win_center_x, win_center_y);
				win_warped_mouse = true;
			}
		} else {
			win_warped_mouse = false;
		}
	} else {
		win_mouse_axes[0].value = x - win_mouse.x;
		win_mouse_axes[1].value = y - win_mouse.y;
	}
	win_mouse.shift = win_key.shift;
	win_mouse.x = x;
	win_mouse.y = y;
	if (!in_win_send_mouse_event (ie_mousemove)) {
		in_win_send_axis_event (win_mouse_device.devid, &win_mouse_axes[0]);
		in_win_send_axis_event (win_mouse_device.devid, &win_mouse_axes[1]);
	}
	return 0;
}

static LONG
event_mousewheel (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int         x = LOWORD (lParam);
	int         y = HIWORD (lParam);
	short       w = HIWORD (wParam);
	int         button = w < 0 ? 5 : 4;
	// FIXME should (also) treat as an axis
	event_button (true, button, x, y);
	event_button (false, button, x, y);
	return 0;
}

static long
event_activate (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// fix leftover Alt from any Alt-Tab or the like that switched us away
	if (in_win_initialized) {
		IN_ClearStates ();
	}
	return 0;
}

void
IN_Win_Preinit (void)
{
	Win_AddEvent (WM_MOUSELEAVE, event_leave);

	Win_AddEvent (WM_SETFOCUS, event_focusin);
	Win_AddEvent (WM_KILLFOCUS, event_focusout);

	Win_AddEvent (WM_KEYDOWN, event_key);
	Win_AddEvent (WM_SYSKEYDOWN, event_key);
	Win_AddEvent (WM_KEYUP, event_key);
	Win_AddEvent (WM_SYSKEYUP, event_key);
	Win_AddEvent (WM_CHAR, event_char);
	Win_AddEvent (WM_SYSCHAR, event_syschar);

	Win_AddEvent (WM_LBUTTONDOWN, event_button_left);
	Win_AddEvent (WM_LBUTTONUP, event_button_left);
	Win_AddEvent (WM_RBUTTONDOWN, event_button_right);
	Win_AddEvent (WM_RBUTTONUP, event_button_right);
	Win_AddEvent (WM_MBUTTONDOWN, event_button_mid);
	Win_AddEvent (WM_MBUTTONUP, event_button_mid);
	Win_AddEvent (WM_XBUTTONDOWN, event_button_X);
	Win_AddEvent (WM_XBUTTONUP, event_button_X);
	Win_AddEvent (WM_MOUSEMOVE, event_mouse);

	Win_AddEvent (WM_MOUSEWHEEL, event_mousewheel);

	Win_AddEvent (WM_ACTIVATE, event_activate);
}

static in_driver_t in_win_driver = {
	.init = in_win_init,
	.shutdown = in_win_shutdown,
	.set_device_event_data = in_win_set_device_event_data,
	.get_device_event_data = in_win_get_device_event_data,
	.process_events = in_win_process_events,
	.clear_states = in_win_clear_states,
	.grab_input = in_win_grab_input,

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
