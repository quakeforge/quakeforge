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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/csqc.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/zone.h"
#include "QF/gib.h"

#include "compat.h"
#include "old_keys.h"

/*  key up events are sent even if in console mode */

cvar_t     *in_bind_imt;

VISIBLE keydest_t   key_dest = key_console;
VISIBLE imt_t		game_target = IMT_CONSOLE;

VISIBLE struct keybind_s keybindings[IMT_LAST][QFK_LAST];
VISIBLE int			keydown[QFK_LAST];

static int  keyhelp;
static cbuf_t *cbuf;

typedef struct {
	const char *name;
	imt_t	imtnum;
} imtname_t;

imtname_t   imtnames[] = {
	{"IMT_CONSOLE",	IMT_CONSOLE},
	{"IMT_0",		IMT_0},
	{"IMT_1",		IMT_1},
	{"IMT_2",		IMT_2},
	{"IMT_3",		IMT_3},
	{"IMT_4",		IMT_4},
	{"IMT_5",		IMT_5},
	{"IMT_6",		IMT_6},
	{"IMT_7",		IMT_7},
	{"IMT_8",		IMT_8},
	{"IMT_9",		IMT_9},
	{"IMT_10",		IMT_10},
	{"IMT_11",		IMT_11},
	{"IMT_12",		IMT_12},
	{"IMT_13",		IMT_13},
	{"IMT_14",		IMT_14},
	{"IMT_15",		IMT_15},
	{"IMT_16",		IMT_16},

	{"IMT_DEFAULT",	IMT_0},

	{NULL, 0}
};

typedef struct {
	const char *name;
	knum_t	keynum;
} keyname_t;

