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

#ifndef __hash_h
#define __hash_h

#include <stdlib.h> // should be sys/types.h, but bc is stupid

typedef struct hashlink_s {
	struct hashlink_s *next;
	struct hashlink_s **prev;
	void *data;
} hashlink_t;

typedef struct hashtab_s {
	size_t tab_size;
	void *user_data;
	const char *(*get_key)(void*,void*);
	void (*free_ele)(void*,void*);
	hashlink_t *tab[1];				// variable size
} hashtab_t;

hashtab_t *Hash_NewTable (int tsize, const char *(*gk)(void*,void*),
						  void (*f)(void*,void*), void *ud);
void Hash_DelTable (hashtab_t *tab);
void Hash_FlushTable (hashtab_t *tab);
int Hash_Add (hashtab_t *tab, void *ele);
void *Hash_Find (hashtab_t *tab, const char *key);
int Hash_Del (hashtab_t *tab, const char *key);

#endif // __hash_h
