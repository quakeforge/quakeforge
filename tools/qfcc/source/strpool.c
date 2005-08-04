/*
	strpool.c

	unique string support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/5

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
#include <stdlib.h>

#include "QF/hash.h"

#include "strpool.h"

static const char *
strpool_get_key (void *_str, void *_strpool)
{
	long        str = (long) _str;
	strpool_t  *strpool = (strpool_t *) _strpool;

	return strpool->strings + str;
}

strpool_t *
strpool_new (void)
{
	strpool_t  *strpool = malloc (sizeof (strpool_t));

	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool);
	strpool->size = 1;
	strpool->max_size = 16384;
	strpool->strings = malloc (strpool->max_size);
	strpool->strings[0] = 0;
	return strpool;
}

strpool_t *
strpool_build (const char *strings, int size)
{
	long        s;

	strpool_t  *strpool = malloc (sizeof (strpool_t));
	strpool->str_tab = Hash_NewTable (16381, strpool_get_key, 0, strpool);
	strpool->size = size + (*strings != 0);
	strpool->max_size = (strpool->size + 16383) & ~16383;
	strpool->strings = malloc (strpool->max_size);
	memcpy (strpool->strings + (*strings != 0), strings, strpool->size);
	strpool->strings[0] = 0;
	for (s = 1; s < strpool->size; s += strlen (strpool->strings + s) + 1) {
		Hash_Add (strpool->str_tab, (void *) s);
	}
	return strpool;
}

void
strpool_delete (strpool_t *strpool)
{
	Hash_DelTable (strpool->str_tab);
	free (strpool->strings);
	free (strpool);
}

int
strpool_addstr (strpool_t *strpool, const char *str)
{
	long        s;
	int         len;

	if (!str || !*str)
		return 0;
	s = (long) Hash_Find (strpool->str_tab, str);
	if (s)
		return s;
	len = strlen (str) + 1;
	if (strpool->size + len > strpool->max_size) {
		strpool->max_size += (len + 16383) & ~16383;
		strpool->strings = realloc (strpool->strings, strpool->max_size);
	}
	s = strpool->size;
	strpool->size += len;
	strcpy (strpool->strings + s, str);
	Hash_Add (strpool->str_tab, (void *) s);
	return s;
}
