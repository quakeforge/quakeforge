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

#include "compat.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/vid.h"

#ifdef WIN32
// FIXME: this is evil...
# include <windows.h>
HWND        mainwindow;
#endif

cvar_t     *m_filter;
cvar_t     *_windowed_mouse;
int         old_windowed_mouse;

int         modestate;					// FIXME: just to avoid cross-comp.

										// errors - remove later

/*
	IN_SendKeyEvents
*/

void
IN_LL_SendKeyEvents (void)
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
							sym = KP_INS;
						else
							sym = SDLK_0;
						break;
					case SDLK_KP1:
						if (modstate & KMOD_NUM)
							sym = KP_END;
						else
							sym = SDLK_1;
						break;
					case SDLK_KP2:
						if (modstate & KMOD_NUM)
							sym = KP_DOWNARROW;
						else
							sym = SDLK_2;
						break;
					case SDLK_KP3:
						if (modstate & KMOD_NUM)
							sym = KP_PGDN;
						else
							sym = SDLK_3;
						break;
					case SDLK_KP4:
						if (modstate & KMOD_NUM)
							sym = KP_LEFTARROW;
						else
							sym = SDLK_4;
						break;
					case SDLK_KP5:
						if (modstate & KMOD_NUM)
							sym = KP_5;
						else
							sym = SDLK_5;
						break;
					case SDLK_KP6:
						if (modstate & KMOD_NUM)
							sym = KP_RIGHTARROW;
						else
							sym = SDLK_6;
						break;
					case SDLK_KP7:
						if (modstate & KMOD_NUM)
							sym = KP_HOME;
						else
							sym = SDLK_7;
						break;
					case SDLK_KP8:
						if (modstate & KMOD_NUM)
							sym = KP_UPARROW;
						else
							sym = SDLK_8;
						break;
					case SDLK_KP9:
						if (modstate & KMOD_NUM)
							sym = KP_PGUP;
						else
							sym = SDLK_9;
						break;
					case SDLK_KP_PERIOD:
						if (modstate & KMOD_NUM)
							sym = KP_DEL;
						else
							sym = SDLK_PERIOD;
						break;
					case SDLK_KP_DIVIDE:
						sym = KP_DIVIDE;
						break;
					case SDLK_KP_MULTIPLY:
						sym = KP_MULTIPLY;
						break;
					case SDLK_KP_MINUS:
						sym = KP_MINUS;
						break;
					case SDLK_KP_PLUS:
						sym = KP_PLUS;
						break;
					case SDLK_KP_ENTER:
						sym = KP_ENTER;
						break;
					case SDLK_KP_EQUALS:
						sym = SDLK_EQUALS;
						break;
				}
				// If we're not directly handled and still >= K_NUM_KEYS
				// just force it to 0
				if (sym >= K_NUM_KEYS)
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
						Key_Event (K_MWHEELUP, 0, true);
						Key_Event (K_MWHEELUP, 0, false);
						break;
					case 5:
						Key_Event (K_MWHEELDOWN, 0, true);
						Key_Event (K_MWHEELDOWN, 0, false);
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				in_mouse_x += event.motion.xrel;
				in_mouse_y += event.motion.yrel;
				break;

			case SDL_QUIT:
				//CL_Disconnect ();
				Sys_Quit ();
				break;
			default:
				break;
		}
	}
}


void
IN_LL_Commands (void)
{
	if (old_windowed_mouse != _windowed_mouse->value) {
		old_windowed_mouse = _windowed_mouse->value;
		if (!_windowed_mouse->value) {
			SDL_ShowCursor (1);
			SDL_WM_GrabInput (SDL_GRAB_OFF);
		} else {
			SDL_WM_GrabInput (SDL_GRAB_ON);
			SDL_ShowCursor (0);
		}
	}
}

void
IN_LL_Init (void)
{
	JOY_Init ();

	if (COM_CheckParm ("-nomouse") && !_windowed_mouse->value)
		return;

	in_mouse_x = in_mouse_y = 0.0;
	in_mouse_avail = 1;
}

void
IN_LL_Init_Cvars (void)
{
}

void
IN_LL_Shutdown (void)
{
	in_mouse_avail = 0;
}

void
IN_LL_ClearStates (void)
{
}
