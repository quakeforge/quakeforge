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

typedef struct hashtab_s hashtab_t;

/*
	create a new hash table:
		tsize:	table size. larger values will give better distribution, but
				use more memory.
		gk:		a function that returns a string to be used as the key for
				inserting or finding the element. First parameter is a pointer
				to the element from which to extract the key, the second is
				the user data pointer.
		f:		a function to free the element. Only ever called from
				Hash_FlushTable and Hash_DelTable. The first parameter is the
				element to be freed and the second is the user data pointer.
		ud:		user data pointer. set to whatever you want, it will be passed
				to the get key and free functions as the second parameter.
	returns a pointer to the hash table (to be passed to the other functions)
	or 0 on error.

	multiple inserions of the same key are fine; later insertions override
	previous ones until the later one is removed (Hash_Del).
*/
hashtab_t *Hash_NewTable (int tsize, const char *(*gk)(void*,void*),
						  void (*f)(void*,void*), void *ud);

/*
	delete a hash table:
		tab:	the table to be deleted
*/
void Hash_DelTable (hashtab_t *tab);

/*
	clean out all the entries from a hash table, starting over again:
		tab:	the table to be cleared
*/
void Hash_FlushTable (hashtab_t *tab);

/*
	add an entry to a hash table:
		tab:	the table to be added to
		ele:	the element to add to the table
	returns 0 for success, -1 for error.
*/
int Hash_Add (hashtab_t *tab, void *ele);

/*
	find an element within a hash table:
		tab:	the table to search
		key:	the key string identifying the element being searched for
	returns a pointer to the element if found, otherwise 0.
*/
void *Hash_Find (hashtab_t *tab, const char *key);

/*
	delete an element from a hash table:
		tab:	the table to remove the element from
		key:	the key string identifying the element to be deleted
	returns 0 on success, -1 if the element could not be found.
	Does /NOT/ call the free element function. That is the caller's
	responsibility.
*/
int Hash_Del (hashtab_t *tab, const char *key);

#endif // __hash_h
