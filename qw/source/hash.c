/*
	hash.h

	hash tables

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000  Marcus Sundberg <mackan@stacken.kth.se>

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

#include "compat.h"
#include "hash.h"

static unsigned long
hash (const char *str)
{
	unsigned long h = 0;
	while (*str) {
		h = (h << 4) + (unsigned char)*str++;
		if (h&0xf0000000)
			h = (h ^ (h >> 24)) & 0xfffffff;
	}
	return h;
}

hashtab_t *
Hash_NewTable (int tsize, char *(*gk)(void*), void (*f)(void*))
{
	hashtab_t *tab = calloc (1, (size_t)&((hashtab_t*)0)->tab[tsize]);
	if (!tab)
		return 0;
	tab->tab_size = tsize;
	tab->get_key = gk;
	tab->free_ele = f;
	return tab;
}

void
Hash_DelTable (hashtab_t *tab)
{
	int i;

	for (i = 0; i < tab->tab_size; i++) {
		while (tab->tab[i]) {
			hashlink_t *t = tab->tab[i]->next;
			if (tab->free_ele)
				tab->free_ele (&tab->tab[i]->data);
			free (tab->tab[i]);
			tab->tab[i] = t;
		}
	}
	free (tab);
}

int
Hash_Add (hashtab_t *tab, void *ele)
{
	unsigned long h = hash (tab->get_key(ele));
	size_t ind = h % tab->tab_size;
	hashlink_t *lnk = malloc (sizeof (hashlink_t));

	if (!lnk)
		return -1;
	if (tab->tab[ind])
		tab->tab[ind]->prev = &lnk->next;
	lnk->next = tab->tab[ind];
	lnk->prev = &tab->tab[ind];
	lnk->data = ele;
	tab->tab[ind] = lnk;
	return 0;
}

void *
Hash_Find (hashtab_t *tab, const char *key)
{
	unsigned long h = hash (key);
	size_t ind = h % tab->tab_size;
	hashlink_t *lnk = tab->tab[ind];

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data)))
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

int
Hash_Del (hashtab_t *tab, const char *key)
{
	unsigned long h = hash (key);
	size_t ind = h % tab->tab_size;
	hashlink_t *lnk = tab->tab[ind];

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data))) {
			if (lnk->next)
				lnk->next->prev = lnk->prev;
			*lnk->prev = lnk->next;
			free (lnk);
			return 0;
		}
		lnk = lnk->next;
	}
	return -1;
}
