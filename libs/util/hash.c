/*
	hash.h

	hash tables

	Copyright (C) 2000  Bill Currie <bill@taniwha.org>

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

#include <stdlib.h>		// should be sys/types.h, but bc is stupid

#include "QF/hash.h"

#include "compat.h"

struct hashlink_s {
	struct hashlink_s *next;
	struct hashlink_s **prev;
	void *data;
};

struct hashtab_s {
	size_t tab_size;
	void *user_data;
	int (*compare)(void*,void*,void*);
	unsigned long (*get_hash)(void*,void*);
	const char *(*get_key)(void*,void*);
	void (*free_ele)(void*,void*);
	struct hashlink_s *tab[1];             // variable size
};


unsigned long
Hash_String (const char *str)
{
	//FIXME not 64 bit clean
#if 0
	unsigned long h = 0;
	while (*str) {
		h = (h << 4) + (unsigned char)*str++;
		if (h&0xf0000000)
			h = (h ^ (h >> 24)) & 0xfffffff;
	}
	return h;
#else
	// dx_hack_hash 
	// shamelessly stolen from Daniel Phillips <phillips@innominate.de>
	// from his post to lkml
	unsigned long hash0 = 0x12a3fe2d, hash1 = 0x37abe8f9;
	while (*str)
	{
		unsigned long hash = hash1 + (hash0 ^ (*str++ * 71523));
		if (hash < 0) hash -= 0x7fffffff;
		hash1 = hash0;
		hash0 = hash;
	}
	return hash0;
#endif
}

static unsigned long
get_hash (void *ele, void *data)
{
	return (unsigned long)ele;
}

static int
compare (void *a, void *b, void *data)
{
	return a == b;
}

hashtab_t *
Hash_NewTable (int tsize, const char *(*gk)(void*,void*),
			   void (*f)(void*,void*), void *ud)
{
	hashtab_t *tab = calloc (1, field_offset (hashtab_t, tab[tsize]));
	if (!tab)
		return 0;
	tab->tab_size = tsize;
	tab->user_data = ud;
	tab->get_key = gk;
	tab->free_ele = f;

	tab->get_hash = get_hash;
	tab->compare = compare;
	return tab;
}

void
Hash_SetHashCompare (hashtab_t *tab, unsigned long (*gh)(void*,void*),
					 int (*cmp)(void*,void*,void*))
{
	tab->get_hash = gh;
	tab->compare = cmp;
}

void
Hash_DelTable (hashtab_t *tab)
{
	Hash_FlushTable (tab);
	free (tab);
}

void
Hash_FlushTable (hashtab_t *tab)
{
	int i;

	for (i = 0; i < tab->tab_size; i++) {
		while (tab->tab[i]) {
			struct hashlink_s *t = tab->tab[i]->next;
			void *data = tab->tab[i]->data;

			free (tab->tab[i]);
			tab->tab[i] = t;
			if (tab->free_ele)
				tab->free_ele (data, tab->user_data);
		}
	}
}

int
Hash_Add (hashtab_t *tab, void *ele)
{
	unsigned long h = Hash_String (tab->get_key(ele, tab->user_data));
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = malloc (sizeof (struct hashlink_s));

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

int
Hash_AddElement (hashtab_t *tab, void *ele)
{
	unsigned long h = tab->get_hash (ele, tab->user_data);
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = malloc (sizeof (struct hashlink_s));

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
	unsigned long h = Hash_String (key);
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = tab->tab[ind];

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data)))
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

void *
Hash_FindElement (hashtab_t *tab, void *ele)
{
	unsigned long h = tab->get_hash (ele, tab->user_data);
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = tab->tab[ind];

	while (lnk) {
		if (tab->compare (lnk->data, ele, tab->user_data))
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

void *
Hash_Del (hashtab_t *tab, const char *key)
{
	unsigned long h = Hash_String (key);
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = tab->tab[ind];
	void *data;

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data))) {
			data = lnk->data;
			if (lnk->next)
				lnk->next->prev = lnk->prev;
			*lnk->prev = lnk->next;
			free (lnk);
			return data;
		}
		lnk = lnk->next;
	}
	return 0;
}

void *
Hash_DelElement (hashtab_t *tab, void *ele)
{
	unsigned long h = tab->get_hash (ele, tab->user_data);
	size_t ind = h % tab->tab_size;
	struct hashlink_s *lnk = tab->tab[ind];
	void *data;

	while (lnk) {
		if (tab->compare (lnk->data, ele, tab->user_data)) {
			data = lnk->data;
			if (lnk->next)
				lnk->next->prev = lnk->prev;
			*lnk->prev = lnk->next;
			free (lnk);
			return data;
		}
		lnk = lnk->next;
	}
	return 0;
}
