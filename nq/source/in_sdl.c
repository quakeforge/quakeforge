
/*
	in_sdl.c

	general sdl input driver

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <SDL.h>

#include "client.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "draw.h"
#include "host.h"
#include "input.h"
#include "joystick.h"
#include "QF/keys.h"
#include "QF/sys.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "vid.h"
#include "view.h"

#ifdef WIN32
// FIXME: this is evil...
# include <windows.h>
HWND        mainwindow;
#endif

cvar_t     *_windowed_mouse;
int         old_windowed_mouse;

int         modestate;					// FIXME: just to avoid cross-comp.

										// errors - remove later

static qboolean mouse_avail;
static float mouse_x, mouse_y;
static int  mouse_oldbuttonstate = 0;

extern viddef_t vid;					// global video state

/*
	IN_SendKeyEvents
*/

void
IN_SendKeyEvents (void)
{
	SDL_Event   event;
	int         sym, state, but;
	int         modstate;

	while (SDL_PollEvent (&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			sym = event.key.keysym.sym;
			state = event.key.state;
			modstate = SDL_GetModState ();
			switch (sym) {
				case SDLK_DELETE:
				sym = K_DEL;
				break;
				case SDLK_BACKSPACE:
				sym = K_BACKSPACE;
				break;
				case SDLK_F1:
				sym = K_F1;
				break;
				case SDLK_F2:
				sym = K_F2;
				break;
				case SDLK_F3:
				sym = K_F3;
				break;
				case SDLK_F4:
				sym = K_F4;
				break;
				case SDLK_F5:
				sym = K_F5;
				break;
				case SDLK_F6:
				sym = K_F6;
				break;
				case SDLK_F7:
				sym = K_F7;
				break;
				case SDLK_F8:
				sym = K_F8;
				break;
				case SDLK_F9:
				sym = K_F9;
				break;
				case SDLK_F10:
				sym = K_F10;
				break;
				case SDLK_F11:
				sym = K_F11;
				break;
				case SDLK_F12:
				sym = K_F12;
				break;
				case SDLK_BREAK:
				case SDLK_PAUSE:
				sym = K_PAUSE;
				break;
				case SDLK_UP:
				sym = K_UPARROW;
				break;
				case SDLK_DOWN:
				sym = K_DOWNARROW;
				break;
				case SDLK_RIGHT:
				sym = K_RIGHTARROW;
				break;
				case SDLK_LEFT:
				sym = K_LEFTARROW;
				break;
				case SDLK_INSERT:
				sym = K_INS;
				break;
				case SDLK_HOME:
				sym = K_HOME;
				break;
				case SDLK_END:
				sym = K_END;
				break;
				case SDLK_PAGEUP:
				sym = K_PGUP;
				break;
				case SDLK_PAGEDOWN:
				sym = K_PGDN;
				break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
				sym = K_SHIFT;
				break;
				case SDLK_RCTRL:
				case SDLK_LCTRL:
				sym = K_CTRL;
				break;
				case SDLK_RALT:
				case SDLK_LALT:
				sym = K_ALT;
				break;
				case SDLK_CAPSLOCK:
				sym = K_CAPSLOCK;
				break;
				case SDLK_KP0:
				if (modstate & KMOD_NUM)
					sym = K_INS;
				else
					sym = SDLK_0;
				break;
				case SDLK_KP1:
				if (modstate & KMOD_NUM)
					sym = K_END;
				else
					sym = SDLK_1;
				break;
				case SDLK_KP2:
				if (modstate & KMOD_NUM)
					sym = K_DOWNARROW;
				else
					sym = SDLK_2;
				break;
				case SDLK_KP3:
				if (modstate & KMOD_NUM)
					sym = K_PGDN;
				else
					sym = SDLK_3;
				break;
				case SDLK_KP4:
				if (modstate & KMOD_NUM)
					sym = K_LEFTARROW;
				else
					sym = SDLK_4;
				break;
				case SDLK_KP5:
				sym = SDLK_5;
				break;
				case SDLK_KP6:
				if (modstate & KMOD_NUM)
					sym = K_RIGHTARROW;
				else
					sym = SDLK_6;
				break;
				case SDLK_KP7:
				if (modstate & KMOD_NUM)
					sym = K_HOME;
				else
					sym = SDLK_7;
				break;
				case SDLK_KP8:
				if (modstate & KMOD_NUM)
					sym = K_UPARROW;
				else
					sym = SDLK_8;
				break;
				case SDLK_KP9:
				if (modstate & KMOD_NUM)
					sym = K_PGUP;
				else
					sym = SDLK_9;
				break;
				case SDLK_KP_PERIOD:
				if (modstate & KMOD_NUM)
					sym = K_DEL;
				else
					sym = SDLK_PERIOD;
				break;
				case SDLK_KP_DIVIDE:
				sym = SDLK_SLASH;
				break;
				case SDLK_KP_MULTIPLY:
				sym = SDLK_ASTERISK;
				break;
				case SDLK_KP_MINUS:
				sym = SDLK_MINUS;
				break;
				case SDLK_KP_PLUS:
				sym = SDLK_PLUS;
				break;
				case SDLK_KP_ENTER:
				sym = SDLK_RETURN;
				break;
				case SDLK_KP_EQUALS:
				sym = SDLK_EQUALS;
				break;
			}
			// If we're not directly handled and still above 255
			// just force it to 0
			if (sym > 255)
				sym = 0;
			Key_Event (sym, -1, state);
			break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			but = event.button.button;
			if (but == 2)
				but = 3;
			else if (but == 3)
				but = 2;

			switch (but) {
				case 1:
				case 2:
				case 3:
				Key_Event (K_MOUSE1 + but - 1, 0, event.type
						   == SDL_MOUSEBUTTONDOWN);
				break;
				case 4:
				Key_Event (K_MWHEELUP, 0, event.type == SDL_MOUSEBUTTONDOWN);
				break;
				case 5:
				Key_Event (K_MWHEELDOWN, 0, event.type == SDL_MOUSEBUTTONDOWN);
				break;
			}
			break;

			case SDL_MOUSEMOTION:
			if (_windowed_mouse->value) {
				if ((event.motion.x != (vid.width / 2))
					|| (event.motion.y != (vid.height / 2))) {
					// *2 for vid_sdl.c, *10 for vid_sgl.c.
					mouse_x = event.motion.xrel * 5;
					mouse_y = event.motion.yrel * 5;
					if ((event.motion.x < ((vid.width / 2) - (vid.width / 4)))
						|| (event.motion.x >
							((vid.width / 2) + (vid.width / 4)))
						|| (event.motion.y <
							((vid.height / 2) - (vid.height / 4)))
						|| (event.motion.y >
							((vid.height / 2) + (vid.height / 4))))
							SDL_WarpMouse (vid.width / 2, vid.height / 2);
				}
			} else {
				// following are *2 in vid_sdl.c, vid_sgl.c is *10
				mouse_x = event.motion.xrel * 5;
				mouse_y = event.motion.yrel * 5;
			}
			break;

			case SDL_QUIT:
			CL_Disconnect ();
			Sys_Quit ();
			break;
			default:
			break;
		}
	}
}


