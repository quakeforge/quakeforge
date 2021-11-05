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
#include <X11/Sunkeysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_DGA
# include <X11/extensions/XShm.h>
# ifdef DGA_OLD_HEADERS
#  include <X11/extensions/xf86dga.h>
# else
#  include <X11/extensions/Xxf86dga.h>
#endif
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
#include "qfselect.h"
#include "vid_internal.h"

typedef struct x11_device_s {
	const char *name;
	int         num_axes;
	int         num_buttons;
	in_axisinfo_t *axes;
	in_buttoninfo_t *buttons;
	int         devid;
} x11_device_t;

// The X11 protocol supports only 256 keys
static in_buttoninfo_t x11_key_buttons[256];
// X11 mice have only two axes (FIXME always true?)
static in_axisinfo_t x11_mouse_axes[2];
// FIXME assume up to 32 mouse buttons
static in_buttoninfo_t x11_mouse_buttons[32];

#define infosize(x) (sizeof (x) / sizeof (x[0]))

static x11_device_t x11_keyboard_device = {
	"core:keyboard",
	0, infosize (x11_key_buttons),
	0, x11_key_buttons,
};

static x11_device_t x11_mouse_device = {
	"core:mouse",
	infosize (x11_mouse_axes), infosize (x11_mouse_buttons),
	x11_mouse_axes, x11_mouse_buttons,
};

cvar_t	   *in_snd_block;
cvar_t	   *in_dga;
cvar_t	   *in_mouse_accel;

static qboolean dga_avail;
static qboolean dga_active;
static IE_mouse_event_t x11_mouse;
static IE_key_event_t x11_key;
static int input_grabbed = 0;

static int x11_fd;
static int x11_driver_handle = -1;

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
in_dga_f (cvar_t *var)
{
	if (var->int_val && input_grabbed) {
		Sys_MaskPrintf (SYS_vid, "VID: in_dga_f on\n");
		dga_on ();
	} else {
		Sys_MaskPrintf (SYS_vid, "VID: in_dga_f off\n");
		dga_off ();
	}
}

static void
in_mouse_accel_f (cvar_t *var)
{
	if (var->int_val) {
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

	// get bytes to keys.c
	for (p = data; num_bytes && *p; p++, num_bytes--) {
		Key_Event (QFK_UNKNOWN, *p, 1);
		Key_Event (QFK_UNKNOWN, 0, 0);
	}
	XFree (data);
}

static void
enter_notify (XEvent *event)
{
	x_time = event->xcrossing.time;

	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
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
in_x11_keydest_callback (keydest_t key_dest, void *data)
{
//	if (key_dest == key_game) {
//		XAutoRepeatOff (x_disp);
//	} else {
//		XAutoRepeatOn (x_disp);
//	}
}

static void
event_focusout (XEvent *event)
{
	Key_FocusEvent (0);
	if (x_have_focus) {
		x_have_focus = false;
		if (in_snd_block->int_val) {
			S_BlockSound ();
			CDAudio_Pause ();
		}
		X11_RestoreGamma ();
	}
}

static void
event_focusin (XEvent *event)
{
	x_have_focus = true;
	Key_FocusEvent (1);
	if (in_snd_block->int_val) {
		S_UnblockSound ();
		CDAudio_Resume ();
	}
	VID_UpdateGamma (vid_gamma);
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
			.devid = devid,
			.axis = axis->axis,
			.value = axis->value,
		},
	};
	IE_Send_Event (&event);
}

static void
in_x11_send_mouse_event (IE_mouse_type type)
{
	IE_event_t  event = {
		.type = ie_mouse,
		.when = Sys_LongTime (),
		.mouse = x11_mouse,
	};
	event.mouse.type = type;
	IE_Send_Event (&event);
}

static void
in_x11_send_key_event (int press)
{
	IE_event_t  event = {
		.type = ie_key,
		.when = Sys_LongTime (),
		.key = x11_key,
	};
	IE_Send_Event (&event);
}

