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

#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/gib_buffer.h"

gib_local_t *
GIB_Local_New (void)
{
	gib_local_t *new = calloc (1, sizeof (gib_local_t));
	new->key = dstring_newstr();
	new->value = dstring_newstr();
	
	return new;
}

const char *
GIB_Local_Get_Key (void *ele, void *ptr)
{
	return ((gib_local_t *)ele)->key->str;
}

void
GIB_Local_Free (void *ele, void *ptr)
{
	gib_local_t *l = (gib_local_t *)ele;
	dstring_delete (l->key);
	dstring_delete (l->value);
}

void
GIB_Local_Set (cbuf_t *cbuf, const char *key, const char *value)
{
	gib_local_t *l;
	if (!GIB_DATA(cbuf)->locals)
		GIB_DATA(cbuf)->locals = Hash_NewTable (256, GIB_Local_Get_Key, GIB_Local_Free, 0);
	if ((l = Hash_Find (GIB_DATA(cbuf)->locals, key)))
		dstring_clearstr (l->value);
	else {
		l = GIB_Local_New ();
		dstring_appendstr (l->key, key);
		Hash_Add (GIB_DATA(cbuf)->locals, l);
	}
	dstring_appendstr (l->value, value);
}

const char *
GIB_Local_Get (cbuf_t *cbuf, const char *key)
{
	gib_local_t *l;
	
	if (!GIB_DATA(cbuf)->locals)
		return 0;
	if (!(l = Hash_Find (GIB_DATA(cbuf)->locals, key)))
		return 0;
	return l->value->str;
}

void GIB_Buffer_Construct (struct cbuf_s *cbuf)
{
	cbuf->data = calloc (1, sizeof (gib_buffer_data_t));
	GIB_DATA (cbuf)->arg_composite = dstring_newstr ();
	GIB_DATA (cbuf)->current_token = dstring_newstr ();
}

void GIB_Buffer_Destruct (struct cbuf_s *cbuf)
{
	dstring_delete (GIB_DATA (cbuf)->arg_composite);
	dstring_delete (GIB_DATA (cbuf)->current_token);
	if (GIB_DATA(cbuf)->locals)
		Hash_DelTable (GIB_DATA(cbuf)->locals);
	free (cbuf->data);
}
