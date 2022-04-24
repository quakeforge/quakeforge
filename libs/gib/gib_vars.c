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

#include <string.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/va.h"
#include "QF/hash.h"
#include "QF/cvar.h"

#include "gib_parse.h"
#include "gib_vars.h"

hashtab_t  *gib_globals = 0;
hashtab_t  *gib_domains = 0;

static gib_var_t *
GIB_Var_New (const char *key)
{
	gib_var_t  *new = calloc (1, sizeof (gib_var_t));

	new->array = calloc (1, sizeof (struct gib_varray_s));
	new->key = strdup (key);
	return new;
}

static const char *
GIB_Var_Get_Key (const void *ele, void *ptr)
{
	return ((gib_var_t *) ele)->key;
}

static void
GIB_Var_Free (void *ele, void *ptr)
{
	unsigned int i;
	gib_var_t  *l = (gib_var_t *) ele;

	for (i = 0; i < l->size; i++) {
		if (l->array[i].value)
			dstring_delete (l->array[i].value);
		if (l->array[i].leaves)
			Hash_DelTable (l->array[i].leaves);
	}
	free (l->array);
	free ((void *) l->key);
	free (l);
}

gib_var_t *
GIB_Var_Get (hashtab_t * first, hashtab_t * second, const char *key)
{
	gib_var_t  *var;

	if (first && (var = Hash_Find (first, key)))
		return var;
	else if (second && (var = Hash_Find (second, key)))
		return var;
	else
		return 0;
}

/* Alters key, but restores it */
gib_var_t *
GIB_Var_Get_Complex (hashtab_t ** first, hashtab_t ** second, char *key,
					 unsigned int *ind, qboolean create)
{
	static hashtab_t *zero = 0;
	unsigned int i, n, index = 0, len, start;
	gib_var_t  *var = 0;

	len = strlen(key);
	for (start = i = 0; i <= len; i++) {
		if (key[i] == '.' || key[i] == 0) {
			index = 0;
			key[i] = 0;
			n = 0;
			if (i && key[i - 1] == ']')
				for (n = i - 1; n; n--)
					if (key[n] == '[') {
						index = atoi (key + n + 1);
						key[n] = 0;
						break;
					}
			if (!(var = GIB_Var_Get (*first, *second, key+start)) && create) {
				var = GIB_Var_New (key+start);
				if (!*first)
					*first = Hash_NewTable (256, GIB_Var_Get_Key,
											GIB_Var_Free, 0, 0);
				Hash_Add (*first, var);
			}

			// We are done looking up/creating var, fix up key
			if (n)
				key[n] = '[';
			if (i < len)
				key[i] = '.';

			// Give up
			if (!var)
				return 0;
			else if (index >= var->size) {
				if (create) {
					var->array = realloc (var->array,
									(index + 1) * sizeof (struct gib_varray_s));
					memset (var->array + var->size, 0,
						(index + 1 - var->size) * sizeof (struct gib_varray_s));
					var->size = index + 1;
				} else
					return 0;
			}
			second = &zero;
			first = &var->array[index].leaves;
			start = i+1;
		}
	}
	if (!var->array[index].value)
		var->array[index].value = dstring_newstr ();
	*ind = index;
	return var;
}

