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

	$Id$
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#ifdef HAVE_DGA
# include <X11/extensions/XShm.h>
# include <X11/extensions/xf86dga.h>
#endif

#include "cl_input.h"
#include "client.h"
#include "QF/console.h"
#include "context_x11.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "dga_check.h"
#include "host.h"
#include "input.h"
#include "joystick.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "view.h"

cvar_t     *_windowed_mouse;
cvar_t     *m_filter;

cvar_t     *in_dga;
cvar_t     *in_dga_mouseaccel;

static qboolean dga_avail;
static qboolean dga_active;

static keydest_t old_key_dest = key_none;

static qboolean mouse_avail;
static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;
static int  p_mouse_x, p_mouse_y;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)
#define INPUT_MASK (KEY_MASK | MOUSE_MASK)

static int
XLateKey (XKeyEvent * ev, qboolean modified)
{
	char        tmp[2];
	int         key = 0;
	KeySym      keysym;

	if (!modified) {
		keysym = XLookupKeysym (ev, 0);
	} else {
		XLookupString (ev, tmp, 1, &keysym, NULL);
	}

	switch (keysym) {
		case XK_KP_Page_Up:
			key = KP_PGUP;
			break;
		case XK_Page_Up:
			key = K_PGUP;
			break;

		case XK_KP_Page_Down:
			key = KP_PGDN;
			break;
		case XK_Page_Down:
			key = K_PGDN;
			break;

		case XK_KP_Home:
			key = KP_HOME;
			break;
		case XK_Home:
			key = K_HOME;
			break;

		case XK_KP_End:
			key = KP_END;
			break;
		case XK_End:
			key = K_END;
			break;

		case XK_KP_Left:
			key = KP_LEFTARROW;
			break;
		case XK_Left:
			key = K_LEFTARROW;
			break;

		case XK_KP_Right:
			key = KP_RIGHTARROW;
			break;
		case XK_Right:
			key = K_RIGHTARROW;
			break;

		case XK_KP_Down:
			key = KP_DOWNARROW;
			break;
		case XK_Down:
			key = K_DOWNARROW;
			break;

		case XK_KP_Up:
			key = KP_UPARROW;
			break;
		case XK_Up:
			key = K_UPARROW;
			break;

		case XK_Escape:
			key = K_ESCAPE;
			break;

		case XK_KP_Enter:
			key = KP_ENTER;
			break;
		case XK_Return:
			key = K_ENTER;
			break;

		case XK_Tab:
			key = K_TAB;
			break;

		case XK_F1:
			key = K_F1;
			break;
		case XK_F2:
			key = K_F2;
			break;
		case XK_F3:
			key = K_F3;
			break;
		case XK_F4:
			key = K_F4;
			break;
		case XK_F5:
			key = K_F5;
			break;
		case XK_F6:
			key = K_F6;
			break;
		case XK_F7:
			key = K_F7;
			break;
		case XK_F8:
			key = K_F8;
			break;
		case XK_F9:
			key = K_F9;
			break;
		case XK_F10:
			key = K_F10;
			break;
		case XK_F11:
			key = K_F11;
			break;
		case XK_F12:
			key = K_F12;
			break;

		case XK_BackSpace:
			key = K_BACKSPACE;
			break;

		case XK_KP_Delete:
			key = KP_DEL;
			break;
		case XK_Delete:
			key = K_DEL;
			break;

		case XK_Pause:
			key = K_PAUSE;
			break;

		case XK_Shift_L:
		case XK_Shift_R:
			key = K_SHIFT;
			break;

		case XK_Execute:
		case XK_Control_L:
		case XK_Control_R:
			key = K_CTRL;
			break;

		case XK_Mode_switch:
		case XK_Alt_L:
		case XK_Meta_L:
		case XK_Alt_R:
		case XK_Meta_R:
			key = K_ALT;
			break;

		case XK_Caps_Lock:
			key = K_CAPSLOCK;
			break;
		case XK_KP_Begin:
			key = KP_5;
			break;

		case XK_Insert:
			key = K_INS;
			break;
		case XK_KP_Insert:
			key = KP_INS;
			break;

		case XK_KP_Multiply:
			key = KP_MULTIPLY;
			break;
		case XK_KP_Add:
			key = KP_PLUS;
			break;
		case XK_KP_Subtract:
			key = KP_MINUS;
			break;
		case XK_KP_Divide:
			key = KP_DIVIDE;
			break;

			/* For Sun keyboards */
		case XK_F27:
			key = K_HOME;
			break;
		case XK_F29:
			key = K_PGUP;
			break;
		case XK_F33:
			key = K_END;
			break;
		case XK_F35:
			key = K_PGDN;
			break;

		default:
			if (keysym < 128) {
				/* ASCII keys */
				key = keysym;
				if (!modified && ((key >= 'A') && (key <= 'Z'))) {
					key = key + ('a' - 'A');
				}
			}
			break;
	}

	return key;
}


static void
event_key (XEvent * event)
{
	if (old_key_dest != key_dest) {
		old_key_dest = key_dest;
		if (key_dest == key_game) {
			XAutoRepeatOff (x_disp);
		} else {
			XAutoRepeatOn (x_disp);
		}
	}
	Key_Event (XLateKey (&event->xkey, 0), XLateKey (&event->xkey, 1),
			   event->type == KeyPress);
}


