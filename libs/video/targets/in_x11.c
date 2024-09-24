/*
	in_x11.c

	general x11 input driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define _BSD
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Sunkeysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_DGA
# ifdef DGA_OLD_HEADERS
#  include <X11/extensions/xf86dga.h>
# else
#  include <X11/extensions/Xxf86dga.h>
# endif
#endif
#ifdef HAVE_XI2
# include <X11/extensions/XInput2.h>
#endif
#ifdef HAVE_XFIXES
# include <X11/extensions/Xfixes.h>
#endif

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/input/event.h"

#include "compat.h"
#include "context_x11.h"
#include "dga_check.h"
#include "in_x11.h"
#include "qfselect.h"
#include "vid_internal.h"

typedef struct x11_device_s {
	const char *name;
	int         num_axes;
	int         num_buttons;
	in_axisinfo_t *axes;
	in_buttoninfo_t *buttons;
	void       *event_data;
	int         devid;
} x11_device_t;

#define X11_MOUSE_BUTTONS 32
#define X11_MOUSE_AXES 2
// The X11 protocol supports only 256 keys
static in_buttoninfo_t x11_key_buttons[256];
// X11 mice have only two axes (FIXME NOT always true (XInput says otherwise!)
static in_axisinfo_t x11_mouse_axes[X11_MOUSE_AXES];
// FIXME assume up to 32 mouse buttons (see X11_MOUSE_BUTTONS)
static in_buttoninfo_t x11_mouse_buttons[X11_MOUSE_BUTTONS];
static const char *x11_mouse_axis_names[] = {"M_X", "M_Y"};
static const char *x11_mouse_button_names[] = {
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

static x11_device_t x11_keyboard_device = {
	"core:keyboard",
	0, SIZE (x11_key_buttons),
	0, x11_key_buttons,
};

static x11_device_t x11_mouse_device = {
	"core:mouse",
	SIZE (x11_mouse_axes), SIZE (x11_mouse_buttons),
	x11_mouse_axes, x11_mouse_buttons,
};

int in_auto_focus;
static cvar_t in_auto_focus_cvar = {
	.name = "in_auto_focus",
	.description =
		"grab input focus when the mouse enters the window when using xinput2 "
		"with certain window managers using focus-follows-mouse (eg, openbox)",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &in_auto_focus },
};
int in_snd_block;
static cvar_t in_snd_block_cvar = {
	.name = "in_snd_block",
	.description =
		"block sound output on window focus loss",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &in_snd_block },
};
int in_dga;
static cvar_t in_dga_cvar = {
	.name = "in_dga",
	.description =
		"DGA Input support",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &in_dga },
};
int in_mouse_accel;
static cvar_t in_mouse_accel_cvar = {
	.name = "in_mouse_accel",
	.description =
		"set to 0 to remove mouse acceleration",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &in_mouse_accel },
};

static bool dga_avail;
static bool dga_active;
static IE_mouse_event_t x11_mouse;
static IE_key_event_t x11_key;
static int input_grabbed = 0;

#ifdef HAVE_XI2
static int xi_opcode;
#endif
#ifdef HAVE_XFIXES
static int xf_opcode;
static PointerBarrier x11_left_barrier;
static PointerBarrier x11_right_barrier;
static PointerBarrier x11_top_barrier;
static PointerBarrier x11_bottom_barrier;
#endif
static IE_app_window_event_t x11_aw;
static int x11_have_xi;
static int x11_fd;
static int x11_driver_handle = -1;
static int x11_event_handler_id;
static bool x11_have_pointer;

static void
dga_on (void)
{
#ifdef HAVE_DGA
	if (dga_avail && !dga_active) {
		int ret;
		ret = XF86DGADirectVideo (x_disp, DefaultScreen (x_disp),
								  XF86DGADirectMouse);
		Sys_MaskPrintf (SYS_vid, "XF86DGADirectVideo returned %d\n", ret);
		if (ret)
			dga_active = true;
	}
#endif
}

static void
dga_off (void)
{
#ifdef HAVE_DGA
	if (dga_avail && dga_active) {
		int ret;
		ret = XF86DGADirectVideo (x_disp, DefaultScreen (x_disp), 0);
		Sys_MaskPrintf (SYS_vid, "XF86DGADirectVideo returned %d\n", ret);
		if (ret)
			dga_active = false;
	}
#endif
}

static void
in_dga_f (void *data, const cvar_t *cvar)
{
	if (in_dga && input_grabbed) {
		Sys_MaskPrintf (SYS_vid, "VID: in_dga_f on\n");
		dga_on ();
	} else {
		Sys_MaskPrintf (SYS_vid, "VID: in_dga_f off\n");
		dga_off ();
	}
}

static void
in_mouse_accel_f (void *data, const cvar_t *cvar)
{
	if (in_mouse_accel) {
		X11_RestoreMouseAcceleration ();
	} else {
		X11_SaveMouseAcceleration ();
		X11_RemoveMouseAcceleration ();
	}
}

static void
in_paste_buffer_f (void)
{
	Atom        property;

	if (XGetSelectionOwner (x_disp, XA_PRIMARY) == None)
		return;
	property = XInternAtom (x_disp, "GETCLIPBOARDDATA_PROP", False);
	XConvertSelection (x_disp, XA_PRIMARY, XA_STRING, property, x_win, x_time);
	XFlush (x_disp);
}

static void
enter_notify (XEvent *event)
{
	x11_have_pointer = true;
	x_time = event->xcrossing.time;

	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
	if (!x_have_focus && in_auto_focus) {
		XSetInputFocus (x_disp, x_win, RevertToPointerRoot, CurrentTime);
	}
}

static void
leave_notify (XEvent *event)
{
	x11_have_pointer = false;
}

static void
XLateKey (XKeyEvent *ev, int *k, int *u)
{
	char        buffer[4];
	int         unicode;
	int         key = 0;
	KeySym      keysym, shifted_keysym;
	XComposeStatus compose;

	keysym = XLookupKeysym (ev, 0);
	XLookupString (ev, buffer, sizeof(buffer), &shifted_keysym, &compose);
	unicode = (byte) buffer[0];

	switch (keysym) {
		case XK_KP_Page_Up:
			key = QFK_KP9;
			break;
		case XK_Page_Up:
			key = QFK_PAGEUP;
			break;

		case XK_KP_Page_Down:
			key = QFK_KP3;
			break;
		case XK_Page_Down:
			key = QFK_PAGEDOWN;
			break;

		case XK_KP_Home:
			key = QFK_KP7;
			break;
		case XK_Home:
			key = QFK_HOME;
			break;

		case XK_KP_End:
			key = QFK_KP1;
			break;
		case XK_End:
			key = QFK_END;
			break;

		case XK_KP_Left:
			key = QFK_KP4;
			break;
		case XK_Left:
			key = QFK_LEFT;
			break;

		case XK_KP_Right:
			key = QFK_KP6;
			break;
		case XK_Right:
			key = QFK_RIGHT;
			break;

		case XK_KP_Down:
			key = QFK_KP2;
			break;
		case XK_Down:
			key = QFK_DOWN;
			break;

		case XK_KP_Up:
			key = QFK_KP8;
			break;
		case XK_Up:
			key = QFK_UP;
			break;

		case XK_Escape:
			key = QFK_ESCAPE;
			break;

		case XK_KP_Enter:
			key = QFK_KP_ENTER;
			break;
		case XK_Return:
			key = QFK_RETURN;
			break;

		case XK_Tab:
			key = QFK_TAB;
			break;

		case XK_F1:
			key = QFK_F1;
			break;
		case XK_F2:
			key = QFK_F2;
			break;
		case XK_F3:
			key = QFK_F3;
			break;
		case XK_F4:
			key = QFK_F4;
			break;
		case XK_F5:
			key = QFK_F5;
			break;
		case XK_F6:
			key = QFK_F6;
			break;
		case XK_F7:
			key = QFK_F7;
			break;
		case XK_F8:
			key = QFK_F8;
			break;
		case XK_F9:
			key = QFK_F9;
			break;
		case XK_F10:
			key = QFK_F10;
			break;
		case XK_F11:
			key = QFK_F11;
			break;
		case XK_F12:
			key = QFK_F12;
			break;

		case XK_BackSpace:
			key = QFK_BACKSPACE;
			break;

		case XK_KP_Delete:
			key = QFK_KP_PERIOD;
			break;
		case XK_Delete:
			key = QFK_DELETE;
			break;

		case XK_Pause:
			key = QFK_PAUSE;
			break;

		case XK_Shift_L:
			key = QFK_LSHIFT;
			break;
		case XK_Shift_R:
			key = QFK_RSHIFT;
			break;

		case XK_Execute:
		case XK_Control_L:
			key = QFK_LCTRL;
			break;
		case XK_Control_R:
			key = QFK_RCTRL;
			break;

		case XK_Mode_switch:
		case XK_Alt_L:
			key = QFK_LALT;
			break;
		case XK_Meta_L:
			key = QFK_LMETA;
			break;
		case XK_Alt_R:
			key = QFK_RALT;
			break;
		case XK_Meta_R:
			key = QFK_RMETA;
			break;
		case XK_Super_L:
			key = QFK_LSUPER;
			break;
		case XK_Super_R:
			key = QFK_RSUPER;
			break;

		case XK_Multi_key:
			key = QFK_COMPOSE;
			break;

		case XK_Menu:
			key = QFK_MENU;
			break;

		case XK_Caps_Lock:
			key = QFK_CAPSLOCK;
			break;

		case XK_KP_Begin:
			key = QFK_KP5;
			break;

		case XK_Print:
			key = QFK_PRINT;
			break;

		case XK_Scroll_Lock:
			key = QFK_SCROLLOCK;
			break;

		case XK_Num_Lock:
			key = QFK_NUMLOCK;
			break;

		case XK_Insert:
			key = QFK_INSERT;
			break;
		case XK_KP_Insert:
			key = QFK_KP0;
			break;

		case XK_KP_Multiply:
			key = QFK_KP_MULTIPLY;
			break;
		case XK_KP_Add:
			key = QFK_KP_PLUS;
			break;
		case XK_KP_Subtract:
			key = QFK_KP_MINUS;
			break;
		case XK_KP_Divide:
			key = QFK_KP_DIVIDE;
			break;

		// For Sun keyboards
		case XK_F27:
			key = QFK_HOME;
			break;
		case XK_F29:
			key = QFK_PAGEUP;
			break;
		case XK_F33:
			key = QFK_END;
			break;
		case XK_F35:
			key = QFK_PAGEDOWN;
			break;

		// Some high ASCII symbols, for azerty keymaps
		case XK_twosuperior:
			key = QFK_WORLD_18;
			break;
		case XK_eacute:
			key = QFK_WORLD_63;
			break;
		case XK_section:
			key = QFK_WORLD_7;
			break;
		case XK_egrave:
			key = QFK_WORLD_72;
			break;
		case XK_ccedilla:
			key = QFK_WORLD_71;
			break;
		case XK_agrave:
			key = QFK_WORLD_64;
			break;

		case XK_Kanji:
			key = QFK_KANJI;
			break;
		case XK_Muhenkan:
			key = QFK_MUHENKAN;
			break;
		case XK_Henkan:
			key = QFK_HENKAN;
			break;
		case XK_Romaji:
			key = QFK_ROMAJI;
			break;
		case XK_Hiragana:
			key = QFK_HIRAGANA;
			break;
		case XK_Katakana:
			key = QFK_KATAKANA;
			break;
		case XK_Hiragana_Katakana:
			key = QFK_HIRAGANA_KATAKANA;
			break;
		case XK_Zenkaku:
			key = QFK_ZENKAKU;
			break;
		case XK_Hankaku:
			key = QFK_HANKAKU;
			break;
		case XK_Zenkaku_Hankaku:
			key = QFK_ZENKAKU_HANKAKU;
			break;
		case XK_Touroku:
			key = QFK_TOUROKU;
			break;
		case XK_Massyo:
			key = QFK_MASSYO;
			break;
		case XK_Kana_Lock:
			key = QFK_KANA_LOCK;
			break;
		case XK_Kana_Shift:
			key = QFK_KANA_SHIFT;
			break;
		case XK_Eisu_Shift:
			key = QFK_EISU_SHIFT;
			break;
		case XK_Eisu_toggle:
			key = QFK_EISU_TOGGLE;
			break;
		case XK_Kanji_Bangou:
			key = QFK_KANJI_BANGOU;
			break;
		case XK_Zen_Koho:
			key = QFK_ZEN_KOHO;
			break;
		case XK_Mae_Koho:
			key = QFK_MAE_KOHO;
			break;
		case XF86XK_HomePage:
			key = QFK_HOMEPAGE;
			break;
		case XF86XK_Search:
			key = QFK_SEARCH;
			break;
		case XF86XK_Mail:
			key = QFK_MAIL;
			break;
		case XF86XK_Favorites:
			key = QFK_FAVORITES;
			break;
		case XF86XK_AudioMute:
			key = QFK_AUDIOMUTE;
			break;
		case XF86XK_AudioLowerVolume:
			key = QFK_AUDIOLOWERVOLUME;
			break;
		case XF86XK_AudioRaiseVolume:
			key = QFK_AUDIORAISEVOLUME;
			break;
		case XF86XK_AudioPlay:
			key = QFK_AUDIOPLAY;
			break;
		case XF86XK_Calculator:
			key = QFK_CALCULATOR;
			break;
		case XK_Help:
			key = QFK_HELP;
			break;
		case XK_Undo:
			key = QFK_UNDO;
			break;
		case XK_Redo:
			key = QFK_REDO;
			break;
		case XF86XK_New:
			key = QFK_NEW;
			break;
		case XF86XK_Reload:	// eh? it's open (hiraku) on my kb
			key = QFK_RELOAD;
			break;
		case SunXK_Open:
			//FALL THROUGH
		case XF86XK_Open:
			key = QFK_OPEN;
			break;
		case XF86XK_Close:
			key = QFK_CLOSE;
			break;
		case XF86XK_Reply:
			key = QFK_REPLY;
			break;
		case XF86XK_MailForward:
			key = QFK_MAILFORWARD;
			break;
		case XF86XK_Send:
			key = QFK_SEND;
			break;
		case XF86XK_Save:
			key = QFK_SAVE;
			break;
		case XK_KP_Equal:
			key = QFK_KP_EQUALS;
			break;
		case XK_parenleft:
			key = QFK_LEFTPAREN;
			break;
		case XK_parenright:
			key = QFK_RIGHTPAREN;
			break;
		case XF86XK_Back:
			key = QFK_BACK;
			break;
		case XF86XK_Forward:
			key = QFK_FORWARD;
			break;

		default:
			if (keysym < 128) {								// ASCII keys
				key = keysym;
				if ((key >= 'A') && (key <= 'Z')) {
					key = key + ('a' - 'A');
				}
			}
			break;
	}
	*k = key;
	*u = unicode;
}

static void
in_x11_send_focus_event (int gain)
{
	IE_event_t  event = {
		.type = gain ? ie_app_gain_focus : ie_app_lose_focus,
		.when = Sys_LongTime (),
	};
	IE_Send_Event (&event);
}

static void
center_pointer (void)
{
	XEvent      event = {};

	event.type = MotionNotify;
	event.xmotion.display = x_disp;
	event.xmotion.window = x_win;
	event.xmotion.x = viddef.width / 2;
	event.xmotion.y = viddef.height / 2;
	XSendEvent (x_disp, x_win, False, PointerMotionMask, &event);
	XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0,
				  viddef.width / 2, viddef.height / 2);
}

static void
in_x11_send_axis_event (int devid, in_axisinfo_t *axis)
{
	IE_event_t  event = {
		.type = ie_axis,
		.when = Sys_LongTime (),
		.axis = {
			.data = x11_mouse_device.event_data,
			.devid = devid,
			.axis = axis->axis,
			.value = axis->value,
		},
	};
	IE_Send_Event (&event);
}

static int
in_x11_send_mouse_event (IE_mouse_type type)
{
	IE_event_t  event = {
		.type = ie_mouse,
		.when = Sys_LongTime (),
		.mouse = x11_mouse,
	};
	event.mouse.type = type;
	return IE_Send_Event (&event);
}

static int
in_x11_send_key_event (void)
{
	IE_event_t  event = {
		.type = ie_key,
		.when = Sys_LongTime (),
		.key = x11_key,
	};
	return IE_Send_Event (&event);
}

static void
in_x11_send_button_event (int devid, in_buttoninfo_t *button, void *event_data)
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

static void
selection_notify (XEvent *event)
{
	unsigned char *data, *p;
	unsigned long num_bytes;
	unsigned long tmp, len;
	int         format;
	Atom        type, property;

	x_time = event->xselection.time;

	if ((property = event->xselection.property) == None)
		return;

	XGetWindowProperty (x_disp, x_win, property, 0, 0, False, AnyPropertyType,
						&type, &format, &len, &num_bytes, &data);
	if (num_bytes <= 0)
		return;
	if (XGetWindowProperty (x_disp, x_win, property, 0, num_bytes, True,
							AnyPropertyType, &type, &format, &len, &tmp, &data)
		!= Success) {
		XFree (data);	// FIXME is this correct for this instance?
		return;
	}

	//FIXME send paste event instead of key presses?
	x11_key.code = 0;
	for (p = data; num_bytes && *p; p++, num_bytes--) {
		x11_key.unicode = *p;
		in_x11_send_key_event ();
	}
	XFree (data);
}

static void
event_motion (XEvent *event)
{
	x_time = event->xmotion.time;
	if (x_time <= x_mouse_time) {
		x11_mouse.x = event->xmotion.x;
		x11_mouse.y = event->xmotion.y;
		return;
	}

	if (dga_active) {
		x11_mouse_axes[0].value = event->xmotion.x_root;
		x11_mouse_axes[1].value = event->xmotion.y_root;
	} else {
		if (input_grabbed) {
			if (!event->xmotion.send_event) {
				int         center_x = viddef.width / 2;
				int         center_y = viddef.height / 2;
				unsigned    dist_x = abs (center_x - event->xmotion.x);
				unsigned    dist_y = abs (center_y - event->xmotion.y);

				x11_mouse_axes[0].value = event->xmotion.x - x11_mouse.x;
				x11_mouse_axes[1].value = event->xmotion.y - x11_mouse.y;
				if (dist_x > viddef.width / 4 || dist_y > viddef.height / 4) {
					center_pointer ();
				}
			}
		} else {
			x11_mouse_axes[0].value = event->xmotion.x - x11_mouse.x;
			x11_mouse_axes[1].value = event->xmotion.y - x11_mouse.y;
		}
	}

	x11_mouse.shift = event->xmotion.state & 0xff;
	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
	if (!in_x11_send_mouse_event (ie_mousemove)) {
		in_x11_send_axis_event (x11_mouse_device.devid, &x11_mouse_axes[0]);
		in_x11_send_axis_event (x11_mouse_device.devid, &x11_mouse_axes[1]);
	}
}

static void
event_button (XEvent *event)
{
	unsigned    but;

	x_time = event->xbutton.time;

	// x11 buttons are 1-based
	but = event->xbutton.button - 1;
	if (but > X11_MOUSE_BUTTONS) {
		return;
	}

	int press = event->type == ButtonPress;

	x11_mouse_buttons[but].state = press;

	x11_mouse.shift = event->xmotion.state & 0xff;
	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
	if (press) {
		x11_mouse.buttons |= 1 << but;
	} else {
		x11_mouse.buttons &= ~(1 << but);
	}
	if (!in_x11_send_mouse_event (press ? ie_mousedown : ie_mouseup)) {
		in_x11_send_button_event (x11_mouse_device.devid,
								  &x11_mouse_buttons[but],
								  x11_mouse_device.event_data);
	}
}

static unsigned
in_x11_process_key_button (unsigned keycode, int press)
{
	// X11 protocol supports only 256 keys. The key codes are the AT scan codes
	// offset by 8 (so Esc is 9 instead of 1).
	unsigned    key = (keycode - 8) & 0xff;
	x11_key_buttons[key].state = press;
	return key;
}

static void
event_key (XEvent *event)
{
	unsigned    key = 0;

	x_time = event->xkey.time;
	if (!x11_have_xi) {
		key = in_x11_process_key_button (event->xkey.keycode,
									     event->type == KeyPress);
	}
	x11_key.shift = event->xmotion.state & 0xff;
	XLateKey (&event->xkey, &x11_key.code, &x11_key.unicode);
	if (!(event->type == KeyPress && in_x11_send_key_event ())
		&& !x11_have_xi) {
		in_x11_send_button_event (x11_keyboard_device.devid,
								  &x11_key_buttons[key],
								  x11_keyboard_device.event_data);
	}
}

#ifdef HAVE_XI2
static void
xi_raw_key (void *event, int press)
{
	if (!x_have_focus) {
		//avoid being a keylogger
		return;
	}
	XIRawEvent *re = event;
	unsigned    key = in_x11_process_key_button (re->detail, press);

	// Send only the button press: event_key takes care of the UI key event,
	// which includes character translation and repeat (XInput2 raw key events
	// do not repeat)
	in_x11_send_button_event (x11_keyboard_device.devid,
							  &x11_key_buttons[key],
							  x11_keyboard_device.event_data);
}

static void
xi_raw_key_press (void *event)
{
	xi_raw_key (event, 1);
}

static void
xi_raw_key_resease (void *event)
{
	xi_raw_key (event, 0);
}

static void
xi_raw_motion (void *event)
{
	int         root_x, root_y;
	Window      root_w, child_w;
	XIRawEvent *re = event;

	//FIXME it turns out mice can have multiple axes (my trackball has 4:
	//usual x and y, +scroll up/down and left/right (clicky and icky, but
	//still...)
	unsigned    mask = re->valuators.mask[0] & 3;
	if (!mask) {
		return;
	}

	x11_mouse_axes[0].value = 0;
	x11_mouse_axes[1].value = 0;
	for (int axis = 0, ind = 0; mask; axis++, mask >>= 1) {
		if (mask & 1) {
			x11_mouse_axes[axis].value = re->raw_values[ind++];
		}
	}
	XQueryPointer (x_disp, x_win, &root_w, &child_w, &root_x, &root_y,
				   &x11_mouse.x, &x11_mouse.y, &x11_mouse.shift);
	// want only the modifier state
	x11_mouse.shift &= 0xff;
	if (!in_x11_send_mouse_event (ie_mousemove)) {
		in_x11_send_axis_event (x11_mouse_device.devid,
								&x11_mouse_axes[0]);
		in_x11_send_axis_event (x11_mouse_device.devid,
								&x11_mouse_axes[1]);
	}
}

static void
xi_raw_button (void *event, int press)
{
	int         root_x, root_y;
	Window      root_w, child_w;
	unsigned    button;
	XIRawEvent *re = event;

	// x11 buttons are 1-based
	button = re->detail - 1;
	if (button > X11_MOUSE_BUTTONS) {
		return;
	}
	XQueryPointer (x_disp, x_win, &root_w, &child_w, &root_x, &root_y,
				   &x11_mouse.x, &x11_mouse.y, &x11_mouse.shift);
	// want only the modifier state
	x11_mouse.shift &= 0xff;
	if (press) {
		x11_mouse.buttons |= 1 << button;
	} else {
		x11_mouse.buttons &= ~(1 << button);
	}
	x11_mouse_buttons[button].state = press;
	if (!in_x11_send_mouse_event (press ? ie_mousedown : ie_mouseup)) {
		in_x11_send_button_event (x11_mouse_device.devid,
								  &x11_mouse_buttons[button],
								  x11_mouse_device.event_data);
	}
}

static void
xi_raw_button_press (void *event)
{
	xi_raw_button (event, 1);
}

static void
xi_raw_button_resease (void *event)
{
	xi_raw_button (event, 0);
}

#ifdef HAVE_XFIXES
static void
xi_barrier_hit (void *event)
{
	__auto_type be = *(XIBarrierEvent *) event;

	if (be.barrier != x11_left_barrier
		&& be.barrier != x11_right_barrier
		&& be.barrier != x11_top_barrier
		&& be.barrier != x11_bottom_barrier) {
		Sys_MaskPrintf (SYS_vid, "xi_barrier_hit: bad barrier %ld\n",
						be.barrier);
		return;
	}

	center_pointer ();
}

static void
xi_barrier_leave (void *event)
{
}
#endif

static void
event_generic (XEvent *event)
{
	// XI_LASTEVENT is the actual last event, not +1
	static void (*xi_event_handlers[XI_LASTEVENT + 1]) (void *) = {
		[XI_RawKeyPress] = xi_raw_key_press,
		[XI_RawKeyRelease] = xi_raw_key_resease,
		[XI_RawMotion] = xi_raw_motion,
		[XI_RawButtonPress] = xi_raw_button_press,
		[XI_RawButtonRelease] = xi_raw_button_resease,
#ifdef HAVE_XFIXES
		[XI_BarrierHit] = xi_barrier_hit,
		[XI_BarrierLeave] = xi_barrier_leave,
#endif
	};
	XGenericEventCookie *cookie = &event->xcookie;

	if (cookie->type != GenericEvent || cookie->extension != xi_opcode
		|| !XGetEventData (x_disp, cookie)) {
		return;
	}

	if ((unsigned) cookie->evtype <= XI_LASTEVENT
		&& xi_event_handlers[cookie->evtype]) {
		xi_event_handlers[cookie->evtype] (cookie->data);
	}

	XFreeEventData (x_disp, cookie);
}
#endif

static void
grab_error (int code, const char *device)
{
	const char *reason;

	switch (code) {
		case AlreadyGrabbed:
			reason = "already grabbed";
			break;
		case GrabNotViewable:
			reason = "grab not viewable";
			break;
		case GrabFrozen:
			reason = "grab frozen";
			break;
		case GrabInvalidTime:
			reason = "grab invalid time";
			break;
		default:
			reason = "unknown reason";
			break;
	}
	Sys_Printf ("failed to grab %s: %s\n", device, reason);
}

#ifdef HAVE_XFIXES
static void
in_x11_remove_barriers (void)
{
	if (x11_left_barrier > 0) {
		XFixesDestroyPointerBarrier (x_disp, x11_left_barrier);
		x11_left_barrier = 0;
	}
	if (x11_right_barrier > 0) {
		XFixesDestroyPointerBarrier (x_disp, x11_right_barrier);
		x11_right_barrier = 0;
	}
	if (x11_top_barrier > 0) {
		XFixesDestroyPointerBarrier (x_disp, x11_top_barrier);
		x11_top_barrier = 0;
	}
	if (x11_bottom_barrier > 0) {
		XFixesDestroyPointerBarrier (x_disp, x11_bottom_barrier);
		x11_bottom_barrier = 0;
	}
}

static void
in_x11_setup_barriers (int xpos, int ypos, int xlen, int ylen)
{
	in_x11_remove_barriers ();

	int         lx = bound (0, xpos, x_width - 1);
	int         ty = bound (0, ypos, x_height - 1);
	int         rx = bound (0, xpos + xlen - 1, x_width - 1);
	int         by = bound (0, ypos + ylen - 1, x_height - 1);
	x11_left_barrier = XFixesCreatePointerBarrier (x_disp, x_root,
												   lx, ty-1, lx, by+1,
												   BarrierPositiveX, 0, 0);
	x11_right_barrier = XFixesCreatePointerBarrier (x_disp, x_root,
													rx, ty-1, rx, by+1,
												    BarrierNegativeX, 0, 0);
	x11_top_barrier = XFixesCreatePointerBarrier (x_disp, x_root,
												  lx-1, ty, rx+1, ty,
												  BarrierPositiveY, 0, 0);
	x11_bottom_barrier = XFixesCreatePointerBarrier (x_disp, x_root,
												     lx-1, by, rx+1, by,
												     BarrierNegativeY, 0, 0);
}
#endif

static void
event_focusout (XEvent *event)
{
	if (x_have_focus) {
		x_have_focus = false;
#ifdef HAVE_XFIXES
		in_x11_remove_barriers ();
#endif
		in_x11_send_focus_event (0);
		if (in_snd_block) {
			S_BlockSound ();
			CDAudio_Pause ();
		}
		X11_RestoreGamma ();
	}
}

static void
event_focusin (XEvent *event)
{
	in_x11_send_focus_event (1);
	x_have_focus = true;
	if (in_snd_block) {
		S_UnblockSound ();
		CDAudio_Resume ();
	}
	if (input_grabbed) {
#ifdef HAVE_XFIXES
		in_x11_setup_barriers (x11_aw.xpos, x11_aw.ypos,
							   x11_aw.xlen, x11_aw.ylen);
#endif
	}
	VID_UpdateGamma ();
}

static void
in_x11_grab_input (void *data, int grab)
{
	if (!x_disp || !x_win)
		return;

#ifdef HAVE_XFIXES
	if (xf_opcode) {
		input_grabbed = grab;
		if (input_grabbed) {
			in_x11_setup_barriers (x11_aw.xpos, x11_aw.ypos,
								   x11_aw.xlen, x11_aw.ylen);
		} else {
			in_x11_remove_barriers ();
		}
		return;
	}
#endif

	if ((input_grabbed && grab) || (!input_grabbed && !grab))
		return;

	if (grab) {
		int         ret;

		ret = XGrabPointer (x_disp, x_win, True, X11_MOUSE_MASK, GrabModeAsync,
							GrabModeAsync, x_win, None, CurrentTime);
		if (ret != GrabSuccess) {
			grab_error (ret, "mouse");
			return;
		}
		ret = XGrabKeyboard (x_disp, x_win, 1, GrabModeAsync, GrabModeAsync,
							 CurrentTime);
		if (ret != GrabSuccess) {
			XUngrabPointer (x_disp, CurrentTime);
			grab_error (ret, "keyboard");
			return;
		}
		input_grabbed = 1;
		in_dga_f (0, &in_dga_cvar);
	} else {
		XUngrabPointer (x_disp, CurrentTime);
		XUngrabKeyboard (x_disp, CurrentTime);
		input_grabbed = 0;
		in_dga_f (0, &in_dga_cvar);
	}
}
#ifdef X11_USE_SELECT
static void
in_x11_add_select (qf_fd_set *fdset, int *maxfd, void *data)
{
	QF_FD_SET (x11_fd, fdset);
	if (*maxfd < x11_fd) {
		*maxfd = x11_fd;
	}
}

static void
in_x11_check_select (qf_fd_set *fdset, void *data)
{
	if (QF_FD_ISSET (x11_fd, fdset)) {
		Sys_Printf ("in_x11_check_select\n");
		X11_ProcessEvents ();	// Get events from X server.
	}
}
#else
static void
in_x11_process_events (void *data)
{
	X11_ProcessEvents ();	// Get events from X server.
}
#endif
static void
in_x11_axis_info (void *data, void *device, in_axisinfo_t *axes, int *numaxes)
{
	x11_device_t *dev = device;
	if (!axes) {
		*numaxes = dev->num_axes;
		return;
	}
	if (*numaxes > dev->num_axes) {
		*numaxes = dev->num_axes;
	}
	if (dev->num_axes) {
		memcpy (axes, dev->axes, *numaxes * sizeof (in_axisinfo_t));
	}
}

static void
in_x11_button_info (void *data, void *device, in_buttoninfo_t *buttons,
					int *numbuttons)
{
	x11_device_t *dev = device;
	if (!buttons) {
		*numbuttons = dev->num_buttons;
		return;
	}
	if (*numbuttons > dev->num_buttons) {
		*numbuttons = dev->num_buttons;
	}
	if (dev->num_buttons) {
		memcpy (buttons, dev->buttons, *numbuttons * sizeof (in_buttoninfo_t));
	}
}

static const char *
in_x11_get_axis_name (void *data, void *device, int axis_num)
{
	x11_device_t *dev = device;
	const char *name = 0;

	if (dev == &x11_keyboard_device) {
		// keyboards don't have axes...
	} else if (dev == &x11_mouse_device) {
		if ((unsigned) axis_num < SIZE (x11_mouse_axis_names)) {
			name = x11_mouse_axis_names[axis_num];
		}
	}
	return name;
}

static const char *
in_x11_get_button_name (void *data, void *device, int button_num)
{
	x11_device_t *dev = device;
	const char *name = 0;

	if (dev == &x11_keyboard_device) {
		// X11 protocol supports only 256 keys. The key codes are the AT scan
		// codes offset by 8 (so Esc is 9 instead of 1).
		KeyCode     keycode = (button_num + 8) & 0xff;
		KeySym      keysym = XkbKeycodeToKeysym (x_disp, keycode, 0, 0);
		if (keysym != NoSymbol) {
			name = XKeysymToString (keysym);
		}
	} else if (dev == &x11_mouse_device) {
		if ((unsigned) button_num < SIZE (x11_mouse_button_names)) {
			name = x11_mouse_button_names[button_num];
		}
	}
	return name;
}

static int
in_x11_get_axis_info (void *data, void *device, int axis_num,
					  in_axisinfo_t *info)
{
	x11_device_t *dev = device;
	if (axis_num < 0 || axis_num > dev->num_axes) {
		return 0;
	}
	*info = dev->axes[axis_num];
	return 1;
}

static int
in_x11_get_button_info (void *data, void *device, int button_num,
						in_buttoninfo_t *info)
{
	x11_device_t *dev = device;
	if (button_num < 0 || button_num > dev->num_buttons) {
		return 0;
	}
	*info = dev->buttons[button_num];
	return 1;
}

static int
in_x11_get_axis_num (void *data, void *device, const char *axis_name)
{
	x11_device_t *dev = device;
	int         num = -1;

	if (dev == &x11_keyboard_device) {
		// keyboards don't have axes...
	} else if (dev == &x11_mouse_device) {
		for (size_t i = 0; i < SIZE (x11_mouse_axis_names); i++) {
			if (strcasecmp (axis_name, x11_mouse_axis_names[i]) == 0) {
				num = i;
				break;
			}
		}
	}
	return num;
}

static int
in_x11_get_button_num (void *data, void *device, const char *button_name)
{
	x11_device_t *dev = device;
	int         num = -1;

	if (dev == &x11_keyboard_device) {
		KeySym      keysym = XStringToKeysym (button_name);
		if (keysym != NoSymbol) {
			KeyCode     keycode = XKeysymToKeycode (x_disp, keysym);
			if (keycode != 0) {
				// X11 protocol supports only 256 keys. The key codes are the
				// AT scan codes offset by 8 (so Esc is 9 instead of 1).
				num = (keycode - 8) & 0xff;
			}
		}
	} else if (dev == &x11_mouse_device) {
		for (size_t i = 0; i < SIZE (x11_mouse_button_names); i++) {
			if (strcasecmp (button_name, x11_mouse_button_names[i]) == 0) {
				num = i;
				break;
			}
		}
	}
	return num;
}

static void
in_x11_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_vid, "in_x11_shutdown\n");
	if (x_disp) {
//		XAutoRepeatOn (x_disp);
		dga_off ();
	}
	if (!in_mouse_accel)
		X11_RestoreMouseAcceleration ();
}

static void
in_x11_set_device_event_data (void *device, void *event_data, void *data)
{
	x11_device_t *dev = device;
	dev->event_data = event_data;
}

static void *
in_x11_get_device_event_data (void *device, void *data)
{
	x11_device_t *dev = device;
	return dev->event_data;
}

static void
in_x11_init_cvars (void *data)
{
	Cvar_Register (&in_auto_focus_cvar, 0, 0);
	Cvar_Register (&in_snd_block_cvar, 0, 0);
	Cvar_Register (&in_dga_cvar, in_dga_f, 0);
	Cvar_Register (&in_mouse_accel_cvar, in_mouse_accel_f, 0);
}

static void
x11_add_device (x11_device_t *dev)
{
	for (int i = 0; i < dev->num_axes; i++) {
		dev->axes[i].axis = i;
	}
	for (int i = 0; i < dev->num_buttons; i++) {
		dev->buttons[i].button = i;
	}
	dev->devid = IN_AddDevice (x11_driver_handle, dev, dev->name, dev->name);
}

#ifdef HAVE_XI2
static int
in_x11_check_xi2 (void)
{
	// Support XI 2.2
	int         major = 2, minor = 2;
	int         event, error;
	int         ret;

	if (!XQueryExtension (x_disp, "XInputExtension",
						  &xi_opcode, &event, &error)) {
		Sys_MaskPrintf (SYS_vid, "X Input extenions not available.\n");
		return 0;
	}
	if ((ret = XIQueryVersion (x_disp, &major, &minor)) == BadRequest) {
		Sys_MaskPrintf (SYS_vid,
						"No XI2 support. Server supports only %d.%d\n",
						major, minor);
		return 0;
	} else if (ret != Success) {
		Sys_MaskPrintf (SYS_vid, "in_x11_check_xi2: Xlib return bad: %d\n",
						ret);
	}
	Sys_MaskPrintf (SYS_vid, "XI2 supported: version %d.%d, op: %d err: %d\n",
					major, minor, xi_opcode, error);

#ifdef HAVE_XFIXES
	if (!XQueryExtension (x_disp, "XFIXES", &xf_opcode, &event, &error)) {
		Sys_MaskPrintf (SYS_vid, "X fixes extenions not available.\n");
		return 0;
	}
	major = 5, minor = 0;
	if (!XFixesQueryVersion (x_disp, &major, &minor)
		|| (major * 10 + minor) < 50) {
		Sys_MaskPrintf (SYS_vid, "Need X fixes 5.0.\n");
		return 0;
	}
	Sys_MaskPrintf (SYS_vid,
					"XFixes supported: version %d.%d, op: %d err: %d\n",
					major, minor, xf_opcode, error);
#endif
	return 1;
}

static void
in_x11_xi_select_events (void)
{
	byte        mask[(XI_LASTEVENT + 7) / 8] = {};
	XIEventMask evmask = {
		.deviceid = XIAllMasterDevices,
		.mask_len = sizeof (mask),
		.mask = mask,
	};
	XISetMask (mask, XI_BarrierHit);
	XISetMask (mask, XI_BarrierLeave);
	XISetMask (mask, XI_RawKeyPress);
	XISetMask (mask, XI_RawKeyRelease);
	XISelectEvents (x_disp, x_root, &evmask, 1);
}

static void
in_x11_xi_setup_grabs (void)
{
	// FIXME normal apps aren't supposed to care about the client pointer,
	// but grabbing all master devices grabs the keyboard, too, which blocks
	// alt-tab and the like
	int dev;
	XIGetClientPointer (x_disp, x_win, &dev);

	byte        mask[(XI_LASTEVENT + 7) / 8] = {};
	XIEventMask evmask = {
		.deviceid = dev,
		.mask_len = sizeof (mask),
		.mask = mask,
	};
	XISetMask (mask, XI_RawMotion);
	XISetMask (mask, XI_RawButtonPress);
	XISetMask (mask, XI_RawButtonRelease);

	XIGrabModifiers modif = {
		.modifiers = XIAnyModifier,
	};

	// Grab mouse events on the app window so window manager actions (eg,
	// alt-middle-click) don't interfere. However, the keyboard is not grabbed
	// so the user always has a way to take control (assuming the window
	// manager provides such, but most do).
	XIGrabEnter (x_disp, dev, x_win, None, XIGrabModeAsync, XIGrabModeAsync,
				 0, &evmask, 1, &modif);
}
#endif

static void
x11_app_window (const IE_event_t *ie_event)
{
	x11_aw = ie_event->app_window;
	Sys_MaskPrintf (SYS_vid, "window: %d %d %d %d\n",
					x11_aw.xpos, x11_aw.ypos, x11_aw.xlen, x11_aw.ylen);
#ifdef HAVE_XFIXES
	if (input_grabbed) {
		in_x11_setup_barriers (x11_aw.xpos, x11_aw.ypos,
							   x11_aw.xlen, x11_aw.ylen);
	}
#endif
}

static int
x11_event_handler (const IE_event_t *ie_event, void *unused)
{
	static void (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_app_window] = x11_app_window,
	};
	if ((unsigned) ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return 0;
	}
	handlers[ie_event->type] (ie_event);
	return 1;
}

long
IN_X11_Preinit (void)
{
	if (!x_disp)
		Sys_Error ("IN: No display!!");

	long        event_mask = X11_INPUT_MASK;

	x11_event_handler_id = IE_Add_Handler (x11_event_handler, 0);
#ifdef HAVE_XI2
	x11_have_xi = in_x11_check_xi2 ();
#endif
	X11_AddEvent (KeyPress, &event_key);
	X11_AddEvent (KeyRelease, &event_key);
	X11_AddEvent (FocusIn, &event_focusin);
	X11_AddEvent (FocusOut, &event_focusout);
	X11_AddEvent (SelectionNotify, &selection_notify);
	X11_AddEvent (EnterNotify, &enter_notify);
	X11_AddEvent (LeaveNotify, &leave_notify);

	if (x11_have_xi) {
#ifdef HAVE_XI2
		X11_AddEvent (GenericEvent, &event_generic);
#endif
	} else {
		X11_AddEvent (MotionNotify, &event_motion);
		X11_AddEvent (ButtonPress, &event_button);
		X11_AddEvent (ButtonRelease, &event_button);
	}

	Cmd_AddCommand ("in_paste_buffer", in_paste_buffer_f,
					"Paste the contents of X's C&P buffer to the console");
	return event_mask;
}

void
IN_X11_Postinit (void)
{
	if (!x_disp)
		Sys_Error ("IN: No display!!");
	if (!x_win)
		Sys_Error ("IN: No window!!");

	if (x11_have_xi) {
#ifdef HAVE_XI2
		in_x11_xi_select_events ();
		in_x11_xi_setup_grabs ();
#endif
	} else {
		dga_avail = VID_CheckDGA (x_disp, NULL, NULL, NULL);
		Sys_MaskPrintf (SYS_vid, "VID_CheckDGA returned %d\n",
						dga_avail);
	}
}

static void
in_x11_init (void *data)
{
	x11_fd = ConnectionNumber (x_disp);

	x11_add_device (&x11_keyboard_device);
	x11_add_device (&x11_mouse_device);
}

static void
in_x11_clear_states (void *data)
{
}

static in_driver_t in_x11_driver = {
	.init_cvars = in_x11_init_cvars,
	.init = in_x11_init,
	.shutdown = in_x11_shutdown,
	.set_device_event_data = in_x11_set_device_event_data,
	.get_device_event_data = in_x11_get_device_event_data,
#ifdef X11_USE_SELECT
	.add_select = in_x11_add_select,
	.check_select = in_x11_check_select,
#else
	.process_events = in_x11_process_events,
#endif
	.clear_states = in_x11_clear_states,
	.grab_input = in_x11_grab_input,

	.axis_info = in_x11_axis_info,
	.button_info = in_x11_button_info,

	.get_axis_name = in_x11_get_axis_name,
	.get_button_name = in_x11_get_button_name,

	.get_axis_num = in_x11_get_axis_num,
	.get_button_num = in_x11_get_button_num,

	.get_axis_info = in_x11_get_axis_info,
	.get_button_info = in_x11_get_button_info,
};

static void __attribute__((constructor))
in_x11_register_driver (void)
{
	x11_driver_handle = IN_RegisterDriver (&in_x11_driver, 0);
}

int x11_force_link;
