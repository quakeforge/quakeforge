/*
	bi_inputline.c

	CSQC string hashes builtins

	Copyright (C) 2002 Robin Redeker <elmex@x-paste.de>

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/draw.h"
#include "QF/progs.h"
#include "QF/zone.h"

#define MAX_SH_VALUES 16

/*
	DESIGN NOTE (by elmex):

	This file contains code for QuakeC hashes.
	The hases are designed to have around 10 sub-values for
	one key.
	Hashes are stored as hash-ids in the QC-environment.

	The key->value pairs in the hashes are stored in the
	order they are set. So its easy to make array-hashes.
	The function which gets the Lenght of a hash is specialy 
	made for array purposes.
	
	TODO: Check the FIXME's below in the code. 
	      (please taniwha ;)
	      (about memory leaks)
*/

// a hash element
typedef struct {
	char *  key;
	char *  values[MAX_SH_VALUES];
} str_hash_elem;	

// a structure of a hash
typedef struct {
	str_hash_elem **elements;
	int             cnt_elements;
} str_hash;

// a list structure of hashes
typedef struct {
	str_hash **hashes;
	int        cnt_hashes;
} strh_resources_t;

/*
	bi_StringHash_Create

	Creates a hash structure and gives back the hash-id to the
	QC-environment
*/
static void
bi_StringHash_Create (progs_t *pr)
{
	strh_resources_t *res = PR_Resources_Find (pr, "StringHash");
	int               i;
	int               hash_id=-1;

	// check if there is a empty hash in the array
	for(i = 0; i < res->cnt_hashes; i++) {
		if(res->hashes[i]->cnt_elements == 0) {
			// we found a empty already allocated hash
			hash_id = i;
			break;
		}
	}
	if(hash_id == -1) {
		/* allocate a new hash struct, if
		   there is no old one */
		if(res->hashes == NULL) { // allocate a new list of hashes
			res->hashes = (str_hash **) malloc(
				sizeof(str_hash*) * (res->cnt_hashes + 1));
		} else {	// reallocate the list of hashes
			res->hashes = (str_hash **) realloc(res->hashes, 
				sizeof(str_hash*) * (res->cnt_hashes + 1));
		}
		hash_id = res->cnt_hashes;

		res->hashes[hash_id] = 
			(str_hash*) malloc(sizeof(str_hash));
		// dont forge to clean the hash
		memset(res->hashes[hash_id],0,sizeof(str_hash));	

		res->cnt_hashes++; // increase cnt of allocated hashes
	}
	G_INT (pr, OFS_RETURN) = hash_id;
}

