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

static const char rcsid[] =
        "$Id$";

#include <string.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/gib_vars.h"
#include "QF/gib_buffer.h"
        
hashtab_t *gib_globals = 0;

gib_var_t *
GIB_Var_New (void)
{
	gib_var_t *new = calloc (1, sizeof (gib_var_t));
	new->key = dstring_newstr();
	new->value = dstring_newstr();
	
	return new;
}

const char *
GIB_Var_Get_Key (void *ele, void *ptr)
{
	return ((gib_var_t *)ele)->key->str;
}

void
GIB_Var_Free (void *ele, void *ptr)
{
	gib_var_t *l = (gib_var_t *)ele;
	dstring_delete (l->key);
	dstring_delete (l->value);
	if (l->subvars)
		Hash_DelTable (l->subvars);
}

gib_var_t *
GIB_Var_Get_R (hashtab_t *vars, char *name)
{
	char *p;
	gib_var_t *l;
	
	if (!vars)
		return 0;
	if ((p = strchr (name, '.'))) {
		*p = 0;
		l = Hash_Find (vars, name);
		*p = '.';
		if (!l || !l->subvars)
			return 0;
		return GIB_Var_Get_R (l->subvars, p+1);
	} else
		return Hash_Find (vars, name);
}

void
GIB_Var_Set_R (hashtab_t *vars, char *name, const char *value)
{
	char *p;
	gib_var_t *l;
	
	if ((p = strchr (name, '.'))) {
		*p = 0;
		if (!(l = Hash_Find (vars, name))) {
			l = GIB_Var_New ();
			dstring_appendstr (l->key, name);
			Hash_Add (vars, l);
		}
		*p = '.';
		if (!l->subvars)
			l->subvars = Hash_NewTable (256, GIB_Var_Get_Key, GIB_Var_Free, 0);
		GIB_Var_Set_R (l->subvars, p+1, value);
	} else {
		if ((l = Hash_Find (vars, name)))
			dstring_clearstr (l->value);
		else {
			l = GIB_Var_New ();
			dstring_appendstr (l->key, name);
			Hash_Add (vars, l);
		}
		dstring_appendstr (l->value, value);
	}
}
	
void
GIB_Var_Set_Local (cbuf_t *cbuf, const char *key, const char *value)
{
	char *k = strdup (key);
	if (!GIB_DATA(cbuf)->locals) {
		GIB_DATA(cbuf)->locals = Hash_NewTable (256, GIB_Var_Get_Key, GIB_Var_Free, 0);
		if (GIB_DATA(cbuf)->type != GIB_BUFFER_NORMAL)
				GIB_DATA(cbuf->up)->locals = GIB_DATA(cbuf)->locals;
	}
	GIB_Var_Set_R (GIB_DATA(cbuf)->locals, k, value);
	free(k);
}

void
GIB_Var_Set_Global (const char *key, const char *value)
{
	char *k = strdup (key);
	GIB_Var_Set_R (gib_globals, k, value);
	free (k);
}

const char *
GIB_Var_Get_Local (cbuf_t *cbuf, const char *key)
{
	gib_var_t *l;
	char *k;
	if (!GIB_DATA(cbuf)->locals)
		return 0;
	k = strdup(key);
	l = GIB_Var_Get_R (GIB_DATA(cbuf)->locals, k);
	free(k);
	if (l)
		return l->value->str;
	else
		return 0;
}

const char *
GIB_Var_Get_Global (const char *key)
{
	gib_var_t *l;
	char *k = strdup (key);
	l = GIB_Var_Get_R (gib_globals, k);
	free (k);
	if (l)
		return l->value->str;
	else
		return 0;
}

const char *
GIB_Var_Get (cbuf_t *cbuf, char *key)
{
	const char *v;
	
	if ((v = GIB_Var_Get_Local (cbuf, key)) || (v = GIB_Var_Get_Global (key)))
		return v;
	else
		return 0;
}

void
GIB_Var_Set (cbuf_t *cbuf, char *key, const char *value)
{
	int glob = 0;
	char *c = 0;
	if ((c = strchr (key, '.'))) // Only check stem
		*c = 0;
	glob = (!GIB_Var_Get_Local (cbuf, key) && GIB_Var_Get_Global (key));
	if (c)
		*c = '.';
	if (glob)
		GIB_Var_Set_Global (key, value); // Set the global
	else
		GIB_Var_Set_Local (cbuf, key, value); // Set the local
}

void
GIB_Var_Free_Global (const char *key)
{
	char *p, *k;
	gib_var_t *root;
	void *del;
	k = strdup (key);
	if ((p = strrchr (k, '.'))) {
		*p = 0;
		if ((root = GIB_Var_Get_R (gib_globals, k))) {
			del = Hash_Del (root->subvars, p+1);
			if (del)
				GIB_Var_Free (del, 0);
		}
	} else {
		del = Hash_Del (gib_globals, k);
		if (del)
			GIB_Var_Free (del, 0);
	}
	free (k);
}
		
