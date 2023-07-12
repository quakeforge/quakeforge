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

#ifndef __QF_hash_h
#define __QF_hash_h

#include <stdint.h>
#include <stdlib.h>

/** \defgroup hash Hash tables
	\ingroup utils
*/
///@{

typedef struct hashtab_s hashtab_t;
typedef struct hashctx_s hashctx_t;
typedef int (*hash_select_t) (void *ele, void *data);
typedef void (*hash_action_t) (void *ele, void *data);

/** create a new hash table.
	\param tsize	table size. larger values will give better distribution, but
				use more memory.
	\param gk	a function that returns a string to be used as the key for
				inserting or finding the element. First parameter is a pointer
				to the element from which to extract the key, the second is
				the user data pointer.
	\param f	a function to free the element. Called from only
				Hash_FlushTable and Hash_DelTable. The first parameter is the
				element to be freed and the second is the user data pointer.
	\param ud	user data pointer. set to whatever you want, it will be passed
				to the get key and free functions as the second parameter.
	\param hctx Address of opaque pointer used for per-thread allocation of
				internal memory. If null, a local static pointer will be used,
				but the hash table will not be thread-safe unless all tables
				created with a null \a hctx are used in only the one thread.
				However, this applys only to updating a hash table; hash
				tables that are not updated can be safely shared between
				threads.
	\return		pointer to the hash table (to be passed to the other functions)
				or 0 on error.

	Hash_Add(), Hash_Find(), Hash_FindList() and Hash_Del() use gk and strcmp.

	multiple inserions of the same key are fine; later insertions override
	previous ones until the later one is removed (Hash_Del).
*/
hashtab_t *Hash_NewTable (int tsize, const char *(*gk)(const void*,void*),
						  void (*f)(void*,void*), void *ud, hashctx_t **hctx);

/** change the hash and compare functions used by the Hash_*Element functions.
	the default hash function just returns the address of the element, and the
	default compare just compares the addresses. compare is to return 0 for not
	equal and non-0 otherwise.

	With suitably crafted gh and cmp functions, Hash_*Element functions can
	be mixed with the non-element functions, but by default the results will
	be undefined.

	\param tab	the table configure
	\param gh takes the same parameters as gk in Hash_NewTable
	\param cmp is element 1, element 2, userdata
*/
void Hash_SetHashCompare (hashtab_t *tab, uintptr_t (*gh)(const void*,void*),
						  int (*cmp)(const void*,const void*,void*));


/** delete a hash table.
	\param tab	the table to be deleted
*/
void Hash_DelTable (hashtab_t *tab);

void Hash_DelContext (hashctx_t *hashctx);

/** clean out all the entries from a hash table, starting over again.
	\param tab	the table to be cleared
*/
void Hash_FlushTable (hashtab_t *tab);

/** add an entry to a hash table.
	\param tab	the table to be added to
	\param ele	the element to add to the table
	\return		0 for success, -1 for error.
*/
int Hash_Add (hashtab_t *tab, void *ele);

/** add an entry to a hash table.
	\param tab	the table to be added to
	\param ele	the element to add to the table
	\return		0 for success, -1 for error.
*/
int Hash_AddElement (hashtab_t *tab, void *ele);

/** find an element within a hash table.
	\param tab	the table to search
	\param key	the key string identifying the element being searched for
	\return		pointer to the element if found, otherwise 0.
*/
void *Hash_Find (hashtab_t *tab, const char *key);

/** find an element within a hash table.
	\param tab	the table to search
	\param key	the key string identifying the element being searched for
	\param sz   the maximum length of the key string
	\return		pointer to the element if found, otherwise 0.
*/
void *Hash_nFind (hashtab_t *tab, const char *key, size_t sz);

/** find an element within a hash table.
	\param tab	the table to search
	\param ele	element with info identifying the element being searched for
	\return		pointer to the element if found, otherwise 0.
*/
void *Hash_FindElement (hashtab_t *tab, const void *ele);

