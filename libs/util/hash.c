/*
	hash.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <stdlib.h>		// should be sys/types.h, but bc is stupid

#include "QF/hash.h"
#include "QF/qtypes.h"
#include "QF/mathlib.h"
#include "QF/sys.h"

#include "compat.h"

typedef struct hashlink_s {
	struct hashlink_s *next;
	struct hashlink_s **prev;
	void *data;
} hashlink_t;

typedef struct hlinkset_s {
	struct hlinkset_s *next;
} hlinkset_t;

struct hashctx_s {
	hlinkset_t *linksets;
	hashlink_t *free_hashlinks;
};

struct hashtab_s {
	size_t tab_size;
	unsigned int size_bits;
	size_t num_ele;
	void *user_data;
	int (*compare)(const void*,const void*,void*);
	uintptr_t (*get_hash)(const void*,void*);
	const char *(*get_key)(const void*,void*);
	void (*free_ele)(void*,void*);
	hashctx_t  *hashctx;
	hashlink_t *tab[1];             // variable size
};

static hashlink_t *
new_hashlink (hashctx_t *hctx)
{
	hashlink_t *link;

	if (!hctx->free_hashlinks) {
		int		i;

		hlinkset_t *linkset = malloc (sizeof (hlinkset_t)
									  + 1024 * sizeof (hashlink_t));
		linkset->next = hctx->linksets;
		hctx->linksets = linkset;
		hctx->free_hashlinks = (hashlink_t *) &linkset[1];
		for (i = 0, link = hctx->free_hashlinks; i < 1023; i++, link++)
			link->next = link + 1;
		link->next = 0;
	}
	link = hctx->free_hashlinks;
	hctx->free_hashlinks = link->next;
	link->next = 0;
	return link;
}

static void
free_hashlink (hashlink_t *link, hashctx_t *hctx)
{
	link->next = hctx->free_hashlinks;
	hctx->free_hashlinks = link;
}

static hashctx_t *default_hashctx;

VISIBLE uintptr_t
Hash_String (const char *str)
{
#if 0
	uintptr_t   h = 0;
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
	uint32_t hash0 = 0x12a3fe2d, hash1 = 0x37abe8f9;
	while (*str) {
		uint32_t hash = hash1 + (hash0 ^ ((unsigned char)*str++ * 71523));
		if (hash & 0x80000000) hash -= 0x7fffffff;
		hash1 = hash0;
		hash0 = hash;
	}
	return hash0;
#endif
}

VISIBLE uintptr_t
Hash_nString (const char *str, size_t sz)
{
#if 0
	uintptr_t   h = 0;
	while (*str && sz-- > 0) {
		h = (h << 4) + (unsigned char)*str++;
		if (h&0xf0000000)
			h = (h ^ (h >> 24)) & 0xfffffff;
	}
	return h;
#else
	// dx_hack_hash
	// shamelessly stolen from Daniel Phillips <phillips@innominate.de>
	// from his post to lkml
	uint32_t hash0 = 0x12a3fe2d, hash1 = 0x37abe8f9;
	while (sz-- > 0 && *str) {
		uint32_t hash = hash1 + (hash0 ^ ((unsigned char)*str++ * 71523));
		if (hash & 0x80000000) hash -= 0x7fffffff;
		hash1 = hash0;
		hash0 = hash;
	}
	return hash0;
#endif
}

VISIBLE uintptr_t
Hash_Buffer (const void *_buf, int len)
{
	const unsigned char *buf = _buf;
#if 0
	uintptr_t   h = 0;
	while (len-- > 0) {
		h = (h << 4) + (unsigned char)*buf++;
		if (h&0xf0000000)
			h = (h ^ (h >> 24)) & 0xfffffff;
	}
	return h;
#else
	// dx_hack_hash
	// shamelessly stolen from Daniel Phillips <phillips@innominate.de>
	// from his post to lkml
	uint32_t hash0 = 0x12a3fe2d, hash1 = 0x37abe8f9;
	while (len-- > 0) {
		uint32_t hash = hash1 + (hash0 ^ ((unsigned char)*buf++ * 71523));
		if (hash & 0x80000000) hash -= 0x7fffffff;
		hash1 = hash0;
		hash0 = hash;
	}
	return hash0;
#endif
}

static uintptr_t
get_hash (const void *ele, void *data)
{
	return (uintptr_t)ele;
}

static int
compare (const void *a, const void *b, void *data)
{
	return a == b;
}

static inline int
get_index (uintptr_t hash, size_t size, size_t bits)
{
#if 0
	uintptr_t   mask = ~0UL << bits;
	uintptr_t   extract;

	size -= 1;
	for (extract = (hash & mask) >> bits;
		 extract;
		 extract = (hash & mask) >> bits) {
		hash &= ~mask;
		hash ^= extract;
	} while (extract);
	if (hash > size) {
		extract = hash - size;
		hash = size - (extract >> 1);
	}
	return hash;
#else
	return (int)(hash % size);
#endif
}

static void
hash_shutdown (void *data)
{
	Hash_DelContext (default_hashctx);
}

VISIBLE void
Hash_DelContext (hashctx_t *hashctx)
{
	if (!hashctx) {
		return;
	}
	while (hashctx->linksets) {
		hlinkset_t *l = hashctx->linksets->next;
		free (hashctx->linksets);
		hashctx->linksets = l;
	}
	free (hashctx);
}

VISIBLE hashtab_t *
Hash_NewTable (int tsize, const char *(*gk)(const void*,void*),
			   void (*f)(void*,void*), void *ud, hashctx_t **hctx)
{
	hashtab_t *tab = calloc (1, field_offset (hashtab_t, tab[tsize]));
	if (!tab)
		return 0;
	tab->tab_size = tsize;
	tab->user_data = ud;
	tab->get_key = gk;
	tab->free_ele = f;
	if (!hctx) {
		hctx = &default_hashctx;
		if (!default_hashctx) {
			Sys_RegisterShutdown (hash_shutdown, 0);
		}
	}
	if (!*hctx) {
		*hctx = calloc (1, sizeof (hashctx_t));
	}
	tab->hashctx = *hctx;

	while (tsize) {
		tab->size_bits++;
		tsize = ((unsigned int) tsize) >> 1;
	}

	tab->get_hash = get_hash;
	tab->compare = compare;
	return tab;
}

VISIBLE void
Hash_SetHashCompare (hashtab_t *tab, uintptr_t (*gh)(const void*,void*),
					 int (*cmp)(const void*,const void*,void*))
{
	tab->get_hash = gh;
	tab->compare = cmp;
}

VISIBLE void
Hash_DelTable (hashtab_t *tab)
{
	Hash_FlushTable (tab);
	free (tab);
}

VISIBLE void
Hash_FlushTable (hashtab_t *tab)
{
	size_t i;

	for (i = 0; i < tab->tab_size; i++) {
		while (tab->tab[i]) {
			hashlink_t *t = tab->tab[i]->next;
			void *data = tab->tab[i]->data;

			free_hashlink (tab->tab[i], tab->hashctx);
			tab->tab[i] = t;
			if (tab->free_ele)
				tab->free_ele (data, tab->user_data);
		}
	}
	tab->num_ele = 0;
}

static int
hash_add_element (hashtab_t *tab, uintptr_t h, void *ele)
{
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = new_hashlink (tab->hashctx);

	if (!lnk)
		return -1;
	if (tab->tab[ind])
		tab->tab[ind]->prev = &lnk->next;
	lnk->next = tab->tab[ind];
	lnk->prev = &tab->tab[ind];
	lnk->data = ele;
	tab->tab[ind] = lnk;
	tab->num_ele++;
	return 0;
}

VISIBLE int
Hash_Add (hashtab_t *tab, void *ele)
{
	uintptr_t   h = Hash_String (tab->get_key(ele, tab->user_data));
	return hash_add_element (tab, h, ele);
}

VISIBLE int
Hash_AddElement (hashtab_t *tab, void *ele)
{
	uintptr_t   h = tab->get_hash (ele, tab->user_data);
	return hash_add_element (tab, h, ele);
}

VISIBLE void *
Hash_Find (hashtab_t *tab, const char *key)
{
	uintptr_t   h = Hash_String (key);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind];

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data)))
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

VISIBLE void *
Hash_nFind (hashtab_t *tab, const char *key, size_t sz)
{
	uintptr_t   h = Hash_nString (key, sz);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind];

	while (lnk) {
		if (strncmp (key, tab->get_key (lnk->data, tab->user_data), sz) == 0)
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

VISIBLE void *
Hash_FindElement (hashtab_t *tab, const void *ele)
{
	uintptr_t   h = tab->get_hash (ele, tab->user_data);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind];

	while (lnk) {
		if (tab->compare (lnk->data, ele, tab->user_data))
			return lnk->data;
		lnk = lnk->next;
	}
	return 0;
}

VISIBLE void **
Hash_FindList (hashtab_t *tab, const char *key)
{
	uintptr_t   h = Hash_String (key);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind], *start = 0;
	int         count = 0;
	void      **list;

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data))) {
			count++;
			if (!start)
				start = lnk;
		}
		lnk = lnk->next;
	}
	if (!count)
		return 0;
	list = malloc ((count + 1) * sizeof (void *));
	for (count = 0, lnk = start; lnk; lnk = lnk->next) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data)))
			list[count++] = lnk->data;
	}
	list[count] = 0;
	return list;
}

VISIBLE void **
Hash_FindElementList (hashtab_t *tab, void *ele)
{
	uintptr_t   h = tab->get_hash (ele, tab->user_data);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind], *start = 0;
	int         count = 0;
	void      **list;

	while (lnk) {
		if (tab->compare (lnk->data, ele, tab->user_data)) {
			count++;
			if (!start)
				start = lnk;
		}
		lnk = lnk->next;
	}
	if (!count)
		return 0;
	list = malloc ((count + 1) * sizeof (void *));
	for (count = 0, lnk = start; lnk; lnk = lnk->next) {
		if (tab->compare (lnk->data, ele, tab->user_data))
			list[count++] = lnk->data;
	}
	list[count] = 0;
	return list;
}

VISIBLE void *
Hash_Del (hashtab_t *tab, const char *key)
{
	uintptr_t   h = Hash_String (key);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind];
	void *data;

	while (lnk) {
		if (strequal (key, tab->get_key (lnk->data, tab->user_data))) {
			data = lnk->data;
			if (lnk->next)
				lnk->next->prev = lnk->prev;
			*lnk->prev = lnk->next;
			free_hashlink (lnk, tab->hashctx);
			tab->num_ele--;
			return data;
		}
		lnk = lnk->next;
	}
	return 0;
}

VISIBLE void *
Hash_DelElement (hashtab_t *tab, void *ele)
{
	uintptr_t   h = tab->get_hash (ele, tab->user_data);
	size_t ind = get_index (h, tab->tab_size, tab->size_bits);
	hashlink_t *lnk = tab->tab[ind];
	void *data;

	while (lnk) {
		if (tab->compare (lnk->data, ele, tab->user_data)) {
			data = lnk->data;
			if (lnk->next)
				lnk->next->prev = lnk->prev;
			*lnk->prev = lnk->next;
			free_hashlink (lnk, tab->hashctx);
			tab->num_ele--;
			return data;
		}
		lnk = lnk->next;
	}
	return 0;
}

VISIBLE void
Hash_Free (hashtab_t *tab, void *ele)
{
	if (ele && tab->free_ele)
		tab->free_ele (ele, tab->user_data);
}

VISIBLE size_t
Hash_NumElements (hashtab_t *tab)
{
	return tab->num_ele;
}

VISIBLE void **
Hash_GetList (hashtab_t *tab)
{
	void      **list;
	void      **l;
	size_t      ind;

	l = list = malloc ((tab->num_ele + 1) * sizeof (void *));
	if (!list)
		return 0;
	for (ind = 0; ind < tab->tab_size; ind++) {
		hashlink_t *lnk;

		for (lnk = tab->tab[ind]; lnk; lnk = lnk->next) {
			*l++ = lnk->data;
		}
	}
	*l++ = 0;
	return list;
}

VISIBLE void **
Hash_Select (hashtab_t *tab, hash_select_t select, void *data)
{
	void      **list;
	void      **l;
	size_t      ind;

	l = list = malloc ((tab->num_ele + 1) * sizeof (void *));
	if (!list)
		return 0;
	for (ind = 0; ind < tab->tab_size; ind++) {
		hashlink_t *lnk;

		for (lnk = tab->tab[ind]; lnk; lnk = lnk->next) {
			if (select (lnk->data, data)) {
				*l++ = lnk->data;
			}
		}
	}
	*l++ = 0;
	return list;
}

VISIBLE void
Hash_ForEach (hashtab_t *tab, hash_action_t action, void *data)
{
	size_t      ind;

	for (ind = 0; ind < tab->tab_size; ind++) {
		hashlink_t *lnk;

		for (lnk = tab->tab[ind]; lnk; lnk = lnk->next) {
			action (lnk->data, data);
		}
	}
}

static inline double
sqr (double x)
{
	return x * x;
}

VISIBLE void
Hash_Stats (hashtab_t *tab)
{
	int        *lengths = calloc (tab->tab_size, sizeof (int));
	int         chains = 0;
	size_t      i;
	int         min_length = tab->num_ele;
	int         max_length = 0;

	if (!lengths) {
		Sys_Printf ("Hash_Stats: memory alloc error\n");
		return;
	}

	for (i = 0; i < tab->tab_size; i++) {
		hashlink_t *lnk = tab->tab[i];

		while (lnk) {
			lengths[i]++;
			lnk = lnk->next;
		}
		if (lengths[i]) {
			min_length = min (min_length, lengths[i]);
			max_length = max (max_length, lengths[i]);
			chains++;
		}
	}
	Sys_Printf ("%d elements\n", (int)tab->num_ele);
	Sys_Printf ("%d / %d chains\n", chains, (int)tab->tab_size);
	if (chains) {
		double      average = (double) tab->num_ele / chains;
		double      variance = 0;
		Sys_Printf ("%d minium chain length\n", min_length);
		Sys_Printf ("%d maximum chain length\n", max_length);
		Sys_Printf ("%.3g average chain length\n", average);
		for (i = 0; i < tab->tab_size; i++) {
			if (lengths[i])
				variance += sqr (lengths[i] - average);
		}
		variance /= chains;
		Sys_Printf ("%.3g variance, %.3g standard deviation\n",
					variance, sqrt (variance));
	}
}
