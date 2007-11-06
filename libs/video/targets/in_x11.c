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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_DGA
# include <X11/extensions/XShm.h>
# include <X11/extensions/xf86dga.h>
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
#include "QF/vid.h"

#include "compat.h"
#include "context_x11.h"
#include "dga_check.h"

cvar_t	   *in_snd_block;
cvar_t	   *in_dga;
cvar_t	   *in_mouse_accel;

static qboolean dga_avail;
static qboolean dga_active;
static keydest_t old_key_dest = key_none;
static int  p_mouse_x, p_mouse_y;

static void
dga_on (void)
{
#ifdef HAVE_DGA
	if (dga_avail && !dga_active) {
		XF86DGADirectVideo (x_disp, DefaultScreen (x_disp),
							XF86DGADirectMouse);
		dga_active = true;
	}
#endif
}

static void
dga_off (void)
{
#ifdef HAVE_DGA
	if (dga_avail && dga_active) {
		XF86DGADirectVideo (x_disp, DefaultScreen (x_disp), 0);
		dga_active = false;
	}
#endif
}

static void
in_dga_f (cvar_t *var)
{
	if (in_grab && in_grab->int_val) {
		if (var->int_val) {
			dga_on ();
		} else {
			dga_off ();
		}
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

	p_mouse_x = event->xmotion.x;
	p_mouse_y = event->xmotion.y;
}

static void
XLateKey (XKeyEvent * ev, int *k, int *u)
{
	char        buffer[4];
	int         bytes, unicode;
	int         key = 0;
	KeySym      keysym, shifted_keysym;
	XComposeStatus compose;

	keysym = XLookupKeysym (ev, 0);
	bytes = XLookupString (ev, buffer, sizeof(buffer), &shifted_keysym,
						   &compose);
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
event_key (XEvent *event)
{
	int key, unicode;

	x_time = event->xkey.time;
	if (old_key_dest != key_dest) {
		old_key_dest = key_dest;
		if (key_dest == key_game) {
			XAutoRepeatOff (x_disp);
		} else {
			XAutoRepeatOn (x_disp);
		}
	}
	XLateKey (&event->xkey, &key, &unicode);
	Key_Event (key, unicode, event->type == KeyPress);
}

static void
event_button (XEvent *event)
{
	int         but;

	x_time = event->xbutton.time;

	but = event->xbutton.button;
	if (but == 2)
		but = 3;
	else if (but == 3)
		but = 2;
	switch (but) {
		case 1:
		case 2:
		case 3:
		case 6:
		case 7:
			Key_Event (QFM_BUTTON1 + but - 1, 0, event->type == ButtonPress);
			break;
		case 4:
			Key_Event (QFM_WHEEL_UP, 0, event->type == ButtonPress);
			break;
		case 5:
			Key_Event (QFM_WHEEL_DOWN, 0, event->type == ButtonPress);
			break;
	}
}

static void
event_focusout (XEvent *event)
{
	if (x_have_focus) {
		x_have_focus = false;
		XAutoRepeatOn (x_disp);
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
	if (key_dest == key_game)
		XAutoRepeatOff (x_disp);
	if (in_snd_block->int_val) {
		S_UnblockSound ();
		CDAudio_Resume ();
	}
	VID_UpdateGamma (vid_gamma);
}

static void
center_pointer (void)
{
	XEvent      event;

	event.type = MotionNotify;
	event.xmotion.display = x_disp;
	event.xmotion.window = x_win;
	event.xmotion.x = vid.width / 2;
	event.xmotion.y = vid.height / 2;
	XSendEvent (x_disp, x_win, False, PointerMotionMask, &event);
	XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0,
				  vid.width / 2, vid.height / 2);
}

static void
event_motion (XEvent *event)
{
	x_time = event->xmotion.time;
	if (x_time <= x_mouse_time) {
		p_mouse_x = event->xmotion.x;
		p_mouse_y = event->xmotion.y;
		return;
	}

	if (dga_active) {
		in_mouse_x += event->xmotion.x_root;
		in_mouse_y += event->xmotion.y_root;
	} else {
		if (vid_fullscreen->int_val || in_grab->int_val) {
			if (!event->xmotion.send_event) {
				unsigned dist_x = abs (vid.width / 2 - event->xmotion.x);
				unsigned dist_y = abs (vid.height / 2 - event->xmotion.y);
				in_mouse_x += (event->xmotion.x - p_mouse_x);
				in_mouse_y += (event->xmotion.y - p_mouse_y);
				if (dist_x > vid.width / 4 || dist_y > vid.height / 4) {
					center_pointer ();
				}
			}
		} else {
			in_mouse_x += (event->xmotion.x - p_mouse_x);
			in_mouse_y += (event->xmotion.y - p_mouse_y);
		}
		p_mouse_x = event->xmotion.x;
		p_mouse_y = event->xmotion.y;
	}
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

void
IN_LL_Grab_Input (int grab)
{
	static int input_grabbed = 0;

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
		if (in_dga->int_val)
			dga_on ();
	} else {
		XUngrabPointer (x_disp, CurrentTime);
		XUngrabKeyboard (x_disp, CurrentTime);
		input_grabbed = 0;
		if (in_dga->int_val)
			dga_off ();
	}
}

void
IN_LL_ProcessEvents (void)
{
	X11_ProcessEvents ();	// Get events from X server.
}

void
IN_LL_Shutdown (void)
{
	Sys_Printf ("IN_LL_Shutdown\n");
	in_mouse_avail = 0;
	if (x_disp) {
		XAutoRepeatOn (x_disp);
		dga_off ();
	}
	if (in_mouse_accel && !in_mouse_accel->int_val)
		X11_RestoreMouseAcceleration ();
	X11_CloseDisplay ();
}

void
IN_LL_Init (void)
{
	// open the display
	if (!x_disp)
		Sys_Error ("IN: No display!!");
	if (!x_win)
		Sys_Error ("IN: No window!!");

	X11_OpenDisplay (); // call to increment the reference counter

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

	if (!COM_CheckParm ("-nomouse")) {
		dga_avail = VID_CheckDGA (x_disp, NULL, NULL, NULL);

		X11_AddEvent (ButtonPress, &event_button);
		X11_AddEvent (ButtonRelease, &event_button);
		X11_AddEvent (MotionNotify, &event_motion);

		in_mouse_avail = 1;
	}

	Cmd_AddCommand ("in_paste_buffer", in_paste_buffer_f,
					"Paste the contents of X's C&P buffer to the console");
}

void
IN_LL_Init_Cvars (void)
{
	in_snd_block = Cvar_Get ("in_snd_block", "0", CVAR_ARCHIVE, NULL,
							 "block sound output on window focus loss");
	in_dga = Cvar_Get ("in_dga", "1", CVAR_ARCHIVE, in_dga_f,
					   "DGA Input support");
	in_mouse_accel = Cvar_Get ("in_mouse_accel", "1", CVAR_ARCHIVE,
							   in_mouse_accel_f,
							   "set to 0 to remove mouse acceleration");
}

void
IN_LL_ClearStates (void)
{
}