/** find a list of elements within a hash table.
	\param tab	the table to search
	\param key	the key string identifying the elements being searched for
	\return		a null terminated list of element pointers if at least one
				found, otherwise 0.
	\note		it is the caller's responsibilty to free() the list.

	returned list is guaranteed to be in reverse order of insertion. ie,
	deleting items from the list in list order will delete the correct items.
*/
void **Hash_FindList (hashtab_t *tab, const char *key);

/** find a list of elements within a hash table.
	\param tab	the table to search
	\param ele	element with info identifying the elements being searched for
	\return		a null terminated list of element pointers if at least one
				found, otherwise 0.
	\note		it is the caller's responsibilty to free() the list.

	returned list is guaranteed to be in reverse order of insertion. ie,
	deleting items from the list in list order will delete the correct items.
*/
void **Hash_FindElementList (hashtab_t *tab, void *ele);

/** delete an element from a hash table.
	\param tab	the table to remove the element from
	\param key	the key string identifying the element to be deleted
	\return		a pointer to the element on success, 0 if the element could not
				be found.

	Does \em NOT call the free element function. That is the caller's
	responsibility.
*/
void *Hash_Del (hashtab_t *tab, const char *key);

/** delete an element from a hash table.
	\param tab	the table to remove the element from
	\param ele	element with info identifying the element to be deleted
	\return		a pointer to the element on success, 0 if the element could not
				be found.

	Does \em NOT call the free element function. That is the caller's
	responsibility.
*/
void *Hash_DelElement (hashtab_t *tab, void *ele);

/** calls the free element function for the supplied ele
	\param tab	the table associated with the element (for the free function)
	\param ele	the element to be freed

	\par Example:
		Hash_Free (tab, Hash_Del (tab, key));
*/
void Hash_Free (hashtab_t *tab, void *ele);

/** hash a string.
	\param str	the string to hash
	\return		the hash value of the string.

	this is the same function as used internally.
*/
uintptr_t Hash_String (const char *str) __attribute__((pure));

/** hash a string.
	\param str	the string to hash
	\param sz   the maximum length of the string
	\return		the hash value of the string.

	this is the same function as used internally.
*/
uintptr_t Hash_nString (const char *str, size_t sz) __attribute__((pure));

/** hash a buffer.
	\param buf	the buffer to hash
	\param len	the size of the buffer
	\return		the hash value of the string.
*/
uintptr_t Hash_Buffer (const void *buf, int len) __attribute__((pure));

/** get the size of the table
	\param tab	the table in question
	\return		the number of elements in the table.
*/
size_t Hash_NumElements (hashtab_t *tab) __attribute__((pure));

/** list of all elements in the table.
	\param tab	the table to list
	\return		a null terminated list of element pointers for all elements
				in the table
	\note		it is the caller's responsibilty to free() the list.

	returned list is guaranteed to be in reverse order of insertion for
	elements with the same key. ie, deleting items from the list in list order
	will delete the correct items.
*/
void **Hash_GetList (hashtab_t *tab);

/** list of all matching elements in the table.
	\param tab	the table to search
	\param select	function that tests for a match. The expected return value
					is 0 for no match, non-zero for a match.
	\param data		context data passed to the \a select function
	\return			a null terminated list of element pointers for all matchin
					elements in the table
	\note		it is the caller's responsibilty to free() the list.

	returned list is guaranteed to be in reverse order of insertion for
	elements with the same key. ie, deleting items from the list in list order
	will delete the correct items.
*/
void **Hash_Select (hashtab_t *tab, hash_select_t select, void *data);


/** call a function for all elements in the table.
	\param tab	the table to search
	\param action	function to call for each elelemnt
	\param data		context data passed to the \a action function

	call order is guaranteed to be in reverse order of insertion for
	elements with the same key
*/
void Hash_ForEach (hashtab_t *tab, hash_action_t action, void *data);

/** dump statistics about the hash table
	\param tab	the table to dump
*/
void Hash_Stats (hashtab_t *tab);

///@}

#endif//__QF_hash_h