void
IN_Commands (void)
{
	JOY_Command ();

	if (old_windowed_mouse != _windowed_mouse->value) {
		old_windowed_mouse = _windowed_mouse->value;
		if (!_windowed_mouse->value) {
//          SDL_ShowCursor (0);
			SDL_WM_GrabInput (SDL_GRAB_OFF);
		} else {
			SDL_WM_GrabInput (SDL_GRAB_ON);
//          SDL_ShowCursor (1);
		}
	}
}

void
IN_Init (void)
{
	JOY_Init ();

	if (COM_CheckParm ("-nomouse") && !_windowed_mouse->value)
		return;

	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;
//  SDL_ShowCursor (0); 
//  SDL_WM_GrabInput (SDL_GRAB_ON);
//  FIXME: disable DGA if in_dgamouse says to.
}

void
IN_Init_Cvars (void)
{
	JOY_Init_Cvars ();

	_windowed_mouse =
		Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE,
				  "If set to 1, quake will grab the mouse in X");
	// m_filter = Cvar_Get ("m_filter", "0", CVAR_ARCHIVE, "Toggle mouse
	// input filtering");
}

void
IN_Shutdown (void)
{
	mouse_avail = 0;
}

void
IN_Frame (void)
{
	int         i;
	int         mouse_buttonstate;

	if (!mouse_avail)
		return;

	i = SDL_GetMouseState (NULL, NULL);
	/* Quake swaps the second and third buttons */
	mouse_buttonstate = (i & ~0x06) | ((i & 0x02) << 1) | ((i & 0x04) >> 1);
	for (i = 0; i < 3; i++) {
		if ((mouse_buttonstate & (1 << i))
			&& !(mouse_oldbuttonstate & (1 << i)))
			Key_Event (K_MOUSE1 + i, 0, true);

		if (!(mouse_buttonstate & (1 << i))
			&& (mouse_oldbuttonstate & (1 << i)))
			Key_Event (K_MOUSE1 + i, 0, false);
	}
	mouse_oldbuttonstate = mouse_buttonstate;
}

void
IN_Move (usercmd_t *cmd)
{
	JOY_Move (cmd);

	if (!mouse_avail)
		return;

/* from vid_sdl.c
	if (m_filter->value) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x; 
	old_mouse_y = mouse_y; 
*/

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

	if ((in_strafe.state & 1) || (lookstrafe->value && (in_mlook.state & 1)))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if (freelook)
		V_StopPitchDrift ();

	if (freelook && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] =
			bound (-70, cl.viewangles[PITCH] + (m_pitch->value * mouse_y), 80);
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward->value * mouse_y;
		else
			cmd->forwardmove -= m_forward->value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

void
IN_HandlePause (qboolean paused)
{
}
