/*
	in_ggi.c

	Input handling for ggi renderer.

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999       Marcus Sundberg [mackan@stacken.kth.se]

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

#define _BSD

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <ggi/ggi.h>

#include "cl_input.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "host.h"
#include "input.h"
#include "joystick.h"
#include "keys.h"
#include "qargs.h"
#include "sys.h"
#include "view.h"

cvar_t     *m_filter;
cvar_t     *_windowed_mouse;

#define NUM_STDBUTTONS	3
#define NUM_BUTTONS	10

static qboolean mouse_avail;
static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;
static int  p_mouse_x, p_mouse_y;
static float old_windowed_mouse;

static ggi_visual_t ggivis = NULL;


static int
XLateKey (ggi_key_event * ev)
{
	int         key = 0;

	if (GII_KTYP (ev->label) == GII_KT_DEAD) {
		ev->label = GII_KVAL (ev->label);
	}
	switch (ev->label) {
		case GIIK_P9:
			key = KP_PGUP;
			break;
		case GIIK_PageUp:
			key = K_PGUP;
			break;

		case GIIK_P3:
			key = KP_PGDN;
			break;
		case GIIK_PageDown:
			key = K_PGDN;
			break;

		case GIIK_P7:
			key = KP_HOME;
			break;
		case GIIK_Home:
			key = K_HOME;
			break;

		case GIIK_P1:
			key = KP_END;
			break;
		case GIIK_End:
			key = K_END;
			break;

		case GIIK_P4:
			key = KP_LEFTARROW;
			break;
		case GIIK_Left:
			key = K_LEFTARROW;
			break;

		case GIIK_P6:
			key = KP_RIGHTARROW;
			break;
		case GIIK_Right:
			key = K_RIGHTARROW;
			break;

		case GIIK_P2:
			key = KP_DOWNARROW;
			break;
		case GIIK_Down:
			key = K_DOWNARROW;
			break;

		case GIIK_P8:
			key = KP_UPARROW;
			break;
		case GIIK_Up:
			key = K_UPARROW;
			break;

		case GIIK_P5:
			key = KP_5;
			break;
		case GIIK_PBegin:
			key = K_AUX32;
			break;

		case GIIK_P0:
			key = KP_INS;
			break;
		case GIIK_Insert:
			key = K_INS;
			break;

		case GIIK_PSeparator:
		case GIIK_PDecimal:
			key = KP_DEL;
			break;
		case GIIUC_Delete:
			key = K_DEL;
			break;

		case GIIK_PStar:
			key = KP_MULTIPLY;
			break;
		case GIIK_PPlus:
			key = KP_PLUS;
			break;
		case GIIK_PMinus:
			key = KP_MINUS;
			break;
		case GIIK_PSlash:
			key = KP_DIVIDE;
			break;

		case GIIK_PEnter:
			key = KP_ENTER;
			break;
		case GIIUC_Return:
			key = K_ENTER;
			break;

		case GIIUC_Escape:
			key = K_ESCAPE;
			break;

		case GIIUC_Tab:
			key = K_TAB;
			break;

		case GIIK_F1:
			key = K_F1;
			break;
		case GIIK_F2:
			key = K_F2;
			break;
		case GIIK_F3:
			key = K_F3;
			break;
		case GIIK_F4:
			key = K_F4;
			break;
		case GIIK_F5:
			key = K_F5;
			break;
		case GIIK_F6:
			key = K_F6;
			break;
		case GIIK_F7:
			key = K_F7;
			break;
		case GIIK_F8:
			key = K_F8;
			break;
		case GIIK_F9:
			key = K_F9;
			break;
		case GIIK_F10:
			key = K_F10;
			break;
		case GIIK_F11:
			key = K_F11;
			break;
		case GIIK_F12:
			key = K_F12;
			break;

		case GIIUC_BackSpace:
			key = K_BACKSPACE;
			break;

		case GIIK_ShiftL:
		case GIIK_ShiftR:
			key = K_SHIFT;
			break;

		case GIIK_Execute:
		case GIIK_CtrlL:
		case GIIK_CtrlR:
			key = K_CTRL;
			break;

		case GIIK_AltL:
		case GIIK_MetaL:
		case GIIK_AltR:
		case GIIK_MetaR:
		case GIIK_AltGr:
		case GIIK_ModeSwitch:
			key = K_ALT;
			break;

		case GIIK_Caps:
			key = K_CAPSLOCK;
			break;
		case GIIK_PrintScreen:
			key = K_PRNTSCR;
			break;
		case GIIK_ScrollLock:
			key = K_SCRLCK;
			break;
		case GIIK_Pause:
			key = K_PAUSE;
			break;
		case GIIK_NumLock:
			key = KP_NUMLCK;
			break;

		case GIIUC_Comma:
		case GIIUC_Minus:
		case GIIUC_Period:
			key = ev->label;
			break;
		case GIIUC_Section:
			key = '~';
			break;

		default:
			if (ev->label >= 0 && ev->label <= 9)
				return ev->label;
			if (ev->label >= 'A' && ev->label <= 'Z') {
				return ev->label - 'A' + 'a';
			}
			if (ev->label >= 'a' && ev->label <= 'z')
				return ev->label;

			if (ev->sym <= 0x7f) {
				key = ev->sym;
				if (key >= 'A' && key <= 'Z') {
					key = key - 'A' + 'a';
				}
				return key;
			}
			if (ev->label <= 0x7f) {
				return ev->label;
			}
			break;
	}

	return key;
}

static void
GetEvent (void)
{
	ggi_event   ev;
	uint32      b;

	ggiEventRead (ggivis, &ev, emAll);
	switch (ev.any.type) {
		case evKeyPress:
			Key_Event (XLateKey (&ev.key), 0, true);
			break;

		case evKeyRelease:
			Key_Event (XLateKey (&ev.key), 0, false);
			break;

		case evPtrRelative:
			mouse_x += (float) ev.pmove.x;
			mouse_y += (float) ev.pmove.y;
			break;

		case evPtrAbsolute:
			mouse_x += (float) (ev.pmove.x - p_mouse_x);
			mouse_y += (float) (ev.pmove.y - p_mouse_y);
			p_mouse_x = ev.pmove.x;
			p_mouse_y = ev.pmove.y;
			break;

		case evPtrButtonPress:
			if (!mouse_avail)
				return;

			b = ev.pbutton.button - 1;

			if (b < NUM_STDBUTTONS) {
				Key_Event (K_MOUSE1 + b, 0, true);
			} else if (b < NUM_STDBUTTONS + 2) {
				b -= 3;
				if (b)
					Key_Event (K_MWHEELDOWN, 0, true);
				else
					Key_Event (K_MWHEELUP, 0, true);
			} else if (b < NUM_BUTTONS) {
				Key_Event (K_AUX32 - NUM_BUTTONS + b, 0, true);
			}
			break;

		case evPtrButtonRelease:
			if (!mouse_avail)
				return;

			b = ev.pbutton.button - 1;

			if (b < NUM_STDBUTTONS) {
				Key_Event (K_MOUSE1 + b, 0, false);
			} else if (b < NUM_STDBUTTONS + 2) {
				b -= 3;
				if (b)
					Key_Event (K_MWHEELDOWN, 0, false);
				else
					Key_Event (K_MWHEELUP, 0, false);
			} else if (b < NUM_BUTTONS) {
				Key_Event (K_AUX32 - NUM_BUTTONS + b, 0, false);
			}
			break;

#if 0
		case ConfigureNotify:
//printf("config notify\n");
			config_notify_width = ev.xconfigure.width;
			config_notify_height = ev.xconfigure.height;
			config_notify = 1;
			break;

		default:
#endif
	}
}


void
IN_SendKeyEvents (void)
{
	/* Get events from LibGGI */
	if (ggivis) {
		struct timeval t = { 0, 0 };

		if (ggiEventPoll (ggivis, emAll, &t)) {
			int         i = ggiEventsQueued (ggivis, emAll);

			while (i--)
				GetEvent ();
		}
	}
}


