/*
	keys.c

	(description)

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef _WIN32
# include <windows.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "compat.h"

/*
  key up events are sent even if in console mode
*/

cvar_t     *cl_chatmode;

#define		MAXCMDLINE	256
char        key_lines[32][MAXCMDLINE];
int         key_linepos;
int         key_lastpress;

int         edit_line = 0;
int         history_line = 0;

keydest_t   key_dest = key_console;
kgt_t		game_target = KGT_CONSOLE;

char       *keybindings[KGT_LAST][K_LAST];
int			keydown[K_LAST];

qboolean    chat_team;
char        chat_buffer[MAXCMDLINE];
int         chat_bufferlen = 0;

typedef struct {
	char	*name;
	kgt_t	kgtnum;
} kgtname_t;

kgtname_t   kgtnames[] = {
	{"KGT_CONSOLE",	KGT_CONSOLE},
	{"KGT_DEFAULT",	KGT_DEFAULT},
	{"KGT_0",		KGT_DEFAULT},
	{"KGT_1",		KGT_1},
	{"KGT_2",		KGT_2},
	{"KGT_3",		KGT_3},
	{"KGT_4",		KGT_4},
	{"KGT_5",		KGT_5},
	{"KGT_6",		KGT_6},
	{"KGT_7",		KGT_7},
	{"KGT_8",		KGT_8},
	{"KGT_9",		KGT_9},
	{"KGT_10",		KGT_10},
	{"KGT_11",		KGT_11},
	{"KGT_12",		KGT_12},
	{"KGT_13",		KGT_13},
	{"KGT_14",		KGT_14},
	{"KGT_15",		KGT_15},
	{"KGT_16",		KGT_16},

	{NULL, 0}
};

typedef struct {
	char	*name;
	knum_t	keynum;
} keyname_t;

