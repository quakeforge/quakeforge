/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include <string.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/gib_vars.h"
#include "QF/gib_buffer.h"
#include "QF/gib_parse.h"
        
hashtab_t *gib_globals = 0;
hashtab_t *gib_domains = 0;

static gib_var_t *
GIB_Var_New (const char *key)
{
	gib_var_t *new = calloc (1, sizeof (gib_var_t));
	new->array = calloc (1, sizeof (dstring_t *));
	new->key = strdup (key);
	return new;
}

static const char *
GIB_Var_Get_Key (void *ele, void *ptr)
{
	return ((gib_var_t *)ele)->key;
}

static void
GIB_Var_Free (void *ele, void *ptr)
{
	unsigned int i;
	gib_var_t *l = (gib_var_t *)ele;
	for (i = 0; i < l->size; i++)
		if (l->array[i])
			dstring_delete (l->array[i]);
	free((void *)l->key);
	free(l);
}

gib_var_t *
GIB_Var_Get (hashtab_t *first, hashtab_t *second, const char *key)
{
	gib_var_t *var;
	if (first && (var = Hash_Find (first, key)))
		return var;
	else if (second && (var = Hash_Find (second, key)))
		return var;
	else
		return 0;
}

/* Modifies key but restores it before returning */
gib_var_t *
GIB_Var_Get_Complex (hashtab_t **first, hashtab_t **second, char *key, unsigned int *ind, qboolean create)
{
	unsigned int i, index;
	qboolean fix = false;
	gib_var_t *var;
	
	i = strlen(key);
	index = 0;
	if (i && key[i-1] == ']')
		for (i--; i; i--)
			if (key[i] == '[') {
				index = atoi (key+i+1);
				key[i] = 0;
				fix = true;
				break;
			}
	if (!(var = GIB_Var_Get (*first, *second, key))) {
		if (create) {
			var = GIB_Var_New (key);
			if (!*first)
				*first = Hash_NewTable (256, GIB_Var_Get_Key, GIB_Var_Free, 0);
			Hash_Add (*first, var);
		} else return 0;
	}
	if (fix)
		key[i] = '[';
	if (index >= var->size) {
		if (create) {
			var->array = realloc (var->array, (index+1) * sizeof (dstring_t *));
			memset (var->array+var->size, 0, (index+1 - var->size) * sizeof (dstring_t *));
			var->size = index+1;
		} else return 0;
	}
	if (!var->array[index])
		var->array[index] = dstring_newstr ();
	*ind = index;
	return var;
}

void
GIB_Var_Assign (gib_var_t *var, unsigned int index, dstring_t **values, unsigned int numv)
{
	unsigned int i, len;

	// Now, expand the array to the correct size
	len = numv + index;
	if (len >= var->size) {
		var->array = realloc (var->array, len * sizeof (dstring_t *));
		memset (var->array+var->size, 0, (len-var->size) * sizeof (dstring_t *));
		var->size = len;
	} else if (len < var->size) {
		for (i = len; i < var->size; i++)
			if (var->array[i])
				dstring_delete (var->array[i]);
		var->array = realloc (var->array, len * sizeof (dstring_t *));
	}
	var->size = len;
	for (i = 0; i < numv; i++) {
		if (var->array[i+index])
			dstring_clearstr (var->array[i+index]);
		else
			var->array[i+index] = dstring_newstr ();
		dstring_appendstr (var->array[i+index], values[i]->str);
	}
}

static const char *
GIB_Domain_Get_Key (void *ele, void *ptr)
{
	return ((gib_domain_t *)ele)->name;
}

static void
GIB_Domain_Free (void *ele, void *ptr)
{
	gib_domain_t *l = (gib_domain_t *)ele;
	Hash_DelTable (l->vars);
	free ((void *)l->name);
	free (l);
}

hashtab_t *
GIB_Domain_Get (const char *name)
{
	gib_domain_t *d = Hash_Find (gib_domains, name);
	if (!d) {
		d = calloc (1, sizeof (gib_domain_t));
		d->name = strdup(name);
		d->vars = Hash_NewTable (1024, GIB_Var_Get_Key, GIB_Var_Free, 0);
		Hash_Add (gib_domains, d);
	}
	return d->vars;
}

void
GIB_Var_Init (void)
{
	gib_globals = Hash_NewTable (1024, GIB_Var_Get_Key, GIB_Var_Free, 0);
	gib_domains = Hash_NewTable (1024, GIB_Domain_Get_Key, GIB_Domain_Free, 0);
}