keyname_t   keynames[] = {
	{ "K_UNKNOWN",		QFK_UNKNOWN },
	{ "K_FIRST",		QFK_FIRST },
	{ "K_BACKSPACE",	QFK_BACKSPACE },
	{ "K_TAB",			QFK_TAB },
	{ "K_CLEAR",		QFK_CLEAR },
	{ "K_RETURN",		QFK_RETURN },
	{ "K_PAUSE",		QFK_PAUSE },
	{ "K_ESCAPE",		QFK_ESCAPE },
	{ "K_SPACE",		QFK_SPACE },
	{ "K_EXCLAIM",		QFK_EXCLAIM },
	{ "K_QUOTEDBL",		QFK_QUOTEDBL },
	{ "K_HASH",			QFK_HASH },
	{ "K_DOLLAR",		QFK_DOLLAR },
	{ "K_PERCENT",		QFK_PERCENT },
	{ "K_AMPERSAND",	QFK_AMPERSAND },
	{ "K_QUOTE",		QFK_QUOTE },
	{ "K_LEFTPAREN",	QFK_LEFTPAREN },
	{ "K_RIGHTPAREN",	QFK_RIGHTPAREN },
	{ "K_ASTERISK",		QFK_ASTERISK },
	{ "K_PLUS",			QFK_PLUS },
	{ "K_COMMA",		QFK_COMMA },
	{ "K_MINUS",		QFK_MINUS },
	{ "K_PERIOD",		QFK_PERIOD },
	{ "K_SLASH",		QFK_SLASH },
	{ "K_0",			QFK_0 },
	{ "K_1",			QFK_1 },
	{ "K_2",			QFK_2 },
	{ "K_3",			QFK_3 },
	{ "K_4",			QFK_4 },
	{ "K_5",			QFK_5 },
	{ "K_6",			QFK_6 },
	{ "K_7",			QFK_7 },
	{ "K_8",			QFK_8 },
	{ "K_9",			QFK_9 },
	{ "K_COLON",		QFK_COLON },
	{ "K_SEMICOLON",	QFK_SEMICOLON },
	{ "K_LESS",			QFK_LESS },
	{ "K_EQUALS",		QFK_EQUALS },
	{ "K_GREATER",		QFK_GREATER },
	{ "K_QUESTION",		QFK_QUESTION },
	{ "K_AT",			QFK_AT },
	{ "K_LEFTBRACKET",	QFK_LEFTBRACKET },
	{ "K_BACKSLASH",	QFK_BACKSLASH },
	{ "K_RIGHTBRACKET",	QFK_RIGHTBRACKET },
	{ "K_CARET",		QFK_CARET },
	{ "K_UNDERSCORE",	QFK_UNDERSCORE },
	{ "K_BACKQUOTE",	QFK_BACKQUOTE },
	{ "K_a",			QFK_a },
	{ "K_b",			QFK_b },
	{ "K_c",			QFK_c },
	{ "K_d",			QFK_d },
	{ "K_e",			QFK_e },
	{ "K_f",			QFK_f },
	{ "K_g",			QFK_g },
	{ "K_h",			QFK_h },
	{ "K_i",			QFK_i },
	{ "K_j",			QFK_j },
	{ "K_k",			QFK_k },
	{ "K_l",			QFK_l },
	{ "K_m",			QFK_m },
	{ "K_n",			QFK_n },
	{ "K_o",			QFK_o },
	{ "K_p",			QFK_p },
	{ "K_q",			QFK_q },
	{ "K_r",			QFK_r },
	{ "K_s",			QFK_s },
	{ "K_t",			QFK_t },
	{ "K_u",			QFK_u },
	{ "K_v",			QFK_v },
	{ "K_w",			QFK_w },
	{ "K_x",			QFK_x },
	{ "K_y",			QFK_y },
	{ "K_z",			QFK_z },
	{ "K_BRACELEFT",	QFK_BRACELEFT },
	{ "K_BAR",			QFK_BAR },
	{ "K_BRACERIGHT",	QFK_BRACERIGHT },
	{ "K_ASCIITILDE",	QFK_ASCIITILDE },
	{ "K_DELETE",		QFK_DELETE },
	{ "K_WORLD_0",		QFK_WORLD_0 },
	{ "K_WORLD_1",		QFK_WORLD_1 },
	{ "K_WORLD_2",		QFK_WORLD_2 },
	{ "K_WORLD_3",		QFK_WORLD_3 },
	{ "K_WORLD_4",		QFK_WORLD_4 },
	{ "K_WORLD_5",		QFK_WORLD_5 },
	{ "K_WORLD_6",		QFK_WORLD_6 },
	{ "K_WORLD_7",		QFK_WORLD_7 },
	{ "K_WORLD_8",		QFK_WORLD_8 },
	{ "K_WORLD_9",		QFK_WORLD_9 },
	{ "K_WORLD_10",		QFK_WORLD_10 },
	{ "K_WORLD_11",		QFK_WORLD_11 },
	{ "K_WORLD_12",		QFK_WORLD_12 },
	{ "K_WORLD_13",		QFK_WORLD_13 },
	{ "K_WORLD_14",		QFK_WORLD_14 },
	{ "K_WORLD_15",		QFK_WORLD_15 },
	{ "K_WORLD_16",		QFK_WORLD_16 },
	{ "K_WORLD_17",		QFK_WORLD_17 },
	{ "K_WORLD_18",		QFK_WORLD_18 },
	{ "K_WORLD_19",		QFK_WORLD_19 },
	{ "K_WORLD_20",		QFK_WORLD_20 },
	{ "K_WORLD_21",		QFK_WORLD_21 },
	{ "K_WORLD_22",		QFK_WORLD_22 },
	{ "K_WORLD_23",		QFK_WORLD_23 },
	{ "K_WORLD_24",		QFK_WORLD_24 },
	{ "K_WORLD_25",		QFK_WORLD_25 },
	{ "K_WORLD_26",		QFK_WORLD_26 },
	{ "K_WORLD_27",		QFK_WORLD_27 },
	{ "K_WORLD_28",		QFK_WORLD_28 },
	{ "K_WORLD_29",		QFK_WORLD_29 },
	{ "K_WORLD_30",		QFK_WORLD_30 },
	{ "K_WORLD_31",		QFK_WORLD_31 },
	{ "K_WORLD_32",		QFK_WORLD_32 },
	{ "K_WORLD_33",		QFK_WORLD_33 },
	{ "K_WORLD_34",		QFK_WORLD_34 },
	{ "K_WORLD_35",		QFK_WORLD_35 },
	{ "K_WORLD_36",		QFK_WORLD_36 },
	{ "K_WORLD_37",		QFK_WORLD_37 },
	{ "K_WORLD_38",		QFK_WORLD_38 },
	{ "K_WORLD_39",		QFK_WORLD_39 },
	{ "K_WORLD_40",		QFK_WORLD_40 },
	{ "K_WORLD_41",		QFK_WORLD_41 },
	{ "K_WORLD_42",		QFK_WORLD_42 },
	{ "K_WORLD_43",		QFK_WORLD_43 },
	{ "K_WORLD_44",		QFK_WORLD_44 },
	{ "K_WORLD_45",		QFK_WORLD_45 },
	{ "K_WORLD_46",		QFK_WORLD_46 },
	{ "K_WORLD_47",		QFK_WORLD_47 },
	{ "K_WORLD_48",		QFK_WORLD_48 },
	{ "K_WORLD_49",		QFK_WORLD_49 },
	{ "K_WORLD_50",		QFK_WORLD_50 },
	{ "K_WORLD_51",		QFK_WORLD_51 },
	{ "K_WORLD_52",		QFK_WORLD_52 },
	{ "K_WORLD_53",		QFK_WORLD_53 },
	{ "K_WORLD_54",		QFK_WORLD_54 },
	{ "K_WORLD_55",		QFK_WORLD_55 },
	{ "K_WORLD_56",		QFK_WORLD_56 },
	{ "K_WORLD_57",		QFK_WORLD_57 },
	{ "K_WORLD_58",		QFK_WORLD_58 },
	{ "K_WORLD_59",		QFK_WORLD_59 },
	{ "K_WORLD_60",		QFK_WORLD_60 },
	{ "K_WORLD_61",		QFK_WORLD_61 },
	{ "K_WORLD_62",		QFK_WORLD_62 },
	{ "K_WORLD_63",		QFK_WORLD_63 },
	{ "K_WORLD_64",		QFK_WORLD_64 },
	{ "K_WORLD_65",		QFK_WORLD_65 },
	{ "K_WORLD_66",		QFK_WORLD_66 },
	{ "K_WORLD_67",		QFK_WORLD_67 },
	{ "K_WORLD_68",		QFK_WORLD_68 },
	{ "K_WORLD_69",		QFK_WORLD_69 },
	{ "K_WORLD_70",		QFK_WORLD_70 },
	{ "K_WORLD_71",		QFK_WORLD_71 },
	{ "K_WORLD_72",		QFK_WORLD_72 },
	{ "K_WORLD_73",		QFK_WORLD_73 },
	{ "K_WORLD_74",		QFK_WORLD_74 },
	{ "K_WORLD_75",		QFK_WORLD_75 },
	{ "K_WORLD_76",		QFK_WORLD_76 },
	{ "K_WORLD_77",		QFK_WORLD_77 },
	{ "K_WORLD_78",		QFK_WORLD_78 },
	{ "K_WORLD_79",		QFK_WORLD_79 },
	{ "K_WORLD_80",		QFK_WORLD_80 },
	{ "K_WORLD_81",		QFK_WORLD_81 },
	{ "K_WORLD_82",		QFK_WORLD_82 },
	{ "K_WORLD_83",		QFK_WORLD_83 },
	{ "K_WORLD_84",		QFK_WORLD_84 },
	{ "K_WORLD_85",		QFK_WORLD_85 },
	{ "K_WORLD_86",		QFK_WORLD_86 },
	{ "K_WORLD_87",		QFK_WORLD_87 },
	{ "K_WORLD_88",		QFK_WORLD_88 },
	{ "K_WORLD_89",		QFK_WORLD_89 },
	{ "K_WORLD_90",		QFK_WORLD_90 },
	{ "K_WORLD_91",		QFK_WORLD_91 },
	{ "K_WORLD_92",		QFK_WORLD_92 },
	{ "K_WORLD_93",		QFK_WORLD_93 },
	{ "K_WORLD_94",		QFK_WORLD_94 },
	{ "K_WORLD_95",		QFK_WORLD_95 },
	{ "K_KP0",			QFK_KP0 },
	{ "K_KP1",			QFK_KP1 },
	{ "K_KP2",			QFK_KP2 },
	{ "K_KP3",			QFK_KP3 },
	{ "K_KP4",			QFK_KP4 },
	{ "K_KP5",			QFK_KP5 },
	{ "K_KP6",			QFK_KP6 },
	{ "K_KP7",			QFK_KP7 },
	{ "K_KP8",			QFK_KP8 },
	{ "K_KP9",			QFK_KP9 },
	{ "K_KP_PERIOD",	QFK_KP_PERIOD },
	{ "K_KP_DIVIDE",	QFK_KP_DIVIDE },
	{ "K_KP_MULTIPLY",	QFK_KP_MULTIPLY },
	{ "K_KP_MINUS",		QFK_KP_MINUS },
	{ "K_KP_PLUS",		QFK_KP_PLUS },
	{ "K_KP_ENTER",		QFK_KP_ENTER },
	{ "K_KP_EQUALS",	QFK_KP_EQUALS },
	{ "K_UP",			QFK_UP },
	{ "K_DOWN",			QFK_DOWN },
	{ "K_RIGHT",		QFK_RIGHT },
	{ "K_LEFT",			QFK_LEFT },
	{ "K_INSERT",		QFK_INSERT },
	{ "K_HOME",			QFK_HOME },
	{ "K_END",			QFK_END },
	{ "K_PAGEUP",		QFK_PAGEUP },
	{ "K_PAGEDOWN",		QFK_PAGEDOWN },
	{ "K_F1",			QFK_F1 },
	{ "K_F2",			QFK_F2 },
	{ "K_F3",			QFK_F3 },
	{ "K_F4",			QFK_F4 },
	{ "K_F5",			QFK_F5 },
	{ "K_F6",			QFK_F6 },
	{ "K_F7",			QFK_F7 },
	{ "K_F8",			QFK_F8 },
	{ "K_F9",			QFK_F9 },
	{ "K_F10",			QFK_F10 },
	{ "K_F11",			QFK_F11 },
	{ "K_F12",			QFK_F12 },
	{ "K_F13",			QFK_F13 },
	{ "K_F14",			QFK_F14 },
	{ "K_F15",			QFK_F15 },
	{ "K_NUMLOCK",		QFK_NUMLOCK },
	{ "K_CAPSLOCK",		QFK_CAPSLOCK },
	{ "K_SCROLLOCK",	QFK_SCROLLOCK },
	{ "K_RSHIFT",		QFK_RSHIFT },
	{ "K_LSHIFT",		QFK_LSHIFT },
	{ "K_RCTRL",		QFK_RCTRL },
	{ "K_LCTRL",		QFK_LCTRL },
	{ "K_RALT",			QFK_RALT },
	{ "K_LALT",			QFK_LALT },
	{ "K_RMETA",		QFK_RMETA },
	{ "K_LMETA",		QFK_LMETA },
	{ "K_LSUPER",		QFK_LSUPER },
	{ "K_RSUPER",		QFK_RSUPER },
	{ "K_MODE",			QFK_MODE },
	{ "K_COMPOSE",		QFK_COMPOSE },
	{ "K_HELP",			QFK_HELP },
	{ "K_PRINT",		QFK_PRINT },
	{ "K_SYSREQ",		QFK_SYSREQ },
	{ "K_BREAK",		QFK_BREAK },
	{ "K_MENU",			QFK_MENU },
	{ "K_POWER",		QFK_POWER },
	{ "K_EURO",			QFK_EURO },
	{ "M_BUTTON1",		QFM_BUTTON1 },
	{ "M_BUTTON2",		QFM_BUTTON2 },
	{ "M_BUTTON3",		QFM_BUTTON3 },
	{ "M_WHEEL_UP",		QFM_WHEEL_UP },
	{ "M_WHEEL_DOWN",	QFM_WHEEL_DOWN },
	{ "M_BUTTON6",		QFM_BUTTON6 },
	{ "M_BUTTON7",		QFM_BUTTON7 },

	{ "J_BUTTON1",		QFJ_BUTTON1 },
	{ "J_BUTTON2",		QFJ_BUTTON2 },
	{ "J_BUTTON3",		QFJ_BUTTON3 },
	{ "J_BUTTON4",		QFJ_BUTTON4 },
	{ "J_BUTTON5",		QFJ_BUTTON5 },
	{ "J_BUTTON6",		QFJ_BUTTON6 },
	{ "J_BUTTON7",		QFJ_BUTTON7 },
	{ "J_BUTTON8",		QFJ_BUTTON8 },
	{ "J_BUTTON9",		QFJ_BUTTON9 },
	{ "J_BUTTON10",		QFJ_BUTTON10 },
	{ "J_BUTTON11",		QFJ_BUTTON11 },
	{ "J_BUTTON12",		QFJ_BUTTON12 },
	{ "J_BUTTON13",		QFJ_BUTTON13 },
	{ "J_BUTTON14",		QFJ_BUTTON14 },
	{ "J_BUTTON15",		QFJ_BUTTON15 },
	{ "J_BUTTON16",		QFJ_BUTTON16 },
	{ "J_BUTTON17",		QFJ_BUTTON17 },
	{ "J_BUTTON18",		QFJ_BUTTON18 },
	{ "J_BUTTON19",		QFJ_BUTTON19 },
	{ "J_BUTTON20",		QFJ_BUTTON20 },
	{ "J_BUTTON21",		QFJ_BUTTON21 },
	{ "J_BUTTON22",		QFJ_BUTTON22 },
	{ "J_BUTTON23",		QFJ_BUTTON23 },
	{ "J_BUTTON24",		QFJ_BUTTON24 },
	{ "J_BUTTON25",		QFJ_BUTTON25 },
	{ "J_BUTTON26",		QFJ_BUTTON26 },
	{ "J_BUTTON27",		QFJ_BUTTON27 },
	{ "J_BUTTON28",		QFJ_BUTTON28 },
	{ "J_BUTTON29",		QFJ_BUTTON29 },
	{ "J_BUTTON30",		QFJ_BUTTON30 },
	{ "J_BUTTON31",		QFJ_BUTTON31 },
	{ "J_BUTTON32",		QFJ_BUTTON32 },

	{NULL, 0}
};