keyname_t   keynames[] = {
	{ "K_UNKNOWN",		K_UNKNOWN },
	{ "K_FIRST",		K_FIRST },
	{ "K_BACKSPACE",	K_BACKSPACE },
	{ "K_TAB",			K_TAB },
	{ "K_CLEAR",		K_CLEAR },
	{ "K_RETURN",		K_RETURN },
	{ "K_PAUSE",		K_PAUSE },
	{ "K_ESCAPE",		K_ESCAPE },
	{ "K_SPACE",		K_SPACE },
	{ "K_EXCLAIM",		K_EXCLAIM },
	{ "K_QUOTEDBL",		K_QUOTEDBL },
	{ "K_HASH",			K_HASH },
	{ "K_DOLLAR",		K_DOLLAR },
	{ "K_AMPERSAND",	K_AMPERSAND },
	{ "K_QUOTE",		K_QUOTE },
	{ "K_LEFTPAREN",	K_LEFTPAREN },
	{ "K_RIGHTPAREN",	K_RIGHTPAREN },
	{ "K_ASTERISK",		K_ASTERISK },
	{ "K_PLUS",			K_PLUS },
	{ "K_COMMA",		K_COMMA },
	{ "K_MINUS",		K_MINUS },
	{ "K_PERIOD",		K_PERIOD },
	{ "K_SLASH",		K_SLASH },
	{ "K_0",			K_0 },
	{ "K_1",			K_1 },
	{ "K_2",			K_2 },
	{ "K_3",			K_3 },
	{ "K_4",			K_4 },
	{ "K_5",			K_5 },
	{ "K_6",			K_6 },
	{ "K_7",			K_7 },
	{ "K_8",			K_8 },
	{ "K_9",			K_9 },
	{ "K_COLON",		K_COLON },
	{ "K_SEMICOLON",	K_SEMICOLON },
	{ "K_LESS",			K_LESS },
	{ "K_EQUALS",		K_EQUALS },
	{ "K_GREATER",		K_GREATER },
	{ "K_QUESTION",		K_QUESTION },
	{ "K_AT",			K_AT },
	{ "K_LEFTBRACKET",	K_LEFTBRACKET },
	{ "K_BACKSLASH",	K_BACKSLASH },
	{ "K_RIGHTBRACKET",	K_RIGHTBRACKET },
	{ "K_CARET",		K_CARET },
	{ "K_UNDERSCORE",	K_UNDERSCORE },
	{ "K_BACKQUOTE",	K_BACKQUOTE },
	{ "K_a",			K_a },
	{ "K_b",			K_b },
	{ "K_c",			K_c },
	{ "K_d",			K_d },
	{ "K_e",			K_e },
	{ "K_f",			K_f },
	{ "K_g",			K_g },
	{ "K_h",			K_h },
	{ "K_i",			K_i },
	{ "K_j",			K_j },
	{ "K_k",			K_k },
	{ "K_l",			K_l },
	{ "K_m",			K_m },
	{ "K_n",			K_n },
	{ "K_o",			K_o },
	{ "K_p",			K_p },
	{ "K_q",			K_q },
	{ "K_r",			K_r },
	{ "K_s",			K_s },
	{ "K_t",			K_t },
	{ "K_u",			K_u },
	{ "K_v",			K_v },
	{ "K_w",			K_w },
	{ "K_x",			K_x },
	{ "K_y",			K_y },
	{ "K_z",			K_z },
	{ "K_DELETE",		K_DELETE },
	{ "K_WORLD_0",		K_WORLD_0 },
	{ "K_WORLD_1",		K_WORLD_1 },
	{ "K_WORLD_2",		K_WORLD_2 },
	{ "K_WORLD_3",		K_WORLD_3 },
	{ "K_WORLD_4",		K_WORLD_4 },
	{ "K_WORLD_5",		K_WORLD_5 },
	{ "K_WORLD_6",		K_WORLD_6 },
	{ "K_WORLD_7",		K_WORLD_7 },
	{ "K_WORLD_8",		K_WORLD_8 },
	{ "K_WORLD_9",		K_WORLD_9 },
	{ "K_WORLD_10",		K_WORLD_10 },
	{ "K_WORLD_11",		K_WORLD_11 },
	{ "K_WORLD_12",		K_WORLD_12 },
	{ "K_WORLD_13",		K_WORLD_13 },
	{ "K_WORLD_14",		K_WORLD_14 },
	{ "K_WORLD_15",		K_WORLD_15 },
	{ "K_WORLD_16",		K_WORLD_16 },
	{ "K_WORLD_17",		K_WORLD_17 },
	{ "K_WORLD_18",		K_WORLD_18 },
	{ "K_WORLD_19",		K_WORLD_19 },
	{ "K_WORLD_20",		K_WORLD_20 },
	{ "K_WORLD_21",		K_WORLD_21 },
	{ "K_WORLD_22",		K_WORLD_22 },
	{ "K_WORLD_23",		K_WORLD_23 },
	{ "K_WORLD_24",		K_WORLD_24 },
	{ "K_WORLD_25",		K_WORLD_25 },
	{ "K_WORLD_26",		K_WORLD_26 },
	{ "K_WORLD_27",		K_WORLD_27 },
	{ "K_WORLD_28",		K_WORLD_28 },
	{ "K_WORLD_29",		K_WORLD_29 },
	{ "K_WORLD_30",		K_WORLD_30 },
	{ "K_WORLD_31",		K_WORLD_31 },
	{ "K_WORLD_32",		K_WORLD_32 },
	{ "K_WORLD_33",		K_WORLD_33 },
	{ "K_WORLD_34",		K_WORLD_34 },
	{ "K_WORLD_35",		K_WORLD_35 },
	{ "K_WORLD_36",		K_WORLD_36 },
	{ "K_WORLD_37",		K_WORLD_37 },
	{ "K_WORLD_38",		K_WORLD_38 },
	{ "K_WORLD_39",		K_WORLD_39 },
	{ "K_WORLD_40",		K_WORLD_40 },
	{ "K_WORLD_41",		K_WORLD_41 },
	{ "K_WORLD_42",		K_WORLD_42 },
	{ "K_WORLD_43",		K_WORLD_43 },
	{ "K_WORLD_44",		K_WORLD_44 },
	{ "K_WORLD_45",		K_WORLD_45 },
	{ "K_WORLD_46",		K_WORLD_46 },
	{ "K_WORLD_47",		K_WORLD_47 },
	{ "K_WORLD_48",		K_WORLD_48 },
	{ "K_WORLD_49",		K_WORLD_49 },
	{ "K_WORLD_50",		K_WORLD_50 },
	{ "K_WORLD_51",		K_WORLD_51 },
	{ "K_WORLD_52",		K_WORLD_52 },
	{ "K_WORLD_53",		K_WORLD_53 },
	{ "K_WORLD_54",		K_WORLD_54 },
	{ "K_WORLD_55",		K_WORLD_55 },
	{ "K_WORLD_56",		K_WORLD_56 },
	{ "K_WORLD_57",		K_WORLD_57 },
	{ "K_WORLD_58",		K_WORLD_58 },
	{ "K_WORLD_59",		K_WORLD_59 },
	{ "K_WORLD_60",		K_WORLD_60 },
	{ "K_WORLD_61",		K_WORLD_61 },
	{ "K_WORLD_62",		K_WORLD_62 },
	{ "K_WORLD_63",		K_WORLD_63 },
	{ "K_WORLD_64",		K_WORLD_64 },
	{ "K_WORLD_65",		K_WORLD_65 },
	{ "K_WORLD_66",		K_WORLD_66 },
	{ "K_WORLD_67",		K_WORLD_67 },
	{ "K_WORLD_68",		K_WORLD_68 },
	{ "K_WORLD_69",		K_WORLD_69 },
	{ "K_WORLD_70",		K_WORLD_70 },
	{ "K_WORLD_71",		K_WORLD_71 },
	{ "K_WORLD_72",		K_WORLD_72 },
	{ "K_WORLD_73",		K_WORLD_73 },
	{ "K_WORLD_74",		K_WORLD_74 },
	{ "K_WORLD_75",		K_WORLD_75 },
	{ "K_WORLD_76",		K_WORLD_76 },
	{ "K_WORLD_77",		K_WORLD_77 },
	{ "K_WORLD_78",		K_WORLD_78 },
	{ "K_WORLD_79",		K_WORLD_79 },
	{ "K_WORLD_80",		K_WORLD_80 },
	{ "K_WORLD_81",		K_WORLD_81 },
	{ "K_WORLD_82",		K_WORLD_82 },
	{ "K_WORLD_83",		K_WORLD_83 },
	{ "K_WORLD_84",		K_WORLD_84 },
	{ "K_WORLD_85",		K_WORLD_85 },
	{ "K_WORLD_86",		K_WORLD_86 },
	{ "K_WORLD_87",		K_WORLD_87 },
	{ "K_WORLD_88",		K_WORLD_88 },
	{ "K_WORLD_89",		K_WORLD_89 },
	{ "K_WORLD_90",		K_WORLD_90 },
	{ "K_WORLD_91",		K_WORLD_91 },
	{ "K_WORLD_92",		K_WORLD_92 },
	{ "K_WORLD_93",		K_WORLD_93 },
	{ "K_WORLD_94",		K_WORLD_94 },
	{ "K_WORLD_95",		K_WORLD_95 },
	{ "K_KP0",			K_KP0 },
	{ "K_KP1",			K_KP1 },
	{ "K_KP2",			K_KP2 },
	{ "K_KP3",			K_KP3 },
	{ "K_KP4",			K_KP4 },
	{ "K_KP5",			K_KP5 },
	{ "K_KP6",			K_KP6 },
	{ "K_KP7",			K_KP7 },
	{ "K_KP8",			K_KP8 },
	{ "K_KP9",			K_KP9 },
	{ "K_KP_PERIOD",	K_KP_PERIOD },
	{ "K_KP_DIVIDE",	K_KP_DIVIDE },
	{ "K_KP_MULTIPLY",	K_KP_MULTIPLY },
	{ "K_KP_MINUS",		K_KP_MINUS },
	{ "K_KP_PLUS",		K_KP_PLUS },
	{ "K_KP_ENTER",		K_KP_ENTER },
	{ "K_KP_EQUALS",	K_KP_EQUALS },
	{ "K_UP",			K_UP },
	{ "K_DOWN",			K_DOWN },
	{ "K_RIGHT",		K_RIGHT },
	{ "K_LEFT",			K_LEFT },
	{ "K_INSERT",		K_INSERT },
	{ "K_HOME",			K_HOME },
	{ "K_END",			K_END },
	{ "K_PAGEUP",		K_PAGEUP },
	{ "K_PAGEDOWN",		K_PAGEDOWN },
	{ "K_F1",			K_F1 },
	{ "K_F2",			K_F2 },
	{ "K_F3",			K_F3 },
	{ "K_F4",			K_F4 },
	{ "K_F5",			K_F5 },
	{ "K_F6",			K_F6 },
	{ "K_F7",			K_F7 },
	{ "K_F8",			K_F8 },
	{ "K_F9",			K_F9 },
	{ "K_F10",			K_F10 },
	{ "K_F11",			K_F11 },
	{ "K_F12",			K_F12 },
	{ "K_F13",			K_F13 },
	{ "K_F14",			K_F14 },
	{ "K_F15",			K_F15 },
	{ "K_NUMLOCK",		K_NUMLOCK },
	{ "K_CAPSLOCK",		K_CAPSLOCK },
	{ "K_SCROLLOCK",	K_SCROLLOCK },
	{ "K_RSHIFT",		K_RSHIFT },
	{ "K_LSHIFT",		K_LSHIFT },
	{ "K_RCTRL",		K_RCTRL },
	{ "K_LCTRL",		K_LCTRL },
	{ "K_RALT",			K_RALT },
	{ "K_LALT",			K_LALT },
	{ "K_RMETA",		K_RMETA },
	{ "K_LMETA",		K_LMETA },
	{ "K_LSUPER",		K_LSUPER },
	{ "K_RSUPER",		K_RSUPER },
	{ "K_MODE",			K_MODE },
	{ "K_COMPOSE",		K_COMPOSE },
	{ "K_HELP",			K_HELP },
	{ "K_PRINT",		K_PRINT },
	{ "K_SYSREQ",		K_SYSREQ },
	{ "K_BREAK",		K_BREAK },
	{ "K_MENU",			K_MENU },
	{ "K_POWER",		K_POWER },
	{ "K_EURO",			K_EURO },
	{ "M_BUTTON1",		M_BUTTON1 },
	{ "M_BUTTON2",		M_BUTTON2 },
	{ "M_BUTTON3",		M_BUTTON3 },
	{ "M_WHEEL_UP",	M_WHEEL_UP },
	{ "M_WHEEL_DOWN",	M_WHEEL_DOWN },

	{ "J_BUTTON1",		J_BUTTON1 },
	{ "J_BUTTON2",		J_BUTTON2 },
	{ "J_BUTTON3",		J_BUTTON3 },
	{ "J_BUTTON4",		J_BUTTON4 },
	{ "J_BUTTON5",		J_BUTTON5 },
	{ "J_BUTTON6",		J_BUTTON6 },
	{ "J_BUTTON7",		J_BUTTON7 },
	{ "J_BUTTON8",		J_BUTTON8 },
	{ "J_BUTTON9",		J_BUTTON9 },
	{ "J_BUTTON10",	J_BUTTON10 },
	{ "J_BUTTON11",	J_BUTTON11 },
	{ "J_BUTTON12",	J_BUTTON12 },
	{ "J_BUTTON13",	J_BUTTON13 },
	{ "J_BUTTON14",	J_BUTTON14 },
	{ "J_BUTTON15",	J_BUTTON15 },
	{ "J_BUTTON16",	J_BUTTON16 },
	{ "J_BUTTON17",	J_BUTTON17 },
	{ "J_BUTTON18",	J_BUTTON18 },
	{ "J_BUTTON19",	J_BUTTON19 },
	{ "J_BUTTON20",	J_BUTTON20 },
	{ "J_BUTTON21",	J_BUTTON21 },
	{ "J_BUTTON22",	J_BUTTON22 },
	{ "J_BUTTON23",	J_BUTTON23 },
	{ "J_BUTTON24",	J_BUTTON24 },
	{ "J_BUTTON25",	J_BUTTON25 },
	{ "J_BUTTON26",	J_BUTTON26 },
	{ "J_BUTTON27",	J_BUTTON27 },
	{ "J_BUTTON28",	J_BUTTON28 },
	{ "J_BUTTON29",	J_BUTTON29 },
	{ "J_BUTTON30",	J_BUTTON30 },
	{ "J_BUTTON31",	J_BUTTON31 },
	{ "J_BUTTON32",	J_BUTTON32 },


	{NULL, 0}
};