/* Mangles the hell out of key */
gib_var_t *
GIB_Var_Get_Very_Complex (hashtab_t ** first, hashtab_t ** second, dstring_t *key, unsigned int start,
					 unsigned int *ind, qboolean create)
{
	static hashtab_t *zero = 0;
	hashtab_t *one = *first, *two = *second;
	unsigned int i, index = 0, index2 = 0, n, protect, varstartskip;
	gib_var_t  *var = 0;
	cvar_t *cvar;
	char c;
	const char *str;
	qboolean done = false;

	for (i = start, protect = 0; !done; i++) {
		if (key->str[i] == '.' || key->str[i] == 0) {
			index = 0;
			if (!key->str[i])
				done = true;
			key->str[i] = 0;
			if (i && key->str[i - 1] == ']')
				for (n = i-1; n; n--)
					if (key->str[n] == '[') {
						index = atoi (key->str + n + 1);
						key->str[n] = 0;
						break;
					}
			if (!(var = GIB_Var_Get (*first, *second, key->str+start))) {
				if (create) {
					var = GIB_Var_New (key->str+start);
					if (!*first)
						*first = Hash_NewTable (256, GIB_Var_Get_Key,
												GIB_Var_Free, 0, 0);
					Hash_Add (*first, var);
				} else
					return 0;
			}
			if (index >= var->size) {
				if (create) {
					var->array = realloc (var->array,
									(index + 1) * sizeof (struct gib_varray_s));
					memset (var->array + var->size, 0,
						(index + 1 - var->size) * sizeof (struct gib_varray_s));
					var->size = index + 1;
				} else
					return 0;
			}
			second = &zero;
			first = &var->array[index].leaves;
			start = i+1;
		} else if (i >= protect && (key->str[i] == '$' || key->str[i] == '#')) {
			n = i;
			if (GIB_Parse_Match_Var (key->str, &i))
				return 0;
			c = key->str[i];
			varstartskip = (c == '}');
			key->str[i] = 0;
			if ((var = GIB_Var_Get_Very_Complex (&one, &two, key, n+1+varstartskip, &index2, create))) {
				if (key->str[n] == '#')
					str = va (0, "%u", var->size);
				else
					str = var->array[index2].value->str;
				key->str[i] = c;
				dstring_replace (key, n, i-n+varstartskip, str, strlen (str));
				protect = n+strlen(str);
			} else if (key->str[n] == '#') {
				key->str[i] = c;
				dstring_replace (key, n, i-n+varstartskip, "0", 1);
				protect = n+1;
			} else if ((cvar = Cvar_FindVar (key->str+n+1+varstartskip))) {
				const char *cvar_str = Cvar_VarString (cvar);
				key->str[i] = c;
				dstring_replace (key, n, i-n+varstartskip, cvar_str,
								 strlen (cvar_str));
				protect = n+strlen(cvar_str);
			} else  {
				key->str[i] = c;
				dstring_snip (key, n, n-i+varstartskip);
				protect = 0;
			}
			i = n;
		}

	}
	if (!var->array[index].value)
		var->array[index].value = dstring_newstr ();
	*ind = index;
	return var;
}

void
GIB_Var_Assign (gib_var_t * var, unsigned int index, dstring_t ** values,
				unsigned int numv, qboolean shrink)
{
	unsigned int i, len;

	// Now, expand the array to the correct size
	len = numv + index;
	if (len >= var->size) {
		var->array = realloc (var->array, len * sizeof (struct gib_varray_s));
		memset (var->array + var->size, 0,
				(len - var->size) * sizeof (struct gib_varray_s));
		var->size = len;
	} else if (len < var->size && shrink) {
		for (i = len; i < var->size; i++) {
			if (var->array[i].value)
				dstring_delete (var->array[i].value);
			if (var->array[i].leaves)
				Hash_DelTable (var->array[i].leaves);
		}
		var->array = realloc (var->array, len * sizeof (struct gib_varray_s));
		var->size = len;
	}
	for (i = 0; i < numv; i++) {
		if (var->array[i + index].value)
			dstring_clearstr (var->array[i + index].value);
		else
			var->array[i + index].value = dstring_newstr ();
		dstring_appendstr (var->array[i + index].value, values[i]->str);
	}
}

static const char *
GIB_Domain_Get_Key (const void *ele, void *ptr)
{
	return ((gib_domain_t *) ele)->name;
}

static void
GIB_Domain_Free (void *ele, void *ptr)
{
	gib_domain_t *l = (gib_domain_t *) ele;

	Hash_DelTable (l->vars);
	free ((void *) l->name);
	free (l);
}

hashtab_t *
GIB_Domain_Get (const char *name)
{
	gib_domain_t *d = Hash_Find (gib_domains, name);

	if (!d) {
		d = calloc (1, sizeof (gib_domain_t));
		d->name = strdup (name);
		d->vars = Hash_NewTable (1024, GIB_Var_Get_Key, GIB_Var_Free, 0, 0);
		Hash_Add (gib_domains, d);
	}
	return d->vars;
}

hashtab_t *
GIB_Var_Hash_New (void)
{
	return Hash_NewTable (1024, GIB_Var_Get_Key, GIB_Var_Free, 0, 0);
}

void
GIB_Var_Init (void)
{
	gib_globals = Hash_NewTable (1024, GIB_Var_Get_Key, GIB_Var_Free, 0, 0);
	gib_domains = Hash_NewTable (1024, GIB_Domain_Get_Key,
								 GIB_Domain_Free, 0, 0);
}
