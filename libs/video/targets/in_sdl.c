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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <SDL.h>

#include "QF/cdaudio.h"
#include "QF/console.h"
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

cvar_t *in_snd_block;

static keydest_t old_key_dest = key_none;
static int have_focus;


static void
event_focusout (void)
{
	if (have_focus) {
		have_focus = 0;
		if (in_snd_block->int_val) {
			S_BlockSound ();
			CDAudio_Pause ();
		}
	}
}

static void
event_focusin (void)
{
	have_focus = 1;
	if (in_snd_block->int_val) {
		S_UnblockSound ();
		CDAudio_Resume ();
	}
}

void
IN_LL_ProcessEvents (void)
{
	SDL_Event   event;
	int         sym, state, but;
	knum_t		ksym;
	short		unicode;


	while (SDL_PollEvent (&event)) {
		// Ugly key repeat handling. Should use a key_dest callback...
		if (old_key_dest != key_dest) {
			old_key_dest = key_dest;
			if (key_dest == key_game)
				SDL_EnableKeyRepeat (0, SDL_DEFAULT_REPEAT_INTERVAL);
			else
				SDL_EnableKeyRepeat (SDL_DEFAULT_REPEAT_DELAY,
									 SDL_DEFAULT_REPEAT_INTERVAL);
		}

		switch (event.type) {
			case SDL_ACTIVEEVENT:
				if (event.active.state == SDL_APPINPUTFOCUS) {
					if (event.active.gain)
						event_focusin ();
					else
						event_focusout ();
				}
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				unicode = event.key.keysym.unicode;
				switch (sym) {
					case SDLK_UNKNOWN:
						ksym = QFK_UNKNOWN;
						break;
					case SDLK_BACKSPACE:
						ksym = QFK_BACKSPACE;
						break;
					case SDLK_TAB:
						ksym = QFK_TAB;
						break;
					case SDLK_CLEAR:
						ksym = QFK_CLEAR;
						break;
					case SDLK_RETURN:
						ksym = QFK_RETURN;
						break;
					case SDLK_PAUSE:
						ksym = QFK_PAUSE;
						break;
					case SDLK_ESCAPE:
						ksym = QFK_ESCAPE;
						break;
					case SDLK_SPACE:
						ksym = QFK_SPACE;
						break;
					case SDLK_EXCLAIM:
						ksym = QFK_EXCLAIM;
						break;
					case SDLK_QUOTEDBL:
						ksym = QFK_QUOTEDBL;
						break;
					case SDLK_HASH:
						ksym = QFK_HASH;
						break;
					case SDLK_DOLLAR:
						ksym = QFK_DOLLAR;
						break;
					case SDLK_AMPERSAND:
						ksym = QFK_AMPERSAND;
						break;
					case SDLK_QUOTE:
						ksym = QFK_QUOTE;
						break;
					case SDLK_LEFTPAREN:
						ksym = QFK_LEFTPAREN;
						break;
					case SDLK_RIGHTPAREN:
						ksym = QFK_RIGHTPAREN;
						break;
					case SDLK_ASTERISK:
						ksym = QFK_ASTERISK;
						break;
					case SDLK_PLUS:
						ksym = QFK_PLUS;
						break;
					case SDLK_COMMA:
						ksym = QFK_COMMA;
						break;
					case SDLK_MINUS:
						ksym = QFK_MINUS;
						break;
					case SDLK_PERIOD:
						ksym = QFK_PERIOD;
						break;
					case SDLK_SLASH:
						ksym = QFK_SLASH;
						break;
					case SDLK_0:
						ksym = QFK_0;
						break;
					case SDLK_1:
						ksym = QFK_1;
						break;
					case SDLK_2:
						ksym = QFK_2;
						break;
					case SDLK_3:
						ksym = QFK_3;
						break;
					case SDLK_4:
						ksym = QFK_4;
						break;
					case SDLK_5:
						ksym = QFK_5;
						break;
					case SDLK_6:
						ksym = QFK_6;
						break;
					case SDLK_7:
						ksym = QFK_7;
						break;
					case SDLK_8:
						ksym = QFK_8;
						break;
					case SDLK_9:
						ksym = QFK_9;
						break;
					case SDLK_COLON:
						ksym = QFK_COLON;
						break;
					case SDLK_SEMICOLON:
						ksym = QFK_SEMICOLON;
						break;
					case SDLK_LESS:
						ksym = QFK_LESS;
						break;
					case SDLK_EQUALS:
						ksym = QFK_EQUALS;
						break;
					case SDLK_GREATER:
						ksym = QFK_GREATER;
						break;
					case SDLK_QUESTION:
						ksym = QFK_QUESTION;
						break;
					case SDLK_AT:
						ksym = QFK_AT;
						break;
					case SDLK_LEFTBRACKET:
						ksym = QFK_LEFTBRACKET;
						break;
					case SDLK_BACKSLASH:
						ksym = QFK_BACKSLASH;
						break;
					case SDLK_RIGHTBRACKET:
						ksym = QFK_RIGHTBRACKET;
						break;
					case SDLK_CARET:
						ksym = QFK_CARET;
						break;
					case SDLK_UNDERSCORE:
						ksym = QFK_UNDERSCORE;
						break;
					case SDLK_BACKQUOTE:
						ksym = QFK_BACKQUOTE;
						break;
					case SDLK_a:
						ksym = QFK_a;
						break;
					case SDLK_b:
						ksym = QFK_b;
						break;
					case SDLK_c:
						ksym = QFK_c;
						break;
					case SDLK_d:
						ksym = QFK_d;
						break;
					case SDLK_e:
						ksym = QFK_e;
						break;
					case SDLK_f:
						ksym = QFK_f;
						break;
					case SDLK_g:
						ksym = QFK_g;
						break;
					case SDLK_h:
						ksym = QFK_h;
						break;
					case SDLK_i:
						ksym = QFK_i;
						break;
					case SDLK_j:
						ksym = QFK_j;
						break;
					case SDLK_k:
						ksym = QFK_k;
						break;
					case SDLK_l:
						ksym = QFK_l;
						break;
					case SDLK_m:
						ksym = QFK_m;
						break;
					case SDLK_n:
						ksym = QFK_n;
						break;
					case SDLK_o:
						ksym = QFK_o;
						break;
					case SDLK_p:
						ksym = QFK_p;
						break;
					case SDLK_q:
						ksym = QFK_q;
						break;
					case SDLK_r:
						ksym = QFK_r;
						break;
					case SDLK_s:
						ksym = QFK_s;
						break;
					case SDLK_t:
						ksym = QFK_t;
						break;
					case SDLK_u:
						ksym = QFK_u;
						break;
					case SDLK_v:
						ksym = QFK_v;
						break;
					case SDLK_w:
						ksym = QFK_w;
						break;
					case SDLK_x:
						ksym = QFK_x;
						break;
					case SDLK_y:
						ksym = QFK_y;
						break;
					case SDLK_z:
						ksym = QFK_z;
						break;
					case SDLK_DELETE:
						ksym = QFK_DELETE;
						break;
					case SDLK_WORLD_0:
						ksym = QFK_WORLD_0;
						break;
					case SDLK_WORLD_1:
						ksym = QFK_WORLD_1;
						break;
					case SDLK_WORLD_2:
						ksym = QFK_WORLD_2;
						break;
					case SDLK_WORLD_3:
						ksym = QFK_WORLD_3;
						break;
					case SDLK_WORLD_4:
						ksym = QFK_WORLD_4;
						break;
					case SDLK_WORLD_5:
						ksym = QFK_WORLD_5;
						break;
					case SDLK_WORLD_6:
						ksym = QFK_WORLD_6;
						break;
					case SDLK_WORLD_7:
						ksym = QFK_WORLD_7;
						break;
					case SDLK_WORLD_8:
						ksym = QFK_WORLD_8;
						break;
					case SDLK_WORLD_9:
						ksym = QFK_WORLD_9;
						break;
					case SDLK_WORLD_10:
						ksym = QFK_WORLD_10;
						break;
					case SDLK_WORLD_11:
						ksym = QFK_WORLD_11;
						break;
					case SDLK_WORLD_12:
						ksym = QFK_WORLD_12;
						break;
					case SDLK_WORLD_13:
						ksym = QFK_WORLD_13;
						break;
					case SDLK_WORLD_14:
						ksym = QFK_WORLD_14;
						break;
					case SDLK_WORLD_15:
						ksym = QFK_WORLD_15;
						break;
					case SDLK_WORLD_16:
						ksym = QFK_WORLD_16;
						break;
					case SDLK_WORLD_17:
						ksym = QFK_WORLD_17;
						break;
					case SDLK_WORLD_18:
						ksym = QFK_WORLD_18;
						break;
					case SDLK_WORLD_19:
						ksym = QFK_WORLD_19;
						break;
					case SDLK_WORLD_20:
						ksym = QFK_WORLD_20;
						break;
					case SDLK_WORLD_21:
						ksym = QFK_WORLD_21;
						break;
					case SDLK_WORLD_22:
						ksym = QFK_WORLD_22;
						break;
					case SDLK_WORLD_23:
						ksym = QFK_WORLD_23;
						break;
					case SDLK_WORLD_24:
						ksym = QFK_WORLD_24;
						break;
					case SDLK_WORLD_25:
						ksym = QFK_WORLD_25;
						break;
					case SDLK_WORLD_26:
						ksym = QFK_WORLD_26;
						break;
					case SDLK_WORLD_27:
						ksym = QFK_WORLD_27;
						break;
					case SDLK_WORLD_28:
						ksym = QFK_WORLD_28;
						break;
					case SDLK_WORLD_29:
						ksym = QFK_WORLD_29;
						break;
					case SDLK_WORLD_30:
						ksym = QFK_WORLD_30;
						break;
					case SDLK_WORLD_31:
						ksym = QFK_WORLD_31;
						break;
					case SDLK_WORLD_32:
						ksym = QFK_WORLD_32;
						break;
					case SDLK_WORLD_33:
						ksym = QFK_WORLD_33;
						break;
					case SDLK_WORLD_34:
						ksym = QFK_WORLD_34;
						break;
					case SDLK_WORLD_35:
						ksym = QFK_WORLD_35;
						break;
					case SDLK_WORLD_36:
						ksym = QFK_WORLD_36;
						break;
					case SDLK_WORLD_37:
						ksym = QFK_WORLD_37;
						break;
					case SDLK_WORLD_38:
						ksym = QFK_WORLD_38;
						break;
					case SDLK_WORLD_39:
						ksym = QFK_WORLD_39;
						break;
					case SDLK_WORLD_40:
						ksym = QFK_WORLD_40;
						break;
					case SDLK_WORLD_41:
						ksym = QFK_WORLD_41;
						break;
					case SDLK_WORLD_42:
						ksym = QFK_WORLD_42;
						break;
					case SDLK_WORLD_43:
						ksym = QFK_WORLD_43;
						break;
					case SDLK_WORLD_44:
						ksym = QFK_WORLD_44;
						break;
					case SDLK_WORLD_45:
						ksym = QFK_WORLD_45;
						break;
					case SDLK_WORLD_46:
						ksym = QFK_WORLD_46;
						break;
					case SDLK_WORLD_47:
						ksym = QFK_WORLD_47;
						break;
					case SDLK_WORLD_48:
						ksym = QFK_WORLD_48;
						break;
					case SDLK_WORLD_49:
						ksym = QFK_WORLD_49;
						break;
					case SDLK_WORLD_50:
						ksym = QFK_WORLD_50;
						break;
					case SDLK_WORLD_51:
						ksym = QFK_WORLD_51;
						break;
					case SDLK_WORLD_52:
						ksym = QFK_WORLD_52;
						break;
					case SDLK_WORLD_53:
						ksym = QFK_WORLD_53;
						break;
					case SDLK_WORLD_54:
						ksym = QFK_WORLD_54;
						break;
					case SDLK_WORLD_55:
						ksym = QFK_WORLD_55;
						break;
					case SDLK_WORLD_56:
						ksym = QFK_WORLD_56;
						break;
					case SDLK_WORLD_57:
						ksym = QFK_WORLD_57;
						break;
					case SDLK_WORLD_58:
						ksym = QFK_WORLD_58;
						break;
					case SDLK_WORLD_59:
						ksym = QFK_WORLD_59;
						break;
					case SDLK_WORLD_60:
						ksym = QFK_WORLD_60;
						break;
					case SDLK_WORLD_61:
						ksym = QFK_WORLD_61;
						break;
					case SDLK_WORLD_62:
						ksym = QFK_WORLD_62;
						break;
					case SDLK_WORLD_63:
						ksym = QFK_WORLD_63;
						break;
					case SDLK_WORLD_64:
						ksym = QFK_WORLD_64;
						break;
					case SDLK_WORLD_65:
						ksym = QFK_WORLD_65;
						break;
					case SDLK_WORLD_66:
						ksym = QFK_WORLD_66;
						break;
					case SDLK_WORLD_67:
						ksym = QFK_WORLD_67;
						break;
					case SDLK_WORLD_68:
						ksym = QFK_WORLD_68;
						break;
					case SDLK_WORLD_69:
						ksym = QFK_WORLD_69;
						break;
					case SDLK_WORLD_70:
						ksym = QFK_WORLD_70;
						break;
					case SDLK_WORLD_71:
						ksym = QFK_WORLD_71;
						break;
					case SDLK_WORLD_72:
						ksym = QFK_WORLD_72;
						break;
					case SDLK_WORLD_73:
						ksym = QFK_WORLD_73;
						break;
					case SDLK_WORLD_74:
						ksym = QFK_WORLD_74;
						break;
					case SDLK_WORLD_75:
						ksym = QFK_WORLD_75;
						break;
					case SDLK_WORLD_76:
						ksym = QFK_WORLD_76;
						break;
					case SDLK_WORLD_77:
						ksym = QFK_WORLD_77;
						break;
					case SDLK_WORLD_78:
						ksym = QFK_WORLD_78;
						break;
					case SDLK_WORLD_79:
						ksym = QFK_WORLD_79;
						break;
					case SDLK_WORLD_80:
						ksym = QFK_WORLD_80;
						break;
					case SDLK_WORLD_81:
						ksym = QFK_WORLD_81;
						break;
					case SDLK_WORLD_82:
						ksym = QFK_WORLD_82;
						break;
					case SDLK_WORLD_83:
						ksym = QFK_WORLD_83;
						break;
					case SDLK_WORLD_84:
						ksym = QFK_WORLD_84;
						break;
					case SDLK_WORLD_85:
						ksym = QFK_WORLD_85;
						break;
					case SDLK_WORLD_86:
						ksym = QFK_WORLD_86;
						break;
					case SDLK_WORLD_87:
						ksym = QFK_WORLD_87;
						break;
					case SDLK_WORLD_88:
						ksym = QFK_WORLD_88;
						break;
					case SDLK_WORLD_89:
						ksym = QFK_WORLD_89;
						break;
					case SDLK_WORLD_90:
						ksym = QFK_WORLD_90;
						break;
					case SDLK_WORLD_91:
						ksym = QFK_WORLD_91;
						break;
					case SDLK_WORLD_92:
						ksym = QFK_WORLD_92;
						break;
					case SDLK_WORLD_93:
						ksym = QFK_WORLD_93;
						break;
					case SDLK_WORLD_94:
						ksym = QFK_WORLD_94;
						break;
					case SDLK_WORLD_95:
						ksym = QFK_WORLD_95;
						break;
					case SDLK_KP0:
						ksym = QFK_KP0;
						break;
					case SDLK_KP1:
						ksym = QFK_KP1;
						break;
					case SDLK_KP2:
						ksym = QFK_KP2;
						break;
					case SDLK_KP3:
						ksym = QFK_KP3;
						break;
					case SDLK_KP4:
						ksym = QFK_KP4;
						break;
					case SDLK_KP5:
						ksym = QFK_KP5;
						break;
					case SDLK_KP6:
						ksym = QFK_KP6;
						break;
					case SDLK_KP7:
						ksym = QFK_KP7;
						break;
					case SDLK_KP8:
						ksym = QFK_KP8;
						break;
					case SDLK_KP9:
						ksym = QFK_KP9;
						break;
					case SDLK_KP_PERIOD:
						ksym = QFK_KP_PERIOD;
						break;
					case SDLK_KP_DIVIDE:
						ksym = QFK_KP_DIVIDE;
						break;
					case SDLK_KP_MULTIPLY:
						ksym = QFK_KP_MULTIPLY;
						break;
					case SDLK_KP_MINUS:
						ksym = QFK_KP_MINUS;
						break;
					case SDLK_KP_PLUS:
						ksym = QFK_KP_PLUS;
						break;
					case SDLK_KP_ENTER:
						ksym = QFK_KP_ENTER;
						break;
					case SDLK_KP_EQUALS:
						ksym = QFK_KP_EQUALS;
						break;
					case SDLK_UP:
						ksym = QFK_UP;
						break;
					case SDLK_DOWN:
						ksym = QFK_DOWN;
						break;
					case SDLK_RIGHT:
						ksym = QFK_RIGHT;
						break;
					case SDLK_LEFT:
						ksym = QFK_LEFT;
						break;
					case SDLK_INSERT:
						ksym = QFK_INSERT;
						break;
					case SDLK_HOME:
						ksym = QFK_HOME;
						break;
					case SDLK_END:
						ksym = QFK_END;
						break;
					case SDLK_PAGEUP:
						ksym = QFK_PAGEUP;
						break;
					case SDLK_PAGEDOWN:
						ksym = QFK_PAGEDOWN;
						break;
					case SDLK_F1:
						ksym = QFK_F1;
						break;
					case SDLK_F2:
						ksym = QFK_F2;
						break;
					case SDLK_F3:
						ksym = QFK_F3;
						break;
					case SDLK_F4:
						ksym = QFK_F4;
						break;
					case SDLK_F5:
						ksym = QFK_F5;
						break;
					case SDLK_F6:
						ksym = QFK_F6;
						break;
					case SDLK_F7:
						ksym = QFK_F7;
						break;
					case SDLK_F8:
						ksym = QFK_F8;
						break;
					case SDLK_F9:
						ksym = QFK_F9;
						break;
					case SDLK_F10:
						ksym = QFK_F10;
						break;
					case SDLK_F11:
						ksym = QFK_F11;
						break;
					case SDLK_F12:
						ksym = QFK_F12;
						break;
					case SDLK_F13:
						ksym = QFK_F13;
						break;
					case SDLK_F14:
						ksym = QFK_F14;
						break;
					case SDLK_F15:
						ksym = QFK_F15;
						break;
					case SDLK_NUMLOCK:
						ksym = QFK_NUMLOCK;
						break;
					case SDLK_CAPSLOCK:
						ksym = QFK_CAPSLOCK;
						break;
					case SDLK_SCROLLOCK:
						ksym = QFK_SCROLLOCK;
						break;
					case SDLK_RSHIFT:
						ksym = QFK_RSHIFT;
						break;
					case SDLK_LSHIFT:
						ksym = QFK_LSHIFT;
						break;
					case SDLK_RCTRL:
						ksym = QFK_RCTRL;
						break;
					case SDLK_LCTRL:
						ksym = QFK_LCTRL;
						break;
					case SDLK_RALT:
						ksym = QFK_RALT;
						break;
					case SDLK_LALT:
						ksym = QFK_LALT;
						break;
					case SDLK_RMETA:
						ksym = QFK_RMETA;
						break;
					case SDLK_LMETA:
						ksym = QFK_LMETA;
						break;
					case SDLK_LSUPER:
						ksym = QFK_LSUPER;
						break;
					case SDLK_RSUPER:
						ksym = QFK_RSUPER;
						break;
					case SDLK_MODE:
						ksym = QFK_MODE;
						break;
					case SDLK_COMPOSE:
						ksym = QFK_COMPOSE;
						break;
					case SDLK_HELP:
						ksym = QFK_HELP;
						break;
					case SDLK_PRINT:
						ksym = QFK_PRINT;
						break;
					case SDLK_SYSREQ:
						ksym = QFK_SYSREQ;
						break;
					case SDLK_BREAK:
						ksym = QFK_BREAK;
						break;
					case SDLK_MENU:
						ksym = QFK_MENU;
						break;
					case SDLK_POWER:
						ksym = QFK_POWER;
						break;
					case SDLK_EURO:
						ksym = QFK_EURO;
						break;
					case SDLK_LAST:
						ksym = QFK_LAST;
						break;
					default:
						ksym = QFK_UNKNOWN;
						break;
				}
				if (unicode > 255)
					unicode = 0;
				Key_Event (ksym, unicode, state);
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
						Key_Event (QFM_BUTTON1 + but - 1, 0,
								   event.type == SDL_MOUSEBUTTONDOWN);
						break;
					case 4:
						Key_Event (QFM_WHEEL_UP, 0,
								   event.type == SDL_MOUSEBUTTONDOWN);
						break;
					case 5:
						Key_Event (QFM_WHEEL_DOWN, 0,
								   event.type == SDL_MOUSEBUTTONDOWN);
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				in_mouse_x += event.motion.xrel;
				in_mouse_y += event.motion.yrel;
				break;

			case SDL_QUIT:
				Sys_Quit ();
				break;
			default:
				break;
		}
	}
}

void
IN_LL_Grab_Input (int grab)
{
	static int input_grabbed = 0;

	if ((input_grabbed && grab) || (!input_grabbed && !grab))
		return;
	input_grabbed = (SDL_GRAB_ON == SDL_WM_GrabInput (grab ? SDL_GRAB_ON
														   : SDL_GRAB_OFF));
}

void
IN_LL_Init (void)
{
	SDL_EnableUNICODE (1);	// Enable UNICODE translation for keyboard input

	if (COM_CheckParm ("-nomouse"))
		return;

	in_mouse_x = in_mouse_y = 0.0;
	in_mouse_avail = 1;
}

void
IN_LL_Init_Cvars (void)
{
	in_snd_block = Cvar_Get ("in_snd_block", "0", CVAR_ARCHIVE, NULL,
							 "block sound output on window focus loss");
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