void
IN_Init (void)
{
	JOY_Init ();

	old_windowed_mouse = -1;			/* Force update */
	if (COM_CheckParm ("-nomouse"))
		return;

	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;
}

void
IN_Init_Cvars (void)
{
	JOY_Init_Cvars ();

	_windowed_mouse = Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, "Have Quake grab the mouse from X when you play");
	m_filter = Cvar_Get ("m_filter", "0", CVAR_ARCHIVE, "Mouse input filtering");
}


void
IN_Shutdown (void)
{
	JOY_Shutdown ();

	Con_Printf ("IN_Shutdown\n");
	mouse_avail = 0;
}


void
IN_Commands (void)
{
	JOY_Command ();

	/* Only supported by LibGII 0.7 or later. */
#ifdef GII_CMDCODE_PREFER_RELPTR
	if (old_windowed_mouse != _windowed_mouse->int_val) {
		gii_event   ev;

		old_windowed_mouse = _windowed_mouse->int_val;

		ev.cmd.size = sizeof (gii_cmd_nodata_event);
		ev.cmd.type = evCommand;
		ev.cmd.target = GII_EV_TARGET_ALL;
		ev.cmd.code = _windowed_mouse->int_val ? GII_CMDCODE_PREFER_RELPTR
			: GII_CMDCODE_PREFER_ABSPTR;

		ggiEventSend (ggivis, &ev);
	}
#endif
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
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

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
