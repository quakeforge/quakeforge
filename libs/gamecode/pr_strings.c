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
#include <stdlib.h>

#include <stdarg.h>

#include "hash.h"
#include "progs.h"

static char *
strref_get_key (void *_sr, void *notused)
{
	strref_t *sr = (strref_t*)_sr;
	return sr->string;
}

static void
strref_free (void *_sr, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	strref_t *sr = (strref_t*)_sr;

	// free the string and ref only if it's not a static string
	if (sr < pr->static_strings || sr >= pr->static_strings + pr->num_strings) {
		free (sr->string);

		sr->next->prev = sr->prev;
		sr->prev->next = sr->next;

		free (sr);
	}
}

void
PR_LoadStrings (progs_t *pr)
{
	int count = 0;
	char *end = pr->pr_strings + pr->pr_stringsize;
	char *str = pr->pr_strings;

	while (str < end) {
		count++;
		str += strlen (str) + 1;
	}
	if (pr->strref_hash) {
		Hash_FlushTable (pr->strref_hash);
	} else {
		pr->strref_hash = Hash_NewTable (1021, strref_get_key, strref_free, pr);
	}

	pr->dynamic_strings[0].next = &pr->dynamic_strings[1];
	pr->dynamic_strings[0].prev = 0;
	pr->dynamic_strings[1].next = 0;
	pr->dynamic_strings[1].prev = &pr->dynamic_strings[1];

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

char *
PR_GetString (progs_t *pr, int num)
{
	return pr->pr_strings + num;
}

int
PR_SetString (progs_t *pr, char *s)
{
	strref_t *sr = Hash_Find (pr->strref_hash, s);

	if (sr) {
		s = sr->string;
	} else {
		sr = malloc (sizeof (strref_t));
		sr->string = strdup(s);
		sr->count = 0;

		sr->next = pr->dynamic_strings[0].next;
		sr->prev = pr->dynamic_strings[0].next->prev;

		pr->dynamic_strings[0].next->prev = sr;
		pr->dynamic_strings[0].next = sr;

		Hash_Add (pr->strref_hash, sr);

		s = sr->string;
	}
	return (int) (s - pr->pr_strings);
}