static void
event_button (XEvent * event)
{
	int         but;

	but = event->xbutton.button;
	if (but == 2)
		but = 3;
	else if (but == 3)
		but = 2;
	switch (but) {
		case 1:
		case 2:
		case 3:
			Key_Event (K_MOUSE1 + but - 1, 0, event->type == ButtonPress);
			break;
		case 4:
			Key_Event (K_MWHEELUP, 0, event->type == ButtonPress);
			break;
		case 5:
			Key_Event (K_MWHEELDOWN, 0, event->type == ButtonPress);
			break;
	}
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
event_motion (XEvent * event)
{
	if (dga_active) {
		mouse_x += event->xmotion.x_root * in_dga_mouseaccel->value;
		mouse_y += event->xmotion.y_root * in_dga_mouseaccel->value;
	} else {
		if (vid_fullscreen->int_val || _windowed_mouse->int_val) {
			if (!event->xmotion.send_event) {
				mouse_x += (event->xmotion.x - p_mouse_x);
				mouse_y += (event->xmotion.y - p_mouse_y);
				if (abs (vid.width / 2 - event->xmotion.x) > vid.width / 4
					|| abs (vid.height / 2 - event->xmotion.y) > vid.height / 4) {
					center_pointer ();
				}
			}
		} else {
			mouse_x += (event->xmotion.x - p_mouse_x);
			mouse_y += (event->xmotion.y - p_mouse_y);
		}
		p_mouse_x = event->xmotion.x;
		p_mouse_y = event->xmotion.y;
	}
}


void
IN_Commands (void)
{
	static int  old_windowed_mouse;
	static int  old_in_dga;

	JOY_Command ();

	if ((old_windowed_mouse != _windowed_mouse->int_val)
		|| (old_in_dga != in_dga->int_val)) {
		old_windowed_mouse = _windowed_mouse->int_val;
		old_in_dga = in_dga->int_val;

		if (_windowed_mouse->int_val) {	// grab the pointer
			XGrabPointer (x_disp, x_win, True, MOUSE_MASK, GrabModeAsync,
						  GrabModeAsync, x_win, None, CurrentTime);
#ifdef HAVE_DGA
			if (dga_avail && in_dga->int_val && !dga_active) {
				XF86DGADirectVideo (x_disp, DefaultScreen (x_disp),
									XF86DGADirectMouse);
				dga_active = true;
			}
#endif
		} else {						// ungrab the pointer
#ifdef HAVE_DGA
			if (dga_avail && in_dga->int_val && dga_active) {
				XF86DGADirectVideo (x_disp, DefaultScreen (x_disp), 0);
				dga_active = false;
			}
#endif
			XUngrabPointer (x_disp, CurrentTime);
		}
	}
}


void
IN_SendKeyEvents (void)
{
	/* Get events from X server. */
	x11_process_events ();
}


void
IN_Move (usercmd_t *cmd)
{
	JOY_Move (cmd);

	if (!mouse_avail)
		return;

	if (m_filter->int_val) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;

		old_mouse_x = mouse_x;
		old_mouse_y = mouse_y;
	}

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

	if ((in_strafe.state & 1) || (lookstrafe->int_val && freelook))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if (freelook)
		V_StopPitchDrift ();

	if (freelook && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward->value * mouse_y;
		else
			cmd->forwardmove -= m_forward->value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

/*
  Called at shutdown
*/
void
IN_Shutdown (void)
{
	JOY_Shutdown ();

	Con_Printf ("IN_Shutdown\n");
	mouse_avail = 0;
	if (x_disp) {
		XAutoRepeatOn (x_disp);

#ifdef HAVE_DGA
		if (dga_avail)
			XF86DGADirectVideo (x_disp, DefaultScreen (x_disp), 0);
#endif
	}
	x11_close_display ();
}

void
Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void
IN_Init (void)
{
	// open the display
	if (!x_disp)
		Sys_Error ("IN: No display!!\n");
	if (!x_win)
		Sys_Error ("IN: No window!!\n");

	x11_open_display ();				// call to increment the reference
										// counter

	{
		int         attribmask = CWEventMask;
		XWindowAttributes attribs_1;
		XSetWindowAttributes attribs_2;

		XGetWindowAttributes (x_disp, x_win, &attribs_1);

		attribs_2.event_mask = attribs_1.your_event_mask | INPUT_MASK;

		XChangeWindowAttributes (x_disp, x_win, attribmask, &attribs_2);
	}

	JOY_Init ();

	XAutoRepeatOff (x_disp);

	if (COM_CheckParm ("-nomouse"))
		return;

	dga_avail = VID_CheckDGA (x_disp, NULL, NULL, NULL);
	if (vid_fullscreen->int_val) {
		Cvar_Set (_windowed_mouse, "1");
		_windowed_mouse->flags |= CVAR_ROM;
	}

	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;

	x11_add_event (KeyPress, &event_key);
	x11_add_event (KeyRelease, &event_key);
	x11_add_event (ButtonPress, &event_button);
	x11_add_event (ButtonRelease, &event_button);
	x11_add_event (MotionNotify, &event_motion);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f, "Force view of player to center");
}

void
IN_Init_Cvars (void)
{
	JOY_Init_Cvars ();
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, "With this set to 1, quake will grab the mouse from X");
	m_filter = Cvar_Get ("m_filter", "0", CVAR_ARCHIVE, "Toggle mouse input filtering.");
	in_dga = Cvar_Get ("in_dga", "1", CVAR_ARCHIVE, "DGA Input support");
	in_dga_mouseaccel = Cvar_Get ("in_dga_mouseaccel", "1", CVAR_ARCHIVE,
								  "DGA Mouse accelleration multiplier");
}
