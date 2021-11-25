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
	{"ESCAPE",			"key button 0x01"},
	{"1",				"key button 0x02"},
	{"2",				"key button 0x03"},
	{"3",				"key button 0x04"},
	{"4",				"key button 0x05"},
	{"5",				"key button 0x06"},
	{"6",				"key button 0x07"},
	{"7",				"key button 0x08"},
	{"8",				"key button 0x09"},
	{"9",				"key button 0x0a"},
	{"0",				"key button 0x0b"},
	{"-",				"key button 0x0c"},
	{"=",				"key button 0x0d"},
	{"BACKSPACE",		"key button 0x0e"},
	{"TAB",				"key button 0x0f"},

	{"Q",				"key button 0x10"},
	{"W",				"key button 0x11"},
	{"E",				"key button 0x12"},
	{"R",				"key button 0x13"},
	{"T",				"key button 0x14"},
	{"Y",				"key button 0x15"},
	{"U",				"key button 0x16"},
	{"I",				"key button 0x17"},
	{"O",				"key button 0x18"},
	{"P",				"key button 0x19"},
	{"[",				"key button 0x1a"},
	{"]",				"key button 0x1b"},
	{"ENTER",			"key button 0x1c"},
	{"CTRL",			"key button 0x1d"},
	{"A",				"key button 0x1e"},
	{"S",				"key button 0x1f"},

	{"D",				"key button 0x20"},
	{"F",				"key button 0x21"},
	{"G",				"key button 0x22"},
	{"H",				"key button 0x23"},
	{"J",				"key button 0x24"},
	{"K",				"key button 0x25"},
	{"L",				"key button 0x26"},
	{"SEMICOLON",		"key button 0x27"},
	{"'",				"key button 0x28"},
	{"`",				"key button 0x29"},
	{"SHIFT",			"key button 0x2a"},
	{"\\",				"key button 0x2b"},
	{"Z",				"key button 0x2c"},
	{"X",				"key button 0x2d"},
	{"C",				"key button 0x2e"},
	{"V",				"key button 0x2f"},

	{"B",				"key button 0x30"},
	{"N",				"key button 0x31"},
	{"M",				"key button 0x32"},
	{",",				"key button 0x33"},
	{".",				"key button 0x34"},
	{"/",				"key button 0x35"},
	// quake had no right shift
	{"*",				"key button 0x37"},
	{"ALT",				"key button 0x38"},
	{"SPACE",			"key button 0x39"},
	//{"CAPSLOCK",		"key button 0x3a"},	quake had no way of binding capslock
	{"F1",				"key button 0x3b"},
	{"F2",				"key button 0x3c"},
	{"F3",				"key button 0x3d"},
	{"F4",				"key button 0x3e"},
	{"F5",				"key button 0x3f"},

	{"F6",				"key button 0x40"},
	{"F7",				"key button 0x41"},
	{"F8",				"key button 0x42"},
	{"F9",				"key button 0x43"},
	{"F10",				"key button 0x44"},
	{"PAUSE",			"key button 0x45"},
	{"SCRLCK",			"key button 0x46"},
	{"HOME",			"key button 0x47"},
	{"UPARROW",			"key button 0x48"},
	{"PGUP",			"key button 0x49"},
	//{"-",				"key button 0x4a"},	quake had no way of binding kp -
	{"LEFTARROW",		"key button 0x4b"},
	//{"5",				"key button 0x4c"},	quake had no way of binding kp 5
	{"RIGHTARROW",		"key button 0x4d"},
	{"+",				"key button 0x4e"},
	{"END",				"key button 0x4f"},
	{"DOWNARROW",		"key button 0x50"},
	{"PGDN",			"key button 0x51"},
	{"INS",				"key button 0x52"},
	{"DEL",				"key button 0x53"},
	// nothing 0x54
	// nothing 0x55
	// nothing 0x56
	{"F11",				"key button 0x57"},
	{"F12",				"key button 0x58"},

	{";",				"key button 0x27"},
	{":",				"key button 0x27"},

	{"MOUSE1",			"mouse button 0"},
	{"MOUSE2",			"mouse button 2"},
	{"MOUSE3",			"mouse button 1"},
	{"MOUSE6",			"mouse button 5"},
	{"MOUSE7",			"mouse button 6"},

	{"MWHEELUP",		"mouse button 3"},
	{"MWHEELDOWN",		"mouse button 4"},

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
		return 0;
	}
	return ok->new_name;
}
