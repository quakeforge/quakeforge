/*
	info.c

	Info strings.

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

#include <stdio.h>
#include <ctype.h>

#include "QF/hash.h"
#include "QF/info.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

struct info_s {
	struct hashtab_s	*tab;
	size_t				maxsize;
	int					cursize;
};

/*
	Info_FilterForKey

	Searches for key in the "client-needed" info string list
*/
VISIBLE qboolean
Info_FilterForKey (const char *key, const char **filter_list)
{
	const char  **s;

	for (s = filter_list; *s; s++) {
		if (strequal (*s, key)) {
			return true;
		}
	}
	return false;
}

VISIBLE int
Info_CurrentSize (info_t *info)
{
	return info->cursize;
}

/*
	Info_ValueForKey

	Searches the string for the given
	key and returns the associated value, or an empty string.
*/
VISIBLE const char *
Info_ValueForKey (info_t *info, const char *key)
{
	info_key_t *k = Hash_Find (info->tab, key);
	if (k)
		return k->value;
	return "";
}

VISIBLE info_key_t *
Info_Key (info_t *info, const char *key)
{
	return Hash_Find (info->tab, key);
}

VISIBLE info_key_t **
Info_KeyList (info_t *info)
{
	return (info_key_t **) Hash_GetList (info->tab);
}

VISIBLE void
Info_RemoveKey (info_t *info, const char *key)
{
	info_key_t *k = Hash_Del (info->tab, key);

	if (k) {
		free ((char*)k->key);
		free ((char*)k->value);
		free (k);
	}
}

VISIBLE int
Info_SetValueForStarKey (info_t *info, const char *key, const char *value,
						 int flags)
{
	info_key_t *k;
	int         cursize;
	char		*str;
	byte		*d, *s;

	if (strstr (key, "\\") || strstr (value, "\\")) {
		Sys_Printf ("Can't use keys or values with a \\\n");
		return 0;
	}
	if (strstr (key, "\"") || strstr (value, "\"")) {
		Sys_Printf ("Can't use keys or values with a \"\n");
		return 0;
	}
	if (strlen (key) > 63 || strlen (value) > 63) {
		Sys_Printf ("Keys and values must be < 64 characters.\n");
		return 0;
	}

	k = Hash_Find (info->tab, key);
	cursize = info->cursize;
	if (k) {
		cursize -= strlen (k->key) + 1;
		cursize -= strlen (k->value) + 1;
	}
	if (info->maxsize &&
		cursize + strlen (key) + 1 + strlen (value) + 1 > info->maxsize) {
		Sys_Printf ("Info string length exceeded\n");
		return 0;
	}
	if (k) {
		if (strequal (k->value, value))
			return 0;
		info->cursize -= strlen (k->value) + 1;
		free ((char *) k->value);
	} else {
		if (!(k = malloc (sizeof (info_key_t))))
			Sys_Error ("Info_SetValueForStarKey: out of memory");
		if (!(k->key = strdup (key)))
			Sys_Error ("Info_SetValueForStarKey: out of memory");
		info->cursize += strlen (key) + 1;
		Hash_Add (info->tab, k);
	}
	if (!(str = strdup (value)))
		Sys_Error ("Info_SetValueForStarKey: out of memory");
	for (d = s = (byte *) str; *s; s++) {
		if (flags & 1) {
			*s &= 127;
			if (*s < 32)
				continue;
		}
		if (flags & 2)
			*s = tolower (*s);
		if (*s > 13)
			*d++ = *s;
	}
	*d = 0;
	info->cursize += strlen (str) + 1;
	k->value = str;
	return 1;
}

VISIBLE int
Info_SetValueForKey (info_t *info, const char *key, const char *value,
					 int flags)
{
	if (key[0] == '*') {
		Sys_Printf ("Can't set * keys\n");
		return 0;
	}

	return Info_SetValueForStarKey (info, key, value, flags);
}

VISIBLE void
Info_Print (info_t *info)
{
	info_key_t **key_list;
	info_key_t **key;

	key_list = (info_key_t **) Hash_GetList (info->tab);

	for (key = key_list; *key; key++) {
		Sys_Printf ("%-15s %s\n", (*key)->key, (*key)->value);
	}
	free (key_list);
}

static const char *
info_get_key (const void *k, void *unused)
{
	return ((info_key_t *)k)->key;
}

static void
free_key (void *_k, void *unused)
{
	info_key_t *k = (info_key_t *) _k;
	free ((char *) k->key);
	free ((char *) k->value);
	free (k);
}

VISIBLE info_t *
Info_ParseString (const char *s, int maxsize, int flags)
{
	info_t     *info;
	char       *string = Hunk_TempAlloc (strlen (s) + 1);
	char       *key, *value, *end;

	info = malloc (sizeof (info_t));
	info->tab = Hash_NewTable (61, info_get_key, free_key, 0, 0);
	info->maxsize = maxsize;
	info->cursize = 0;

	strcpy (string, s);
	key = string;
	if (*key == '\\')
		key++;
	while (*key) {
		value = key;
		while (*value && *value != '\\')
			value++;
		if (*value) {
			*value++ = 0;
			for (end = value; *end && *end != '\\'; end++)
				;
			if (*end)
				*end++ = 0;
		} else {
			value = end = (char *) "";
		}
		Info_SetValueForStarKey (info, key, value, flags);
		key = end;
	}
	return info;
}

VISIBLE void
Info_Destroy (info_t *info)
{
	Hash_DelTable (info->tab);
	free (info);
}

VISIBLE char *
Info_MakeString (info_t *info, int (*filter) (const char *))
{
	char       *string;
	const char *s;
	char       *d;
	info_key_t **key_list;
	info_key_t **key;

	d = string = Hunk_TempAlloc (info->cursize + 1);
	key_list = (info_key_t **) Hash_GetList (info->tab);

	for (key = key_list; *key; key++) {
		if (!*(*key)->value)
			continue;
		if (filter && filter ((*key)->key))
			continue;
		*d++ = '\\';
		for (s = (*key)->key; *s; s++)
			*d++ = *s;
		*d++ = '\\';
		for (s = (*key)->value; *s; s++)
			*d++ = *s;
	}
	*d = 0;
	free (key_list);
	return string;
}

VISIBLE void
Info_AddKeys (info_t *info, info_t *keys)
{
	info_key_t **key_list;
	info_key_t **key;

	key_list = (info_key_t **)Hash_GetList (keys->tab);
	for (key = key_list; *key; key++) {
		Info_SetValueForKey (info, (*key)->key, (*key)->value, 0);
	}
	free (key_list);
}