/*
  Key_Game

  Game key handling.
*/
static qboolean
Key_Game (knum_t key, short unicode)
{
	const char *kb;
	char        cmd[1024];

	kb = Key_GetBinding (game_target, key);
	if (!kb && (game_target > IMT_0))
		kb = Key_GetBinding (IMT_0, key);

/*
	Sys_Printf("kb %p, game_target %d, key_dest %d, key %d\n", kb,
			game_target, key_dest, key);
*/
	if (!kb)
		return false;

	if (keydown[key] > 1) 
		return true;

	if (kb[0] == '+') {
		if (keydown[key])
			snprintf (cmd, sizeof (cmd), "%s %d\n", kb, key);
		else
			snprintf (cmd, sizeof (cmd), "-%s %d\n", kb + 1, key);
	} else {
		if (!keydown[key])
			return true;
		snprintf (cmd, sizeof (cmd), "%s\n", kb);
	}
	Cbuf_AddText (cbuf, cmd);
	return true;
}

/*
  Key_Console

  Interactive line editing and console scrollback
*/
static void
Key_Console (knum_t key, short unicode)
{
	// escape is un-bindable
	if (keydown[key] == 1 && key && Key_Game (key, unicode))
		return;

	Con_KeyEvent (key, unicode, keydown[key]);
}

