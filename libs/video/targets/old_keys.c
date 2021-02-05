/*
	old_keys.c

	translations from old to new keynames

	Copyright (C) 2001 Bill Currie <bill@tanwiha.org>

	Author: Bill Currie <bill@tanwiha.org>
	Date: 2001/8/16

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

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <ctype.h>
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/hash.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "old_keys.h"

typedef struct {
	const char *old_name;
	const char *new_name;
} old_keyname_t;

old_keyname_t   old_keynames[] = {
	{"TAB",				"K_TAB"},
	{"ENTER",			"K_RETURN"},
	{"ESCAPE",			"K_ESCAPE"},
	{"SPACE",			"K_SPACE"},
	{"BACKSPACE",		"K_BACKSPACE"},

	{"CAPSLOCK",		"K_CAPSLOCK"},
	{"PRINTSCR",		"K_PRINT"},
	{"SCRLCK",			"K_SCROLLOCK"},
	{"PAUSE",			"K_PAUSE"},

	{"UPARROW",			"K_UP"},
	{"DOWNARROW",		"K_DOWN"},
	{"LEFTARROW",		"K_LEFT"},
	{"RIGHTARROW",		"K_RIGHT"},

	{"ALT",				"K_LALT"},
	{"CTRL",			"K_LCTRL"},
	{"SHIFT",			"K_LSHIFT"},

	{"NUMLOCK",			"K_NUMLOCK"},
	{"KP_NUMLCK",		"K_NUMLOCK"},
	{"KP_NUMLOCK",		"K_NUMLOCK"},
	{"KP_SLASH",		"K_KP_DIVIDE"},
	{"KP_DIVIDE",		"K_KP_DIVIDE"},
	{"KP_STAR",			"K_KP_MULTIPLY"},
	{"KP_MULTIPLY",		"K_KP_MULTIPLY"},
	{"KP_MINUS",		"K_KP_MINUS"},

	{"KP_HOME",			"K_KP7"},
	{"KP_UPARROW",		"K_KP8"},
	{"KP_PGUP",			"K_KP9"},
	{"KP_PLUS",			"K_KP_PLUS"},

	{"KP_LEFTARROW",	"K_KP4"},
	{"KP_5",			"K_KP5"},
	{"KP_RIGHTARROW",	"K_KP6"},

	{"KP_END",			"K_KP1"},
	{"KP_DOWNARROW",	"K_KP2"},
	{"KP_PGDN",			"K_KP3"},

	{"KP_INS",			"K_KP0"},
	{"KP_DEL",			"K_KP_PERIOD"},
	{"KP_ENTER",		"K_KP_ENTER"},

	{"F1",				"K_F1"},
	{"F2",				"K_F2"},
	{"F3",				"K_F3"},
	{"F4",				"K_F4"},
	{"F5",				"K_F5"},
	{"F6",				"K_F6"},
	{"F7",				"K_F7"},
	{"F8",				"K_F8"},
	{"F9",				"K_F9"},
	{"F10",				"K_F10"},
	{"F11",				"K_F11"},
	{"F12",				"K_F12"},

	{"INS",				"K_INSERT"},
	{"DEL",				"K_DELETE"},
	{"PGDN",			"K_PAGEDOWN"},
	{"PGUP",			"K_PAGEUP"},
	{"HOME",			"K_HOME"},
	{"END",				"K_END"},

	{"MOUSE1",			"M_BUTTON1"},
	{"MOUSE2",			"M_BUTTON2"},
	{"MOUSE3",			"M_BUTTON3"},
	{"MOUSE6",			"M_BUTTON6"},
	{"MOUSE7",			"M_BUTTON7"},

	{"JOY1",			"J_BUTTON1"},
	{"JOY2",			"J_BUTTON2"},
	{"JOY3",			"J_BUTTON3"},
	{"JOY4",			"J_BUTTON4"},

	{"MWHEELUP",		"M_WHEEL_UP"},
	{"MWHEELDOWN",		"M_WHEEL_DOWN"},

	{"ASC178",			"K_WORLD_18"},
	{"ASC233",			"K_WORLD_73"},
	{"ASC167",			"K_WORLD_7"},
	{"ASC232",			"K_WORLD_72"},
	{"ASC231",			"K_WORLD_71"},
	{"ASC224",			"K_WORLD_64"},

	{"0",				"K_0"},
	{"1",				"K_1"},
	{"2",				"K_2"},
	{"3",				"K_3"},
	{"4",				"K_4"},
	{"5",				"K_5"},
	{"6",				"K_6"},
	{"7",				"K_7"},
	{"8",				"K_8"},
	{"9",				"K_9"},

	{"A",				"K_a"},
	{"B",				"K_b"},
	{"C",				"K_c"},
	{"D",				"K_d"},
	{"E",				"K_e"},
	{"F",				"K_f"},
	{"G",				"K_g"},
	{"H",				"K_h"},
	{"I",				"K_i"},
	{"J",				"K_j"},
	{"K",				"K_k"},
	{"L",				"K_l"},
	{"M",				"K_m"},
	{"N",				"K_n"},
	{"O",				"K_o"},
	{"P",				"K_p"},
	{"Q",				"K_q"},
	{"R",				"K_r"},
	{"S",				"K_s"},
	{"T",				"K_t"},
	{"U",				"K_u"},
	{"V",				"K_v"},
	{"W",				"K_w"},
	{"X",				"K_x"},
	{"Y",				"K_y"},
	{"Z",				"K_z"},

	{" ",				"K_SPACE"},
	{"!",				"K_EXCLAIM"},
	{"DOUBLEQUOTE",		"K_QUOTEDBL"},
	{"#",				"K_HASH"},
	{"$",				"K_DOLLAR"},
	{"&",				"K_AMPERSAND"},
	{"'",				"K_QUOTE"},
	{"(",				"K_LEFTPAREN"},
	{")",				"K_RIGHTPAREN"},
	{"*",				"K_ASTERISK"},
	{"+",				"K_PLUS"},
	{",",				"K_COMMA"},
	{"-",				"K_MINUS"},
	{".",				"K_PERIOD"},
	{"/",				"K_SLASH"},

	{":",				"K_COLON"},
	{"SEMICOLON",		"K_SEMICOLON"},
	{"<",				"K_LESS"},
	{"=",				"K_EQUALS"},
	{">",				"K_GREATER"},
	{"?",				"K_QUESTION"},
	{"@",				"K_AT"},

	{"[",				"K_LEFTBRACKET"},
	{"\\",				"K_BACKSLASH"},
	{"]",				"K_RIGHTBRACKET"},
	{"^",				"K_CARET"},
	{"_",				"K_UNDERSCORE"},
	{"`",				"K_BACKQUOTE"},
	{"~",				"K_BACKQUOTE"},

	{0,	0}
};

hashtab_t  *old_key_table;

static const char *
ok_get_key (const void *_ok, void *unused)
{
	old_keyname_t *ok = (old_keyname_t *)_ok;
	return ok->old_name;
}

void
OK_Init (void)
{
	old_keyname_t *ok;

	old_key_table = Hash_NewTable (1021, ok_get_key, 0, 0, 0);
	for (ok = old_keynames; ok->old_name; ok++)
		Hash_Add (old_key_table, ok);
}

const char *
OK_TranslateKeyName (const char *name)
{
	old_keyname_t *ok;
	char       *uname = alloca (strlen (name) + 1);
	const char *s = name;
	char       *d = uname;

	while ((*d++ = toupper ((byte) *s)))
		s++;
	ok = Hash_Find (old_key_table, uname);
	if (!ok) {
		Sys_Printf ("unknown old keyname: %s\n", uname);
		return name;
	}
	return ok->new_name;
}
