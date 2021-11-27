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
#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/darray.h"
#include "QF/dstring.h"
#include "QF/keys.h"
#include "QF/input.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "old_keys.h"

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
	{ "K_F16",			QFK_F16 },
	{ "K_F17",			QFK_F17 },
	{ "K_F18",			QFK_F18 },
	{ "K_F19",			QFK_F19 },
	{ "K_F20",			QFK_F20 },
	{ "K_F21",			QFK_F21 },
	{ "K_F22",			QFK_F22 },
	{ "K_F23",			QFK_F23 },
	{ "K_F24",			QFK_F24 },
	{ "K_F25",			QFK_F25 },
	{ "K_F26",			QFK_F26 },
	{ "K_F27",			QFK_F27 },
	{ "K_F28",			QFK_F28 },
	{ "K_F29",			QFK_F29 },
	{ "K_F30",			QFK_F30 },
	{ "K_F31",			QFK_F31 },
	{ "K_F32",			QFK_F32 },
	{ "K_F33",			QFK_F33 },
	{ "K_F34",			QFK_F34 },
	{ "K_F35",			QFK_F35 },
	{ "K_F36",			QFK_F36 },
	{ "K_F37",			QFK_F37 },
	{ "K_F38",			QFK_F38 },
	{ "K_F39",			QFK_F39 },
	{ "K_F40",			QFK_F40 },
	{ "K_F41",			QFK_F41 },
	{ "K_F42",			QFK_F42 },
	{ "K_F43",			QFK_F43 },
	{ "K_F44",			QFK_F44 },
	{ "K_F45",			QFK_F45 },
	{ "K_F46",			QFK_F46 },
	{ "K_F47",			QFK_F47 },
	{ "K_F48",			QFK_F48 },
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

	{ "K_KANJI", 		QFK_KANJI },
	{ "K_MUHENKAN", 	QFK_MUHENKAN },
	{ "K_HENKAN", 		QFK_HENKAN },
	{ "K_ROMAJI", 		QFK_ROMAJI },
	{ "K_HIRAGANA", 	QFK_HIRAGANA },
	{ "K_KATAKANA", 	QFK_KATAKANA },
	{ "K_HIRAGANA_KATAKANA",	QFK_HIRAGANA_KATAKANA },
	{ "K_ZENKAKU", 		QFK_ZENKAKU },
	{ "K_HANKAKU", 		QFK_HANKAKU },
	{ "K_ZENKAKU_HANKAKU",	QFK_ZENKAKU_HANKAKU },
	{ "K_TOUROKU", 		QFK_TOUROKU },
	{ "K_MASSYO", 		QFK_MASSYO },
	{ "K_KANA_LOCK", 	QFK_KANA_LOCK },
	{ "K_KANA_SHIFT", 	QFK_KANA_SHIFT },
	{ "K_EISU_SHIFT", 	QFK_EISU_SHIFT },
	{ "K_EISU_TOGGLE", 	QFK_EISU_TOGGLE },
	{ "K_KANJI_BANGOU", QFK_KANJI_BANGOU },
	{ "K_ZEN_KOHO", 	QFK_ZEN_KOHO },
	{ "K_MAE_KOHO", 	QFK_MAE_KOHO },

	{ "K_HOMEPAGE",		QFK_HOMEPAGE },
	{ "K_SEARCH",		QFK_SEARCH },
	{ "K_MAIL",			QFK_MAIL },
	{ "K_FAVORITES",	QFK_FAVORITES },
	{ "K_AUDIOMUTE",	QFK_AUDIOMUTE },
	{ "K_AUDIOLOWERVOLUME",	QFK_AUDIOLOWERVOLUME },
	{ "K_AUDIORAISEVOLUME",	QFK_AUDIORAISEVOLUME },
	{ "K_AUDIOPLAY",		QFK_AUDIOPLAY },
	{ "K_CALCULATOR",	QFK_CALCULATOR },
	{ "K_UNDO",			QFK_UNDO },
	{ "K_REDO",			QFK_REDO },
	{ "K_NEW",			QFK_NEW },
	{ "K_RELOAD",		QFK_RELOAD },
	{ "K_OPEN",			QFK_OPEN },
	{ "K_CLOSE",		QFK_CLOSE },
	{ "K_REPLY",		QFK_REPLY },
	{ "K_MAILFORWARD",	QFK_MAILFORWARD },
	{ "K_SEND",			QFK_SEND },
	{ "K_SAVE",			QFK_SAVE },
	{ "K_BACK",			QFK_BACK },
	{ "K_FORWARD",		QFK_FORWARD },

	{NULL, 0}
};

VISIBLE int
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