//============================================================================

/*
  Key_StringToIMTnum
  Returns an imt number to be used to index imtbindings[] by looking at
  the given string.  Single ascii characters return themselves, while
  the QFK_* names are matched up.
*/
static int
Key_StringToIMTnum (const char *str)
{
	imtname_t  *kn;

	if (!str || !str[0])
		return -1;

	for (kn = imtnames; kn->name; kn++) {
		if (!strcasecmp (str, kn->name))
			return kn->imtnum;
	}
	return -1;
}

/*
  Key_IMTnumToString

  Returns a string (a QFK_* name) for the given imtnum.
  FIXME: handle quote special (general escape sequence?)
*/
static const char *
Key_IMTnumToString (const imt_t imtnum)
{
	imtname_t  *kn;

	if (imtnum == (imt_t) -1)
		return "<IMT NOT FOUND>";

	for (kn = imtnames; kn->name; kn++)
		if (imtnum == kn->imtnum)
			return kn->name;

	return "<UNKNOWN IMTNUM>";
}

/*
  Key_StringToKeynum

  Returns a key number to be used to index keybindings[] by looking at
  the given string.  Single ascii characters return themselves, while
  the QFK_* names are matched up.
*/
static int
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
  Key_KeynumToString

  Returns a string (a QFK_* name) for the given keynum.
  FIXME: handle quote special (general escape sequence?)
