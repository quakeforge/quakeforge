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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <SDL.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "compat.h"

cvar_t	*m_filter;
cvar_t	*in_grab;

int		old_windowed_mouse;
int		modestate;	// FIXME: just to avoid cross-comp errors - remove later


void
IN_LL_SendKeyEvents (void)
{
	SDL_Event   event;
	int         sym, state, but;
	knum_t		ksym;
	short		unicode;

	while (SDL_PollEvent (&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				unicode = event.key.keysym.unicode;
				switch (sym) {
					case SDLK_UNKNOWN:
						ksym = K_UNKNOWN;
						break;
					case SDLK_BACKSPACE:
						ksym = K_BACKSPACE;
						break;
					case SDLK_TAB:
						ksym = K_TAB;
						break;
					case SDLK_CLEAR:
						ksym = K_CLEAR;
						break;
					case SDLK_RETURN:
						ksym = K_RETURN;
						break;
					case SDLK_PAUSE:
						ksym = K_PAUSE;
						break;
					case SDLK_ESCAPE:
						ksym = K_ESCAPE;
						break;
					case SDLK_SPACE:
						ksym = K_SPACE;
						break;
					case SDLK_EXCLAIM:
						ksym = K_EXCLAIM;
						break;
					case SDLK_QUOTEDBL:
						ksym = K_QUOTEDBL;
						break;
					case SDLK_HASH:
						ksym = K_HASH;
						break;
					case SDLK_DOLLAR:
						ksym = K_DOLLAR;
						break;
					case SDLK_AMPERSAND:
						ksym = K_AMPERSAND;
						break;
					case SDLK_QUOTE:
						ksym = K_QUOTE;
						break;
					case SDLK_LEFTPAREN:
						ksym = K_LEFTPAREN;
						break;
					case SDLK_RIGHTPAREN:
						ksym = K_RIGHTPAREN;
						break;
					case SDLK_ASTERISK:
						ksym = K_ASTERISK;
						break;
					case SDLK_PLUS:
						ksym = K_PLUS;
						break;
					case SDLK_COMMA:
						ksym = K_COMMA;
						break;
					case SDLK_MINUS:
						ksym = K_MINUS;
						break;
					case SDLK_PERIOD:
						ksym = K_PERIOD;
						break;
					case SDLK_SLASH:
						ksym = K_SLASH;
						break;
					case SDLK_0:
						ksym = K_0;
						break;
					case SDLK_1:
						ksym = K_1;
						break;
					case SDLK_2:
						ksym = K_2;
						break;
					case SDLK_3:
						ksym = K_3;
						break;
					case SDLK_4:
						ksym = K_4;
						break;
					case SDLK_5:
						ksym = K_5;
						break;
					case SDLK_6:
						ksym = K_6;
						break;
					case SDLK_7:
						ksym = K_7;
						break;
					case SDLK_8:
						ksym = K_8;
						break;
					case SDLK_9:
						ksym = K_9;
						break;
					case SDLK_COLON:
						ksym = K_COLON;
						break;
					case SDLK_SEMICOLON:
						ksym = K_SEMICOLON;
						break;
					case SDLK_LESS:
						ksym = K_LESS;
						break;
					case SDLK_EQUALS:
						ksym = K_EQUALS;
						break;
					case SDLK_GREATER:
						ksym = K_GREATER;
						break;
					case SDLK_QUESTION:
						ksym = K_QUESTION;
						break;
					case SDLK_AT:
						ksym = K_AT;
						break;
					case SDLK_LEFTBRACKET:
						ksym = K_LEFTBRACKET;
						break;
					case SDLK_BACKSLASH:
						ksym = K_BACKSLASH;
						break;
					case SDLK_RIGHTBRACKET:
						ksym = K_RIGHTBRACKET;
						break;
					case SDLK_CARET:
						ksym = K_CARET;
						break;
					case SDLK_UNDERSCORE:
						ksym = K_UNDERSCORE;
						break;
					case SDLK_BACKQUOTE:
						ksym = K_BACKQUOTE;
						break;
					case SDLK_a:
						ksym = K_a;
						break;
					case SDLK_b:
						ksym = K_b;
						break;
					case SDLK_c:
						ksym = K_c;
						break;
					case SDLK_d:
						ksym = K_d;
						break;
					case SDLK_e:
						ksym = K_e;
						break;
					case SDLK_f:
						ksym = K_f;
						break;
					case SDLK_g:
						ksym = K_g;
						break;
					case SDLK_h:
						ksym = K_h;
						break;
					case SDLK_i:
						ksym = K_i;
						break;
					case SDLK_j:
						ksym = K_j;
						break;
					case SDLK_k:
						ksym = K_k;
						break;
					case SDLK_l:
						ksym = K_l;
						break;
					case SDLK_m:
						ksym = K_m;
						break;
					case SDLK_n:
						ksym = K_n;
						break;
					case SDLK_o:
						ksym = K_o;
						break;
					case SDLK_p:
						ksym = K_p;
						break;
					case SDLK_q:
						ksym = K_q;
						break;
					case SDLK_r:
						ksym = K_r;
						break;
					case SDLK_s:
						ksym = K_s;
						break;
					case SDLK_t:
						ksym = K_t;
						break;
					case SDLK_u:
						ksym = K_u;
						break;
					case SDLK_v:
						ksym = K_v;
						break;
					case SDLK_w:
						ksym = K_w;
						break;
					case SDLK_x:
						ksym = K_x;
						break;
					case SDLK_y:
						ksym = K_y;
						break;
					case SDLK_z:
						ksym = K_z;
						break;
					case SDLK_DELETE:
						ksym = K_DELETE;
						break;
					case SDLK_WORLD_0:
						ksym = K_WORLD_0;
						break;
					case SDLK_WORLD_1:
						ksym = K_WORLD_1;
						break;
					case SDLK_WORLD_2:
						ksym = K_WORLD_2;
						break;
					case SDLK_WORLD_3:
						ksym = K_WORLD_3;
						break;
					case SDLK_WORLD_4:
						ksym = K_WORLD_4;
						break;
					case SDLK_WORLD_5:
						ksym = K_WORLD_5;
						break;
					case SDLK_WORLD_6:
						ksym = K_WORLD_6;
						break;
					case SDLK_WORLD_7:
						ksym = K_WORLD_7;
						break;
					case SDLK_WORLD_8:
						ksym = K_WORLD_8;
						break;
					case SDLK_WORLD_9:
						ksym = K_WORLD_9;
						break;
					case SDLK_WORLD_10:
						ksym = K_WORLD_10;
						break;
					case SDLK_WORLD_11:
						ksym = K_WORLD_11;
						break;
					case SDLK_WORLD_12:
						ksym = K_WORLD_12;
						break;
					case SDLK_WORLD_13:
						ksym = K_WORLD_13;
						break;
					case SDLK_WORLD_14:
						ksym = K_WORLD_14;
						break;
					case SDLK_WORLD_15:
						ksym = K_WORLD_15;
						break;
					case SDLK_WORLD_16:
						ksym = K_WORLD_16;
						break;
					case SDLK_WORLD_17:
						ksym = K_WORLD_17;
						break;
					case SDLK_WORLD_18:
						ksym = K_WORLD_18;
						break;
					case SDLK_WORLD_19:
						ksym = K_WORLD_19;
						break;
					case SDLK_WORLD_20:
						ksym = K_WORLD_20;
						break;
					case SDLK_WORLD_21:
						ksym = K_WORLD_21;
						break;
					case SDLK_WORLD_22:
						ksym = K_WORLD_22;
						break;
					case SDLK_WORLD_23:
						ksym = K_WORLD_23;
						break;
					case SDLK_WORLD_24:
						ksym = K_WORLD_24;
						break;
					case SDLK_WORLD_25:
						ksym = K_WORLD_25;
						break;
					case SDLK_WORLD_26:
						ksym = K_WORLD_26;
						break;
					case SDLK_WORLD_27:
						ksym = K_WORLD_27;
						break;
					case SDLK_WORLD_28:
						ksym = K_WORLD_28;
						break;
					case SDLK_WORLD_29:
						ksym = K_WORLD_29;
						break;
					case SDLK_WORLD_30:
						ksym = K_WORLD_30;
						break;
					case SDLK_WORLD_31:
						ksym = K_WORLD_31;
						break;
					case SDLK_WORLD_32:
						ksym = K_WORLD_32;
						break;
					case SDLK_WORLD_33:
						ksym = K_WORLD_33;
						break;
					case SDLK_WORLD_34:
						ksym = K_WORLD_34;
						break;
					case SDLK_WORLD_35:
						ksym = K_WORLD_35;
						break;
					case SDLK_WORLD_36:
						ksym = K_WORLD_36;
						break;
					case SDLK_WORLD_37:
						ksym = K_WORLD_37;
						break;
					case SDLK_WORLD_38:
						ksym = K_WORLD_38;
						break;
					case SDLK_WORLD_39:
						ksym = K_WORLD_39;
						break;
					case SDLK_WORLD_40:
						ksym = K_WORLD_40;
						break;
					case SDLK_WORLD_41:
						ksym = K_WORLD_41;
						break;
					case SDLK_WORLD_42:
						ksym = K_WORLD_42;
						break;
					case SDLK_WORLD_43:
						ksym = K_WORLD_43;
						break;
					case SDLK_WORLD_44:
						ksym = K_WORLD_44;
						break;
					case SDLK_WORLD_45:
						ksym = K_WORLD_45;
						break;
					case SDLK_WORLD_46:
						ksym = K_WORLD_46;
						break;
					case SDLK_WORLD_47:
						ksym = K_WORLD_47;
						break;
					case SDLK_WORLD_48:
						ksym = K_WORLD_48;
						break;
					case SDLK_WORLD_49:
						ksym = K_WORLD_49;
						break;
					case SDLK_WORLD_50:
						ksym = K_WORLD_50;
						break;
					case SDLK_WORLD_51:
						ksym = K_WORLD_51;
						break;
					case SDLK_WORLD_52:
						ksym = K_WORLD_52;
						break;
					case SDLK_WORLD_53:
						ksym = K_WORLD_53;
						break;
					case SDLK_WORLD_54:
						ksym = K_WORLD_54;
						break;
					case SDLK_WORLD_55:
						ksym = K_WORLD_55;
						break;
					case SDLK_WORLD_56:
						ksym = K_WORLD_56;
						break;
					case SDLK_WORLD_57:
						ksym = K_WORLD_57;
						break;
					case SDLK_WORLD_58:
						ksym = K_WORLD_58;
						break;
					case SDLK_WORLD_59:
						ksym = K_WORLD_59;
						break;
					case SDLK_WORLD_60:
						ksym = K_WORLD_60;
						break;
					case SDLK_WORLD_61:
						ksym = K_WORLD_61;
						break;
					case SDLK_WORLD_62:
						ksym = K_WORLD_62;
						break;
					case SDLK_WORLD_63:
						ksym = K_WORLD_63;
						break;
					case SDLK_WORLD_64:
						ksym = K_WORLD_64;
						break;
					case SDLK_WORLD_65:
						ksym = K_WORLD_65;
						break;
					case SDLK_WORLD_66:
						ksym = K_WORLD_66;
						break;
					case SDLK_WORLD_67:
						ksym = K_WORLD_67;
						break;
					case SDLK_WORLD_68:
						ksym = K_WORLD_68;
						break;
					case SDLK_WORLD_69:
						ksym = K_WORLD_69;
						break;
					case SDLK_WORLD_70:
						ksym = K_WORLD_70;
						break;
					case SDLK_WORLD_71:
						ksym = K_WORLD_71;
						break;
					case SDLK_WORLD_72:
						ksym = K_WORLD_72;
						break;
					case SDLK_WORLD_73:
						ksym = K_WORLD_73;
						break;
					case SDLK_WORLD_74:
						ksym = K_WORLD_74;
						break;
					case SDLK_WORLD_75:
						ksym = K_WORLD_75;
						break;
					case SDLK_WORLD_76:
						ksym = K_WORLD_76;
						break;
					case SDLK_WORLD_77:
						ksym = K_WORLD_77;
						break;
					case SDLK_WORLD_78:
						ksym = K_WORLD_78;
						break;
					case SDLK_WORLD_79:
						ksym = K_WORLD_79;
						break;
					case SDLK_WORLD_80:
						ksym = K_WORLD_80;
						break;
					case SDLK_WORLD_81:
						ksym = K_WORLD_81;
						break;
					case SDLK_WORLD_82:
						ksym = K_WORLD_82;
						break;
					case SDLK_WORLD_83:
						ksym = K_WORLD_83;
						break;
					case SDLK_WORLD_84:
						ksym = K_WORLD_84;
						break;
					case SDLK_WORLD_85:
						ksym = K_WORLD_85;
						break;
					case SDLK_WORLD_86:
						ksym = K_WORLD_86;
						break;
					case SDLK_WORLD_87:
						ksym = K_WORLD_87;
						break;
					case SDLK_WORLD_88:
						ksym = K_WORLD_88;
						break;
					case SDLK_WORLD_89:
						ksym = K_WORLD_89;
						break;
					case SDLK_WORLD_90:
						ksym = K_WORLD_90;
						break;
					case SDLK_WORLD_91:
						ksym = K_WORLD_91;
						break;
					case SDLK_WORLD_92:
						ksym = K_WORLD_92;
						break;
					case SDLK_WORLD_93:
						ksym = K_WORLD_93;
						break;
					case SDLK_WORLD_94:
						ksym = K_WORLD_94;
						break;
					case SDLK_WORLD_95:
						ksym = K_WORLD_95;
						break;
					case SDLK_KP0:
						ksym = K_KP0;
						break;
					case SDLK_KP1:
						ksym = K_KP1;
						break;
					case SDLK_KP2:
						ksym = K_KP2;
						break;
					case SDLK_KP3:
						ksym = K_KP3;
						break;
					case SDLK_KP4:
						ksym = K_KP4;
						break;
					case SDLK_KP5:
						ksym = K_KP5;
						break;
					case SDLK_KP6:
						ksym = K_KP6;
						break;
					case SDLK_KP7:
						ksym = K_KP7;
						break;
					case SDLK_KP8:
						ksym = K_KP8;
						break;
					case SDLK_KP9:
						ksym = K_KP9;
						break;
					case SDLK_KP_PERIOD:
						ksym = K_KP_PERIOD;
						break;
					case SDLK_KP_DIVIDE:
						ksym = K_KP_DIVIDE;
						break;
					case SDLK_KP_MULTIPLY:
						ksym = K_KP_MULTIPLY;
						break;
					case SDLK_KP_MINUS:
						ksym = K_KP_MINUS;
						break;
					case SDLK_KP_PLUS:
						ksym = K_KP_PLUS;
						break;
					case SDLK_KP_ENTER:
						ksym = K_KP_ENTER;
						break;
					case SDLK_KP_EQUALS:
						ksym = K_KP_EQUALS;
						break;
					case SDLK_UP:
						ksym = K_UP;
						break;
					case SDLK_DOWN:
						ksym = K_DOWN;
						break;
					case SDLK_RIGHT:
						ksym = K_RIGHT;
						break;
					case SDLK_LEFT:
						ksym = K_LEFT;
						break;
					case SDLK_INSERT:
						ksym = K_INSERT;
						break;
					case SDLK_HOME:
						ksym = K_HOME;
						break;
					case SDLK_END:
						ksym = K_END;
						break;
					case SDLK_PAGEUP:
						ksym = K_PAGEUP;
						break;
					case SDLK_PAGEDOWN:
						ksym = K_PAGEDOWN;
						break;
					case SDLK_F1:
						ksym = K_F1;
						break;
					case SDLK_F2:
						ksym = K_F2;
						break;
					case SDLK_F3:
						ksym = K_F3;
						break;
					case SDLK_F4:
						ksym = K_F4;
						break;
					case SDLK_F5:
						ksym = K_F5;
						break;
					case SDLK_F6:
						ksym = K_F6;
						break;
					case SDLK_F7:
						ksym = K_F7;
						break;
					case SDLK_F8:
						ksym = K_F8;
						break;
					case SDLK_F9:
						ksym = K_F9;
						break;
					case SDLK_F10:
						ksym = K_F10;
						break;
					case SDLK_F11:
						ksym = K_F11;
						break;
					case SDLK_F12:
						ksym = K_F12;
						break;
					case SDLK_F13:
						ksym = K_F13;
						break;
					case SDLK_F14:
						ksym = K_F14;
						break;
					case SDLK_F15:
						ksym = K_F15;
						break;
					case SDLK_NUMLOCK:
						ksym = K_NUMLOCK;
						break;
					case SDLK_CAPSLOCK:
						ksym = K_CAPSLOCK;
						break;
					case SDLK_SCROLLOCK:
						ksym = K_SCROLLOCK;
						break;
					case SDLK_RSHIFT:
						ksym = K_RSHIFT;
						break;
					case SDLK_LSHIFT:
						ksym = K_LSHIFT;
						break;
					case SDLK_RCTRL:
						ksym = K_RCTRL;
						break;
					case SDLK_LCTRL:
						ksym = K_LCTRL;
						break;
					case SDLK_RALT:
						ksym = K_RALT;
						break;
					case SDLK_LALT:
						ksym = K_LALT;
						break;
					case SDLK_RMETA:
						ksym = K_RMETA;
						break;
					case SDLK_LMETA:
						ksym = K_LMETA;
						break;
					case SDLK_LSUPER:
						ksym = K_LSUPER;
						break;
					case SDLK_RSUPER:
						ksym = K_RSUPER;
						break;
					case SDLK_MODE:
						ksym = K_MODE;
						break;
					case SDLK_COMPOSE:
						ksym = K_COMPOSE;
						break;
					case SDLK_HELP:
						ksym = K_HELP;
						break;
					case SDLK_PRINT:
						ksym = K_PRINT;
						break;
					case SDLK_SYSREQ:
						ksym = K_SYSREQ;
						break;
					case SDLK_BREAK:
						ksym = K_BREAK;
						break;
					case SDLK_MENU:
						ksym = K_MENU;
						break;
					case SDLK_POWER:
						ksym = K_POWER;
						break;
					case SDLK_EURO:
						ksym = K_EURO;
						break;
					case SDLK_LAST:
						ksym = K_LAST;
						break;
					default:
						ksym = K_UNKNOWN;
						break;
				}
				if (unicode > 255)
					unicode = 0;
				Key_Event (sym, unicode, state);
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
						Key_Event (M_BUTTON1 + but - 1, 0,
								   event.type == SDL_MOUSEBUTTONDOWN);
						break;
					case 4:
						Key_Event (M_WHEEL_UP, 0, true);
						Key_Event (M_WHEEL_UP, 0, false);
						break;
					case 5:
						Key_Event (M_WHEEL_DOWN, 0, true);
						Key_Event (M_WHEEL_DOWN, 0, false);
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
IN_LL_Grab_Input (void)
{
	SDL_WM_GrabInput (SDL_GRAB_ON);
	SDL_ShowCursor (0);
}

void
IN_LL_Ungrab_Input (void)
{
	SDL_ShowCursor (1);
	SDL_WM_GrabInput (SDL_GRAB_OFF);
}

void
IN_LL_Init (void)
{
	/* Enable UNICODE translation for keyboard input */
	SDL_EnableUNICODE(1);

	if (COM_CheckParm ("-nomouse") && !in_grab->value)
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
