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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/progs.h"

typedef struct strref_s {
	struct strref_s *next;
	char       *string;
	dstring_t  *dstring;
	int         count;
} strref_t;

static void *
pr_strings_alloc (void *_pr, size_t size)
{
	progs_t    *pr = (progs_t *) _pr;
	return PR_Zone_Malloc (pr, size);
}

static void
pr_strings_free (void *_pr, void *ptr)
{
	progs_t    *pr = (progs_t *) _pr;
	PR_Zone_Free (pr, ptr);
}

static void *
pr_strings_realloc (void *_pr, void *ptr, size_t size)
{
	progs_t    *pr = (progs_t *) _pr;
	return PR_Zone_Realloc (pr, ptr, size);
}

static strref_t *
new_string_ref (progs_t *pr)
{
	strref_t *sr;
	if (!pr->free_string_refs) {
		int		i, size;

		pr->dyn_str_size++;
		size = pr->dyn_str_size * sizeof (strref_t *);
		pr->dynamic_strings = realloc (pr->dynamic_strings, size);
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
	sr->dstring = 0;
	sr->next = pr->free_string_refs;
	pr->free_string_refs = sr;
}

static int
string_index (progs_t *pr, strref_t *sr)
{
	long        o = (long) (sr - pr->static_strings);
	unsigned int i;

	if (o >= 0 && o < pr->num_strings)
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
		PR_Zone_Free (pr, sr->string);
		free_string_ref (pr, sr);
	}
}

int
PR_LoadStrings (progs_t *pr)
{
	char   *end = pr->pr_strings + pr->pr_stringsize;
	char   *str = pr->pr_strings;
	int		count = 0;

	while (str < end) {
		count++;
		str += strlen (str) + 1;
	}

	if (!pr->ds_mem) {
		pr->ds_mem = malloc (sizeof (dstring_mem_t));
		pr->ds_mem->alloc = pr_strings_alloc;
		pr->ds_mem->free = pr_strings_free;
		pr->ds_mem->realloc = pr_strings_realloc;
		pr->ds_mem->data = pr;
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
	pr->static_strings = malloc (count * sizeof (strref_t));
	count = 0;
	str = pr->pr_strings;
	while (str < end) {
		pr->static_strings[count].string = str;
		str += strlen (str) + 1;
		Hash_Add (pr->strref_hash, &pr->static_strings[count]);
		count++;
	}
	pr->num_strings = count;
	return 1;
}

void
PR_GarbageCollect (progs_t *pr)
{
	const char *str;
	unsigned int i;
	int         j;
	ddef_t     *def;
	strref_t   *sr;

	for (i = 0; i < pr->dyn_str_size; i++)
		for (j = 0; j < 1024; j++)
			pr->dynamic_strings[i][j].count = 0;
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if ((def->type & ~DEF_SAVEGLOBAL) == ev_string) {
			str = G_GSTRING (pr, def->ofs);
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
				str = E_GSTRING (pr, EDICT_NUM (pr, j), def->ofs);
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

static inline strref_t *
get_strref (progs_t *pr, int num)
{
	if (num < 0) {
		unsigned int row = ~num / 1024;

		num = ~num % 1024;

		if (row >= pr->dyn_str_size)
			return 0;
		return &pr->dynamic_strings[row][num];
	} else {
		return 0;
	}
}

static inline const char *
get_string (progs_t *pr, int num)
{
	if (num < 0) {
		strref_t   *ref = get_strref (pr, num);
		if (!ref)
			return 0;
		if (ref->dstring)
			return ref->dstring->str;
		return ref->string;
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

const char *
PR_GetString (progs_t *pr, int num)
{
	const char *str;

	str = get_string (pr, num);
	if (str)
		return str;
	PR_RunError (pr, "Invalid string offset %d", num);
}

dstring_t *
PR_GetDString (progs_t *pr, int num)
{
	strref_t   *ref = get_strref (pr, num);
	if (ref) {
		if (ref->dstring)
			return ref->dstring;
		PR_RunError (pr, "not a dstring: %d", num);
	}
	PR_RunError (pr, "Invalid string offset: %d", num);
}

static inline char *
pr_strdup (progs_t *pr, const char *s)
{
	size_t      len = strlen (s) + 1;
	char       *new = PR_Zone_Malloc (pr, len);
	strcpy (new, s);
	return new;
}

int
PR_SetString (progs_t *pr, const char *s)
{
	strref_t   *sr = Hash_Find (pr->strref_hash, s);

	if (!sr) {
		sr = new_string_ref (pr);
		sr->string = pr_strdup(pr, s);
		sr->count = 0;
		Hash_Add (pr->strref_hash, sr);
	}
	return string_index (pr, sr);
}

int
PR_SetTempString (progs_t *pr, const char *s)
{
	strref_t   *sr;

	sr = new_string_ref (pr);
	sr->string = pr_strdup(pr, s);
	sr->count = 0;
	sr->next = pr->pr_xtstr;
	pr->pr_xtstr = sr;
	return string_index (pr, sr);
}

int
PR_NewString (progs_t *pr)
{
	strref_t   *sr = new_string_ref (pr);
	sr->dstring = _dstring_newstr (pr->ds_mem);
	return string_index (pr, sr);
}

void
PR_FreeString (progs_t *pr, int str)
{
	strref_t   *sr = get_strref (pr, str);

	if (sr) {
		if (sr->dstring)
			dstring_delete (sr->dstring);
		else
			PR_Zone_Free (pr, sr->string);
		free_string_ref (pr, sr);
		return;
	}
	PR_RunError (pr, "attempt to free invalid string %d", str);
}

void
PR_FreeTempStrings (progs_t *pr)
{
	strref_t   *sr, *t;

	for (sr = pr->pr_xtstr; sr; sr = t) {
		t = sr->next;
		PR_Zone_Free (pr, sr->string);
		free_string_ref (pr, sr);
	}
	pr->pr_xtstr = 0;
}