*/
VISIBLE const char *
Key_KeynumToString (knum_t keynum)
{
	keyname_t  *kn;

	if (keynum == (knum_t) -1)
		return "<KEY NOT FOUND>";

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

static void
Key_In_Unbind (const char *imt, const char *key)
{
	int t, b;

	t = Key_StringToIMTnum (imt);
	if (t == -1) {
		Sys_Printf ("\"%s\" isn't a valid imt\n", imt);
		return;
	}

	b = Key_StringToKeynum (key);
	if (b == -1) {
		Sys_Printf ("\"%s\" isn't a valid key\n", key);
		return;
	}

	Key_SetBinding (t, b, NULL);
}

static void
Key_In_Unbind_f (void)
{
	if (Cmd_Argc () != 3) {
		Sys_Printf ("in_unbind <imt> <key> : remove commands from a key\n");
		return;
	}
	Key_In_Unbind (Cmd_Argv (1), Cmd_Argv (2));
}

static void
Key_Unbindall_f (void)
{
	int         i, j;

	for (j = 0; j < IMT_LAST; j++)
		for (i = 0; i < QFK_LAST; i++)
			Key_SetBinding (j, i, NULL);
}

static void
Key_In_Bind (const char *imt, const char *key, const char *cmd)
{
	int t, b;

	t = Key_StringToIMTnum (imt);
	if (t == -1) {
		Sys_Printf ("\"%s\" isn't a valid imt\n", imt);
		return;
	}

	b = Key_StringToKeynum (key);
	if (b == -1) {
		Sys_Printf ("\"%s\" isn't a valid key\n", key);
		return;
	}

	if (!cmd) {
		if (Key_GetBinding (t, b))
			Sys_Printf ("%s %s \"%s\"\n", imt, key,
						Key_GetBinding(t, b));
		else
			Sys_Printf ("%s %s is not bound\n", imt, key);
		return;
	}
	Key_SetBinding (t, b, cmd);
}

static void
Key_In_Bind_f (void)
{
	int         c, i;
	const char *imt, *key, *cmd = 0;
	char        cmd_buf[1024];

	c = Cmd_Argc ();

	if (c < 3) {
		Sys_Printf ("in_bind <imt> <key> [command] : attach a command to a "
					"key\n");
		return;
	}

	imt = Cmd_Argv (1);

	key = Cmd_Argv (2);

	if (c >= 4) {
		cmd = cmd_buf;
		cmd_buf[0] = 0;
		for (i = 3; i < c; i++) {
			strncat (cmd_buf, Cmd_Argv (i), sizeof (cmd_buf) -
					 strlen (cmd_buf));
			if (i != (c - 1))
				strncat (cmd_buf, " ", sizeof (cmd_buf) - strlen (cmd_buf));
		}
	}

	Key_In_Bind (imt, key, cmd);
}

static void
Key_Unbind_f (void)
{
	const char *key;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}
	key = OK_TranslateKeyName (Cmd_Argv (1));
	Key_In_Unbind (in_bind_imt->string, key);
}

