/*
	info.c

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <ctype.h>

#include "QF/console.h"
#include "QF/info.h"

/*
	INFO STRINGS
*/

/*
	Info_ValueForKey

	Searches the string for the given
	key and returns the associated value, or an empty string.
*/
char       *
Info_ValueForKey (char *s, char *key)
{
	char        pkey[512];
	static char value[4][512];			// use two buffers so compares

	// work without stomping on each other
	static int  valueindex;
	char       *o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void
Info_RemoveKey (char *s, char *key)
{
	char       *start;
	char        pkey[512];
	char        value[512];
	char       *o;

	if (strstr (key, "\\")) {
		Con_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey)) {
			strcpy (start, s);			// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void
Info_RemovePrefixedKeys (char *start, char prefix)
{
	char       *s;
	char        pkey[512];
	char        value[512];
	char       *o;

	s = start;

	while (1) {
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix) {
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}


void
Info_SetValueForStarKey (char *s, char *key, char *value, size_t maxsize)
{
	char        newstr[1024], *v;
	int         c, is_name, is_team;

	if (strstr (key, "\\") || strstr (value, "\\")) {
		Con_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"")) {
		Con_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen (key) > 63 || strlen (value) > 63) {
		Con_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}
	// this next line is kinda trippy
	if (*(v = Info_ValueForKey (s, key))) {
		// key exists, make sure we have enough room for new value, if we
		// don't,
		// don't change it!
		if (strlen (value) - strlen (v) + strlen (s) > maxsize) {
			Con_Printf ("Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen (value))
		return;

	snprintf (newstr, sizeof (newstr), "\\%s\\%s", key, value);

	if ((int) (strlen (newstr) + strlen (s)) > maxsize) {
		Con_Printf ("Info string length exceeded\n");
		return;
	}
	// only copy ascii values
	s += strlen (s);
	v = newstr;
	is_name = strcaseequal (key, "name");
	is_team = strcaseequal (key, "team");
	while (*v) {
		c = (unsigned char) *v++;
		// client only allows highbits on name
		if (!is_name) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
			// auto lowercase team
			if (is_team)
				c = tolower (c);
		}
		if (c > 13)
			*s++ = c;
	}
	*s = 0;
}

void
Info_SetValueForKey (char *s, char *key, char *value, size_t maxsize)
{
	if (key[0] == '*') {
		Con_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void
Info_Print (char *s)
{
	char        key[512];
	char        value[512];
	char       *o;
	int         l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset (o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Con_Printf ("%s", key);

		if (!*s) {
			Con_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Con_Printf ("%s\n", value);
	}
}