/*
			LINE TYPING INTO THE CONSOLE
*/


qboolean
CheckForCommand (void)
{
	char        command[128];
	const char *cmd, *s;
	int         i;

	s = key_lines[edit_line] + 1;

	for (i = 0; i < 127; i++)
		if (s[i] <= ' ')
			break;
		else
			command[i] = s[i];
	command[i] = 0;

	cmd = Cmd_CompleteCommand (command);
	if (!cmd || strcmp (cmd, command))
		cmd = Cvar_CompleteVariable (command);
	if (!cmd || strcmp (cmd, command))
		return false;					// just a chat message
	return true;
}

/*
====================
Key_Game

Game key handling.
====================
*/
qboolean
Key_Game (knum_t key, short unicode)
{
	char       *kb;
	char        cmd[1024];

	kb = Key_GetBinding(game_target, key);
	if (!kb && game_target > KGT_DEFAULT)
		kb = Key_GetBinding(KGT_DEFAULT, key);

	/*
	Con_Printf("kb %p, game_target %d, key_dest %d, key %d\n", kb,
			game_target, key_dest, key);
			*/
	if (!kb)
		return false;

	if (!keydown[key]) {
		if (kb[0] == '+') {
			snprintf (cmd, sizeof (cmd), "-%s %d\n", kb + 1, key);
			Cbuf_AddText (cmd);
		}
	} else if (keydown[key] == 1) {
		if (kb[0] == '+') {
			snprintf (cmd, sizeof (cmd), "%s %d\n", kb, key);
			Cbuf_AddText (cmd);
		} else {
			snprintf (cmd, sizeof (cmd), "%s\n", kb);
			Cbuf_AddText (cmd);
		}
	}
	return true;
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void
Key_Console (knum_t key, short unicode)
{
	int i;

	if (keydown[key] != 1)
		return;

	if (Key_Game (key, unicode))
		return;

	if (unicode == '\x0D' || key == K_RETURN) {				// backslash text are commands, else
		if (!strncmp(key_lines[edit_line], "//", 2))
			goto no_lf;
		else if (key_lines[edit_line][1] == '\\' ||
				key_lines[edit_line][1] == '/')
			Cbuf_AddText (key_lines[edit_line] + 2);	// skip the >
		else if (cl_chatmode->int_val != 1 && CheckForCommand ())
			Cbuf_AddText (key_lines[edit_line] + 1);	// valid command
		else if (cl_chatmode->int_val) {		// convert to a chat message
			Cbuf_AddText ("say ");
			Cbuf_AddText (key_lines[edit_line] + 1);	// skip the >
		} else
			Cbuf_AddText (key_lines[edit_line] + 1);	// skip the >

		Cbuf_AddText ("\n");
no_lf:
		Con_Printf ("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = '\0';
		key_linepos = 1;
		return;
	}

	if (unicode == '\t') {				// command completion
		Con_CompleteCommandLine ();
		return;
	}

	if (unicode == '\x08') {
		if (key_linepos > 1) {
			memmove(key_lines[edit_line] + key_linepos - 1,
					key_lines[edit_line] + key_linepos,
					strlen(key_lines[edit_line] + key_linepos) + 1);
			key_linepos--;
		}
		return;
	}

	if (key == K_DELETE) {
		if (key_linepos < strlen (key_lines[edit_line])) {
			memmove(key_lines[edit_line] + key_linepos,
					key_lines[edit_line] + key_linepos + 1,
					strlen(key_lines[edit_line] + key_linepos + 1) + 1);
		}
		return;
	}

	if (key == K_RIGHT) {
		if (key_linepos < strlen (key_lines[edit_line]))
			key_linepos++;
		return;
	}
	if (key == K_LEFT) {
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == K_UP) {
		do {
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1) & 31;
		strcpy (key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen (key_lines[edit_line]);
		return;
	}

	if (key == K_DOWN) {
		if (history_line == edit_line)
			return;
		do {
			history_line = (history_line + 1) & 31;
		}
		while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line) {
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		} else {
			strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen (key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PAGEUP || key == M_WHEEL_UP) {
		con->display -= 2;
		return;
	}

	if (key == K_PAGEDOWN || key == M_WHEEL_DOWN) {
		con->display += 2;
		if (con->display > con->current)
			con->display = con->current;
		return;
	}

	if (key == K_HOME) {
		key_linepos = 1;
		return;
	}

	if (key == K_END) {
		key_linepos = strlen (key_lines[edit_line]);
		return;
	}

	if (unicode < 32 || unicode > 127)
		return;							// non printable

	i = strlen (key_lines[edit_line]);
	if (i >= MAXCMDLINE - 1)
		return;

	// This also moves the ending \0
	memmove (key_lines[edit_line] + key_linepos + 1,
			key_lines[edit_line] + key_linepos, i - key_linepos + 1);
	key_lines[edit_line][key_linepos] = unicode;
	key_linepos++;
}

void
Key_Message (knum_t key, short unicode)
{
	if (keydown[key] != 1)
		return;

	if (unicode == '\n') {
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText (chat_buffer);
		Cbuf_AddText ("\"\n");

		key_dest = key_game;
		game_target = KGT_DEFAULT;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (unicode == '\x1b' || key == K_ESCAPE) {
		key_dest = key_game;
		game_target = KGT_DEFAULT;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (unicode == '\b') {
		if (chat_bufferlen) {
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof (chat_buffer) - 1)
		return;							// all full

	if (key < 32 || key > 127)
		return;							// non printable

	chat_buffer[chat_bufferlen++] = unicode;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKgtnum

Returns a kgt number to be used to index kgtbindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int
Key_StringToKgtnum (const char *str)
{
	kgtname_t  *kn;

	if (!str || !str[0])
		return -1;

	for (kn = kgtnames; kn->name; kn++) {
		if (!strcasecmp (str, kn->name))
			return kn->kgtnum;
	}
	return -1;
}

/*
===================
Key_KgtnumToString

Returns a string (a K_* name) for the
given kgtnum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *
Key_KgtnumToString (const int kgtnum)
{
	kgtname_t  *kn;

	if (kgtnum == -1)
		return "<KGT NOT FOUND>";

	for (kn = kgtnames; kn->name; kn++)
		if (kgtnum == kn->kgtnum)
			return kn->name;

	return "<UNKNOWN KGTNUM>";
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int
Key_StringToKeynum (const char *str)
{
	keyname_t  *kn;

	if (!str || !str[0])
		return -1;

	for (kn = keynames; kn->name; kn++) {
		if (!strcasecmp (str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
const char *
Key_KeynumToString (knum_t keynum)
{
	keyname_t  *kn;

	if (keynum == -1)
		return "<KEY NOT FOUND>";

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

/*
===================
Key_Unbind_f
===================
*/
void
Key_Unbind_f (void)
{
	int         b, t;

	if (Cmd_Argc () != 3) {
		Con_Printf ("unbind <kgt> <key> : remove commands from a key\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	Key_SetBinding (t, b, "");
}

void
Key_Unbindall_f (void)
{
	int         i, j;

	for (j = 0; j < KGT_LAST; j++)
		for (i = 0; i < K_LAST; i++)
			Key_SetBinding (j, i, NULL);
}


/*
===================
Key_Bind_f
===================
*/
void
Key_Bind_f (void)
{
	int         i, c, t, b;
	char        cmd[1024];

	c = Cmd_Argc ();

	if (c != 3 && c != 4) {
		Con_Printf ("bind <kgt> <key> [command] : attach a command to a key\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	if (c == 3) {
		if (Key_GetBinding(t, b))
			Con_Printf ("\"%s\"[\"%s\"] = \"%s\"\n", Cmd_Argv (2), Cmd_Argv (1),
						Key_GetBinding(t, b));
		else
			Con_Printf ("\"%s\"[\"%s\"] is not bound\n", Cmd_Argv (2),
						Cmd_Argv (1));
		return;
	}
// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	for (i = 3; i < c; i++) {
		strcat (cmd, Cmd_Argv (i));
		if (i != (c - 1))
			strcat (cmd, " ");
	}

	Key_SetBinding (t, b, cmd);
}

/*
===================
Key_GameTarget_f
===================
*/
void
Key_GameTarget_f (void)
{
	int         c, t;

	c = Cmd_Argc ();

	if (c != 2) {
		Con_Printf ("kgt <kgt> : set to a specific key game target\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	game_target = t;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void
Key_WriteBindings (VFile *f)
{
	int			i, j;
	const char	*bind;

	for (j = 0; j < KGT_LAST; j++)
		for (i = 0; i < K_LAST; i++)
			if ((bind = Key_GetBinding(j, i)))
				Qprintf (f, "bind %s %s \"%s\"\n", Key_KgtnumToString (j),
						 Key_KeynumToString (i), bind);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void
Key_Event (knum_t key, short unicode, qboolean down)
{
//  Con_Printf ("%d %d %d : %d\n", game_target, key_dest, key, down); //@@@

	if (down)
		keydown[key]++;
	else
		keydown[key] = 0;

	key_lastpress = key;
//
// handle escape specialy, so the user can never unbind it
//
	if (unicode == '\x1b' || key == K_ESCAPE) {
		if (!down || (keydown[key] > 1))
			return;
		switch (key_dest) {
			case key_message:
				Key_Message (key, unicode);
				break;
			case key_game:
			case key_console:
				Con_ToggleConsole_f ();
				break;
			default:
				Sys_Error ("Bad key_dest");
		}
		return;
	}

	if (!down && Key_Game (key, unicode))
		return;

//
// if not a consolekey, send to the interpreter no matter what mode is
//
	switch (key_dest) {
		case key_message:
			Key_Message (key, unicode);
			break;
		case key_game:
			Key_Game (key, unicode);
			break;
		case key_console:
			Key_Console (key, unicode);
			break;
		default:
			Sys_Error ("Bad key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void
Key_ClearStates (void)
{
	int         i;

	for (i = 0; i < K_LAST; i++) {
		if (keydown[i])
			Key_Event (i, 0, false);
		keydown[i] = false;
	}
}

/*
===================
Key_Init
===================
*/
void
Key_Init (void)
{
	int         i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	// register our functions
	Cmd_AddCommand ("bind", Key_Bind_f,
					"Assign a command or a set of commands to a key.\n"
					"Note: To bind multiple commands to a key, enclose the "
					"commands in quotes and separate with semi-colons. \n"
					"To bind to non-printable keys, use the key name.\n"
					"Key Name List: Escape, F1-F12, pause, backspace, tab, "
					"semicolon, enter, shift, ctrl, alt, space, ins,\n"
					"home, pgup, del, end, pgdn, uparrow, downarrow, "
					"leftarrow, rightarrow, mouse1-mouse3, aux1-aux9, "
					"joy1-joy4,\n"
					"mwheelup, mwheeldown\n"
					"Special: The escape, and ~ (tilde) keys can only be "
					"bound from an external configuration file.");
	Cmd_AddCommand ("unbind", Key_Unbind_f,
					"Remove the bind from the the selected key");
	Cmd_AddCommand ("unbindall", Key_Unbindall_f,
					"Remove all binds (USE CAUTIOUSLY!!!)");
	Cmd_AddCommand ("kgt", Key_GameTarget_f, "");
}

void
Key_Init_Cvars (void)
{
	cl_chatmode = Cvar_Get ("cl_chatmode", "2", CVAR_NONE, NULL,
							"Controls when console text will be treated as a "
							"chat message: 0 - never, 1 - always, 2 - smart");
}
void
Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;		// clear any typing
	key_linepos = 1;
}

char *
Key_GetBinding (kgt_t kgt, knum_t key)
{
	return keybindings[game_target][key];
}

void
Key_SetBinding (kgt_t target, knum_t keynum, char *binding)
{
	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings[target][keynum]) {
		free (keybindings[target][keynum]);
		keybindings[target][keynum] = NULL;
	}
	// allocate memory for new binding
	if (binding) {
		keybindings[target][keynum] = strdup(binding);
	}
}