static void
Key_Bind_f (void)
{
	int         c, i;
	const char *imt, *key, *cmd = 0;
	char        cmd_buf[1024];

	c = Cmd_Argc ();

	if (c < 2) {
		Sys_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	imt = in_bind_imt->string;

	key = OK_TranslateKeyName (Cmd_Argv (1));

	if (c >= 3) {
		cmd = cmd_buf;
		cmd_buf[0] = 0;
		for (i = 2; i < c; i++) {
			strncat (cmd_buf, Cmd_Argv (i), sizeof (cmd_buf) -
					 strlen (cmd_buf));
			if (i != (c - 1))
				strncat (cmd_buf, " ", sizeof (cmd_buf) - strlen (cmd_buf));
		}
	}

	Key_In_Bind (imt, key, cmd);
}

static void 
Key_GIB_Bind_Get_f (void)
{
	const char *imt, *key, *cmd;
	int t, k;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("key");
		return;
	}

	imt = in_bind_imt->string;

	key = OK_TranslateKeyName (GIB_Argv (1));

	if ((t = Key_StringToIMTnum (imt)) == -1) {
		GIB_Error ("bind", "bind::get: invalid imt %s", imt);
		return;
	}

	if ((k = Key_StringToKeynum (key)) == -1) {
		GIB_Error ("bind", "bind::get: invalid key %s", key);
		return;
	}
	
	if (!(cmd = Key_GetBinding (t, k)))
		GIB_Return ("");
	else
		GIB_Return (cmd);
}