/*
	bi_StringHash_Destroy

	Destroys a hash
*/
static void
bi_StringHash_Destroy (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	str_hash   *sh = NULL;
	int         i,d;

	if(hash_id >= res->cnt_hashes || hash_id < 0) {
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	sh = res->hashes[hash_id];

	/* we dont really destroy the allocated hash,
		but we free the elements of the hash 
		and mark it for reuseing */

	for(i = 0; i < sh->cnt_elements; i++) {
		if(sh->elements[i] != NULL) {
			/* they should never be NULL,
			   buy, who knows? */
			PR_Error(pr, "NULL hash-element found -> not supposed!");
		} else {
			for(d=0;d<MAX_SH_VALUES;d++) {
				// free key
				free(sh->elements[i]->key);
				// free values
				if(sh->elements[i]->values[d] != NULL) {
					free(sh->elements[i]->values[d]);
				}
			}
			free(sh->elements[i]); // free str_hash_elem structs
		}
		/*
			FIXME: HELP: taniwha??? do i have to FREE
			the strings ??
		*/
	}
	free(sh->elements); // free the list pointer
	sh->elements = NULL;
	sh->cnt_elements = 0;
	G_INT (pr, OFS_RETURN) = 1;
}


/*
	bi_StringHash_Set

	Sets a the key-value (with a special id) in a hash
	to a value.
	If a non existing key is given a element if generated for it.
	FIXME: look if this functions does mem-leak
*/
static void
bi_StringHash_Set (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	const char *key = G_STRING (pr, OFS_PARM1);
	const char *val = G_STRING (pr, OFS_PARM2);
	int         val_id = G_INT (pr, OFS_PARM3);
	str_hash   *sh = NULL;
	int         i,found_fl=0;

	// validate the hash ID
	if(res->hashes == NULL || 
		(hash_id >= res->cnt_hashes || hash_id < 0) ||
		(val_id < 0 || val_id >= MAX_SH_VALUES)) 
	{
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	sh = res->hashes[hash_id];

	// first search for existing key
	for(i = 0; i < sh->cnt_elements; i++) {
		if(strcmp(sh->elements[i]->key, key) == 0) {
			// found already a element with that key
			if(sh->elements[i]->values[val_id] == NULL) { // empty val
				// strdup() because strings can dissappear
				sh->elements[i]->values[val_id] = strdup(val);
			} else { 
				// when using strdup(), we have to free the stuff properly
				free(sh->elements[i]->values[val_id]);
				sh->elements[i]->values[val_id] = strdup(val);
			}
			found_fl = 1;
		}
	}
	if(!found_fl) { // add a new element
		if(sh->elements == NULL) { // alloc new elements list pointer
			sh->elements = (str_hash_elem**) malloc(sizeof(str_hash_elem*));
			sh->cnt_elements = 0; // 0 because usage as index here
		} else {
			sh->elements = (str_hash_elem**) realloc(sh->elements, 
				sizeof(str_hash_elem*) * (sh->cnt_elements+1));
		}
		sh->elements[sh->cnt_elements] = malloc(sizeof(str_hash_elem));
		memset(sh->elements[sh->cnt_elements],0,sizeof(str_hash_elem));

		sh->elements[sh->cnt_elements]->key = strdup(key);
		sh->elements[sh->cnt_elements]->values[val_id] = strdup(val);

		sh->cnt_elements++;
	}
	G_INT (pr, OFS_RETURN) = 1;
	return;
}


/*
	bi_StringHash_SetIdx

	Sets a the key-value (with a special id) in a hash
	to a value. This function works by index of the element.
	A element in hash is NOT generated automatically.
	FIXME: look if this functions does mem-leak
*/
static void
bi_StringHash_SetIdx (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	int         idx = G_INT (pr, OFS_PARM1);
	const char *val = G_STRING (pr, OFS_PARM2);
	int         val_id = G_INT (pr, OFS_PARM3);
	str_hash   *sh = NULL;

	// validate the hash ID
	if(res->hashes == NULL || 
		(hash_id >= res->cnt_hashes || hash_id < 0) ||
		(val_id < 0 || val_id >= MAX_SH_VALUES))
	{
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	sh = res->hashes[hash_id];

	if(idx < 0 || idx >= sh->cnt_elements || sh->elements[idx] == NULL) {
		if(sh->elements[idx] == NULL) 
			PR_Error(pr, "NULL hash-element found -> not supposed!");

		G_INT (pr, OFS_RETURN) = 0;
		return;
	}

	if(sh->elements[idx]->values[val_id] != NULL) {
		free(sh->elements[idx]->values[val_id]);
	}
	sh->elements[idx]->values[val_id] = strdup(val);


	G_INT (pr, OFS_RETURN) = 1;
	return;
}

/*
	bi_StringHash_Get

	Gets the value of a key and its id in a hash
*/
static void
bi_StringHash_Get (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	const char *key = G_STRING (pr, OFS_PARM1);
	int         val_id = G_INT (pr, OFS_PARM2);
	str_hash   *sh = NULL;
	int         i,found_fl=0;
	const char *retstr = NULL;

	// validate the hash ID
	if(res->hashes == NULL || hash_id >= res->cnt_hashes || hash_id < 0 || 
		val_id >= MAX_SH_VALUES) 
	{
		retstr = "";
		RETURN_STRING(pr, retstr);
		return;
	}
	sh = res->hashes[hash_id];

	// first search for existing key
	for(i = 0; i < sh->cnt_elements; i++) {
		if(strcmp(sh->elements[i]->key, key) == 0) {
			if(sh->elements[i]->values[val_id] != NULL) {
				retstr = sh->elements[i]->values[val_id];
			} else {
				retstr = "";
			}
			found_fl = 1;
		}
	}
	if(!found_fl) {
		retstr = "";
	}
	RETURN_STRING(pr, retstr);
}

/*
	bi_StringHash_Length

	Gets the count of the elements in a hash
*/
static void
bi_StringHash_Length (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	str_hash   *sh = NULL;

	// validate the hash ID
	if(res->hashes == NULL || hash_id >= res->cnt_hashes || hash_id < 0) {
		G_INT (pr, OFS_RETURN) = 0;
		return;
	}
	sh = res->hashes[hash_id];

	G_INT (pr, OFS_RETURN) = sh->cnt_elements;
}

/*
	bi_StringHash_GetIdx

	Gets a hash elment by its index
	special: if the val_id is -1 the key of the element will
	         be returned
*/
static void
bi_StringHash_GetIdx (progs_t *pr)
{
	strh_resources_t* res = PR_Resources_Find (pr, "StringHash");
	int         hash_id = G_INT (pr, OFS_PARM0);
	int         idx = G_INT (pr, OFS_PARM1);
	int         val_id = G_INT (pr, OFS_PARM2);
	str_hash   *sh = NULL;
	const char       *retstr = NULL;

	// validate the hash ID
	if(res->hashes == NULL || hash_id >= res->cnt_hashes || hash_id < 0) {
		retstr = NULL;
	}
	sh = res->hashes[hash_id];

	if(idx < 0 || idx >= sh->cnt_elements || 
		(val_id < -1 || val_id >= MAX_SH_VALUES)) 
	{
		retstr = NULL;
	} else {
		if(val_id == -1) {
			retstr = sh->elements[idx]->key;
		} else {
			retstr = sh->elements[idx]->values[val_id];
		}
	}
	if(retstr == NULL) { retstr = ""; }

	RETURN_STRING(pr, retstr);
}

/*
	bi_strh_clear

	Free the dynamic allocated memory
	XXX: taniwha: i dont know what to free
         exactly, could you validate this code?
*/
static void
bi_strh_clear (progs_t *pr, void *data)
{
	strh_resources_t *res = (strh_resources_t *)data;
	int         i,d;

	for (i = 0; i < res->cnt_hashes; i++) {
		if (res->hashes[i]) {
			for(d = 0; d < res->hashes[i]->cnt_elements; d++) {
				free(res->hashes[i]->elements[d]);
			}
			free(res->hashes[i]->elements);
			free(res->hashes[i]);
			res->hashes[i] = 0;
		}
	}
	free (res->hashes);
	res->hashes = 0;
	res->cnt_hashes = 0;
}


/*
	StringHash_Progs_Init

	Inits the Progs-system with StringHash resources and
	functions.
*/
void
StringHash_Progs_Init (progs_t *pr)
{
	strh_resources_t *res = malloc (sizeof (strh_resources_t));
	res->cnt_hashes = 0;
	res->hashes = NULL;

	PR_Resources_Register (pr, "StringHash", res, bi_strh_clear);
	PR_AddBuiltin (pr, "StringHash_Create", bi_StringHash_Create, -1);
	PR_AddBuiltin (pr, "StringHash_Destroy", bi_StringHash_Destroy, -1);
	PR_AddBuiltin (pr, "StringHash_Set", bi_StringHash_Set, -1);
	PR_AddBuiltin (pr, "StringHash_Get", bi_StringHash_Get, -1);
	PR_AddBuiltin (pr, "StringHash_Length", bi_StringHash_Length, -1);
	PR_AddBuiltin (pr, "StringHash_SetIdx", bi_StringHash_SetIdx, -1);
	PR_AddBuiltin (pr, "StringHash_GetIdx", bi_StringHash_GetIdx, -1);
}

/* 
	XXX NOTE by elmex: 
	A file, decripted like this is what 
	i want to see everywhere in qf-cvs =) 
	No excuse for undocumented code and design without
	a reason for it.
	We/I want to know why something was designed how it is.
*/