static void
in_x11_send_button_event (int devid, in_buttoninfo_t *button)
{
	IE_event_t  event = {
		.type = ie_button,
		.when = Sys_LongTime (),
		.button = {
			.devid = devid,
			.button = button->button,
			.state = button->state,
		},
	};
	IE_Send_Event (&event);
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
		if (vid_fullscreen->int_val || input_grabbed) {
			if (!event->xmotion.send_event) {
				unsigned    dist_x = abs (viddef.width / 2 - event->xmotion.x);
				unsigned    dist_y = abs (viddef.height / 2 - event->xmotion.y);

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
	in_x11_send_axis_event (x11_mouse_device.devid, &x11_mouse_axes[0]);
	in_x11_send_axis_event (x11_mouse_device.devid, &x11_mouse_axes[1]);

	x11_mouse.shift = event->xmotion.state & 0xff;
	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
	in_x11_send_mouse_event (ie_mousemove);
}

static void
event_button (XEvent *event)
{
	unsigned    but;

	x_time = event->xbutton.time;

	but = event->xbutton.button - 1;
	if (but >= 32) {
		return;
	}

	int press = event->type == ButtonPress;

	x11_mouse_buttons[but].state = press;
	in_x11_send_button_event (x11_mouse_device.devid, &x11_mouse_buttons[but]);

	x11_mouse.shift = event->xmotion.state & 0xff;
	x11_mouse.x = event->xmotion.x;
	x11_mouse.y = event->xmotion.y;
	if (press) {
		x11_mouse.buttons |= 1 << but;
	} else {
		x11_mouse.buttons &= ~(1 << but);
	}
	in_x11_send_mouse_event (press ? ie_mousedown : ie_mouseup);
}

static void
event_key (XEvent *event)
{
	int key;

	x_time = event->xkey.time;
	// X11 protocol supports only 256 keys. The key codes are the AT scan codes
	// offset by 8 (so Esc is 9 instead of 1).
	key = (event->xkey.keycode - 8) & 0xff;
	x11_key_buttons[key].state = event->type == KeyPress;
	in_x11_send_button_event (x11_keyboard_device.devid,
							  &x11_key_buttons[key]);

	x11_key.shift = event->xmotion.state & 0xff;
	XLateKey (&event->xkey, &x11_key.code, &x11_key.unicode);
	in_x11_send_key_event (event->type == KeyPress);
}

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

static void
in_x11_grab_input (void *data, int grab)
{
	if (!x_disp || !x_win)
		return;

	if (vid_fullscreen)
		grab = grab || vid_fullscreen->int_val;

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
		in_dga_f (in_dga);
	} else {
		XUngrabPointer (x_disp, CurrentTime);
		XUngrabKeyboard (x_disp, CurrentTime);
		input_grabbed = 0;
		in_dga_f (in_dga);
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
	memcpy (axes, dev->axes, *numaxes * sizeof (in_axisinfo_t));
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
	memcpy (buttons, dev->buttons, *numbuttons * sizeof (in_buttoninfo_t));
}

static void
in_x11_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_vid, "in_x11_shutdown\n");
	in_mouse_avail = 0;
	if (x_disp) {
//		XAutoRepeatOn (x_disp);
		dga_off ();
	}
	if (in_mouse_accel && !in_mouse_accel->int_val)
		X11_RestoreMouseAcceleration ();
	X11_CloseDisplay ();
}

static void
in_x11_init_cvars (void)
{
	in_snd_block = Cvar_Get ("in_snd_block", "0", CVAR_ARCHIVE, NULL,
							 "block sound output on window focus loss");
	in_dga = Cvar_Get ("in_dga", "0", CVAR_ARCHIVE, in_dga_f, //FIXME 0 until X fixed
					   "DGA Input support");
	in_mouse_accel = Cvar_Get ("in_mouse_accel", "1", CVAR_ARCHIVE,
							   in_mouse_accel_f,
							   "set to 0 to remove mouse acceleration");
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

static void
in_x11_init (void *data)
{
	// open the display
	if (!x_disp)
		Sys_Error ("IN: No display!!");
	if (!x_win)
		Sys_Error ("IN: No window!!");

	in_x11_init_cvars ();

	X11_OpenDisplay (); // call to increment the reference counter

	x11_fd = ConnectionNumber (x_disp);

	{
		int 	attribmask = CWEventMask;

		XWindowAttributes attribs_1;
		XSetWindowAttributes attribs_2;

		XGetWindowAttributes (x_disp, x_win, &attribs_1);

		attribs_2.event_mask = attribs_1.your_event_mask | X11_INPUT_MASK;

		XChangeWindowAttributes (x_disp, x_win, attribmask, &attribs_2);
	}

	X11_AddEvent (KeyPress, &event_key);
	X11_AddEvent (KeyRelease, &event_key);
	X11_AddEvent (FocusIn, &event_focusin);
	X11_AddEvent (FocusOut, &event_focusout);
	X11_AddEvent (SelectionNotify, &selection_notify);
	X11_AddEvent (EnterNotify, &enter_notify);

	x11_add_device (&x11_keyboard_device);

	if (!COM_CheckParm ("-nomouse")) {
		dga_avail = VID_CheckDGA (x_disp, NULL, NULL, NULL);
		Sys_MaskPrintf (SYS_vid, "VID_CheckDGA returned %d\n", dga_avail);

		X11_AddEvent (ButtonPress, &event_button);
		X11_AddEvent (ButtonRelease, &event_button);
		X11_AddEvent (MotionNotify, &event_motion);

		x11_add_device (&x11_mouse_device);

		in_mouse_avail = 1;
	}

	Key_KeydestCallback (in_x11_keydest_callback, 0);

	Cmd_AddCommand ("in_paste_buffer", in_paste_buffer_f,
					"Paste the contents of X's C&P buffer to the console");
}

static void
in_x11_clear_states (void *data)
{
}

static in_driver_t in_x11_driver = {
	.init = in_x11_init,
	.shutdown = in_x11_shutdown,
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
};

static void __attribute__((constructor))
in_x11_register_driver (void)
{
	x11_driver_handle = IN_RegisterDriver (&in_x11_driver, 0);
}

int x11_force_link;