static void
in_bind_imt_f (cvar_t *var)
{
	if (Key_StringToIMTnum (var->string) == -1) {
		Sys_Printf ("\"%s\" is not a valid imt. setting to \"imt_default\"\n",
					var->string);
		Cvar_Set (var, "imt_default");
	}
}

static void
Key_InputMappingTable_f (void)
{
	int         c, t;

	c = Cmd_Argc ();

	if (c != 2) {
		Sys_Printf ("Current imt is %s\n", Key_IMTnumToString(game_target));
		Sys_Printf ("imt <imt> : set to a specific input mapping table\n");
		return;
	}

	t = Key_StringToIMTnum (Cmd_Argv (1));
	if (t == -1) {
		Sys_Printf ("\"%s\" isn't a valid imt\n", Cmd_Argv (1));
		return;
	}

	game_target = t;
}

/*
  Key_WriteBindings

  Writes lines containing "bind key value"
*/
void
Key_WriteBindings (QFile *f)
{
	int			i, j;
	const char	*bind;

	for (j = 0; j < IMT_LAST; j++)
		for (i = 0; i < QFK_LAST; i++)
			if ((bind = Key_GetBinding(j, i))) {
			  if (f) {
					Qprintf (f, "in_bind %s %s \"%s\"\n",
							 Key_IMTnumToString (j),
							 Key_KeynumToString (i), bind);
			  } else {
					Sys_Printf ("in_bind %s %s \"%s\"\n",
							 Key_IMTnumToString (j),
							 Key_KeynumToString (i), bind);
			  }
			}
}

