/*
	util.c

	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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

#include "QF/hash.h"

#include "util.h"

static const char *
strset_get_key (const void *_str, void *unused)
{
	return (const char *)_str;
}

strset_t *
new_strset (const char * const *strings)
{
	hashtab_t  *tab = Hash_NewTable (61, strset_get_key, 0, 0, 0);//FIXME threads
	for ( ; strings && *strings; strings++) {
		Hash_Add (tab, (void *) *strings);
	}
	return (strset_t *) tab;
}

void
del_strset (strset_t *strset)
{
	Hash_DelTable ((hashtab_t *) strset);
}

void
strset_add (strset_t *strset, const char *str)
{
	hashtab_t  *tab = (hashtab_t *) strset;
	Hash_Add (tab, (void *) str);
}

int
strset_contains (strset_t *strset, const char *str)
{
	return Hash_Find ((hashtab_t *) strset, str) != 0;
}

int
count_strings (const char * const *str)
{
	int         count = 0;

	if (str) {
		while (*str++) {
			count++;
		}
	}
	return count;
}

void
merge_strings (const char **out, const char * const *in1,
			   const char * const *in2)
{
	if (in1) {
		while (*in1) {
			*out++ = *in1++;
		}
	}
	if (in2) {
		while (*in2) {
			*out++ = *in2++;
		}
	}
}

void
prune_strings (strset_t *strset, const char **strings, uint32_t *count)
{
	hashtab_t  *tab = (hashtab_t *) strset;

	for (uint32_t i = *count; i-- > 0; ) {
		if (!Hash_Find (tab, strings[i])) {
			memmove (strings + i, strings + i + 1,
					 (--(*count) - i) * sizeof (const char **));
		}
	}
}
