/*
	pr_strings.c

	progs string managment

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "QF/hash.h"
#include "QF/progs.h"


static strref_t *
new_string_ref (progs_t *pr)
{
	strref_t *sr;
	if (!pr->free_string_refs) {
		int		i;

		pr->dyn_str_size++;
		pr->dynamic_strings = realloc (pr->dynamic_strings, pr->dyn_str_size);
		if (!pr->dynamic_strings)
			PR_Error (pr, "out of memory");
		if (!(pr->free_string_refs = calloc (1024, sizeof (strref_t))))
			PR_Error (pr, "out of memory");
		pr->dynamic_strings[pr->dyn_str_size - 1] = pr->free_string_refs;
		for (i = 0, sr = pr->free_string_refs; i < 1023; i++, sr++)
			sr->next = sr + 1;
		sr->next = 0;
	}
	sr = pr->free_string_refs;
	pr->free_string_refs = sr->next;
	sr->next = 0;
	return sr;
}

static void
free_string_ref (progs_t *pr, strref_t *sr)
{
	sr->string = 0;
	sr->next = pr->free_string_refs;
	pr->free_string_refs = sr;
}

static int
string_index (progs_t *pr, strref_t *sr)
{
	int		i = sr - pr->static_strings;

	if (i >= 0 && i < pr->num_strings)
		return sr->string - pr->pr_strings;
	for (i = 0; i < pr->dyn_str_size; i++) {
		int d = sr - pr->dynamic_strings[i];
		if (d >= 0 && d < 1024)
			return ~(i * 1024 + d);
	}
	return 0;
}

static const char *
strref_get_key (void *_sr, void *notused)
{
	strref_t	*sr = (strref_t*)_sr;

	return sr->string;
}

static void
strref_free (void *_sr, void *_pr)
{
	progs_t		*pr = (progs_t*)_pr;
	strref_t	*sr = (strref_t*)_sr;

	// free the string and ref only if it's not a static string
	if (sr < pr->static_strings || sr >= pr->static_strings + pr->num_strings) {
		free (sr->string);
		free_string_ref (pr, sr);
	}
}

void
PR_LoadStrings (progs_t *pr)
{
	char   *end = pr->pr_strings + pr->pr_stringsize;
	char   *str = pr->pr_strings;
	int		count = 0;

	while (str < end) {
		count++;
		str += strlen (str) + 1;
	}
	if (pr->strref_hash) {
		Hash_FlushTable (pr->strref_hash);
	} else {
		pr->strref_hash = Hash_NewTable (1021, strref_get_key, strref_free,
										 pr);
		pr->dynamic_strings = 0;
		pr->free_string_refs = 0;
		pr->dyn_str_size = 0;
	}

	if (pr->static_strings)
		free (pr->static_strings);
	pr->static_strings = calloc (count, sizeof (strref_t));
	count = 0;
	str = pr->pr_strings;
	while (str < end) {
		pr->static_strings[count].string = str;
		str += strlen (str) + 1;
		Hash_Add (pr->strref_hash, &pr->static_strings[count]);
		count++;
	}
	pr->num_strings = count;
}

void
PR_GarbageCollect (progs_t *pr)
{
	char	   *str;
	int			i, j;
	ddef_t	   *def;
	strref_t   *sr;

	for (i = 0; i < pr->dyn_str_size; i++)
		for (j = 0; j < 1024; j++)
			pr->dynamic_strings[i][j].count = 0;
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if ((def->type & ~DEF_SAVEGLOBAL) == ev_string) {
			str = G_STRING (pr, def->ofs);
			if (str) {
				sr = Hash_Find (pr->strref_hash, str);
				if (sr)
					sr->count++;
			}
		}
	}
	for (i = 0; i < pr->progs->numfielddefs; i++) {
		def = &pr->pr_fielddefs[i];
		if ((def->type & ~DEF_SAVEGLOBAL) == ev_string) {
			for (j = 0; j < *pr->num_edicts; j++) {
				str = E_STRING (pr, EDICT_NUM (pr, j), def->ofs);
				if (str) {
					sr = Hash_Find (pr->strref_hash, str);
					if (sr)
						sr->count++;
				}
			}
		}
	}
	for (i = 0; i < pr->dyn_str_size; i++) {
		for (j = 0; j < 1024; j++) {
			sr = &pr->dynamic_strings[i][j];
			if (sr->string && !sr->count) {
				Hash_Del (pr->strref_hash, sr->string);
				strref_free (sr, pr);
			}
		}
	}
}

static inline char *
get_string (progs_t *pr, unsigned int num)
{
	if ((int) num < 0) {
		unsigned int row = ~num / 1024;

		num = ~num % 1024;

		if (row >= pr->dyn_str_size)
			return 0;
		return pr->dynamic_strings[row][num].string;
	} else {
		if (num >= pr->pr_stringsize)
			return 0;
		return pr->pr_strings + num;
	}
}

qboolean
PR_StringValid (progs_t *pr, int num)
{
	return get_string (pr, num) != 0;
}

char *
PR_GetString (progs_t *pr, int num)
{
	char       *str;

	str = get_string (pr, num);
	if (str)
		return str;
	PR_RunError (pr, "Invalid string offset %u", num);
}

int
PR_SetString (progs_t *pr, const char *s)
{
	strref_t *sr = Hash_Find (pr->strref_hash, s);

	if (!sr) {
		sr = new_string_ref (pr);
		sr->string = strdup(s);
		sr->count = 0;
		Hash_Add (pr->strref_hash, sr);
	}
	return string_index (pr, sr);
}