static void
Key_Bindlist_f (void)
{
	Key_WriteBindings (0);
}

static void
keyhelp_f (void)
{
	keyhelp = 1;
}

/*
  Key_Event

  Called by the system between frames for both key up and key down events
  Should NOT be called during an interrupt!
*/
VISIBLE void
Key_Event (knum_t key, short unicode, qboolean down)
{
//  Sys_Printf ("%d %d %d : %d\n", game_target, key_dest, key, down); //@@@

	if (down) {
		keydown[key]++;
		if (keyhelp) {
			Sys_Printf ("Key name for that key is \"%s\"\n",
						Key_KeynumToString (key));
			keyhelp = 0;
			return; // gobble the key
		}
	} else {
		keydown[key] = 0;
	}

	// handle escape specially, so the user can never unbind it
	if (unicode == '\x1b' || key == QFK_ESCAPE) {
		Key_Console (key, unicode);
		return;
	}

	if (!down && Key_Game (key, unicode))
		return;

	// if not a consolekey, send to the interpreter no matter what mode is
	switch (key_dest) {
		case key_game:
			Key_Game (key, unicode);
			break;
		case key_message:
		case key_menu:
		case key_console:
			Key_Console (key, unicode);
			break;
		default:
			Sys_Error ("Bad key_dest");
	}
}

void
Key_ClearStates (void)
{
	int         i;

	for (i = 0; i < QFK_LAST; i++) {
		if (keydown[i])
			Key_Event (i, 0, false);
		keydown[i] = false;
	}
}

void
Key_Init (cbuf_t *cb)
{
	cbuf = cb;

	OK_Init ();

	// register our functions
	Cmd_AddCommand ("in_bind", Key_In_Bind_f, "Assign a command or a set of "
					"commands to a key.\n"
					"Note: To bind multiple commands to a key, enclose the "
					"commands in quotes and separate with semi-colons.");
	Cmd_AddCommand ("in_unbind", Key_In_Unbind_f,
					"Remove the bind from the the selected key");
	Cmd_AddCommand ("unbindall", Key_Unbindall_f,
					"Remove all binds (USE CAUTIOUSLY!!!)");
	Cmd_AddCommand ("imt", Key_InputMappingTable_f, "");
	Cmd_AddCommand ("bind", Key_Bind_f, "wrapper for in_bind that uses "
					"in_bind_imt for the imt parameter");
	Cmd_AddCommand ("unbind", Key_Unbind_f,
					"wrapper for in_unbind that uses in_bind_imt for the imt "
					"parameter");
	Cmd_AddCommand ("bindlist", Key_Bindlist_f, "list all of the key bindings");
	Cmd_AddCommand ("keyhelp", keyhelp_f, "display the keyname for the next "
					"RECOGNIZED key-press. If the key pressed produces no "
					"output, " PROGRAM " does not recognise that key.");

	GIB_Builtin_Add ("bind::get", Key_GIB_Bind_Get_f);

}

void
Key_Init_Cvars (void)
{
	in_bind_imt = Cvar_Get ("in_bind_imt", "imt_default", CVAR_ARCHIVE,
							in_bind_imt_f, "imt parameter for the bind and "
							"unbind wrappers to in_bind and in_unbind");
}

const char *
Key_GetBinding (imt_t imt, knum_t key)
{
	return keybindings[imt][key].str;
}

VISIBLE void
Key_SetBinding (imt_t target, knum_t keynum, const char *binding)
{
	if (keynum == (knum_t) -1)
		return;

	// free old bindings
	if (keybindings[target][keynum].str) {
		free (keybindings[target][keynum].str);
		keybindings[target][keynum].str = NULL;
	}
	// allocate memory for new binding
	if (binding) {
		keybindings[target][keynum].str = strdup(binding);
	}
}
