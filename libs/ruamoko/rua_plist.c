/*
	bi_plist.c

	QuakeC plist api

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/4/7

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qfplist.h"

#include "rua_internal.h"

typedef struct {
	hashtab_t  *items;
} plist_resources_t;

static inline void
return_plitem (progs_t *pr, plitem_t *plitem)
{
	memcpy (&R_INT (pr), &plitem, sizeof (plitem));
}

static inline plitem_t *
p_plitem (progs_t *pr, int n)
{
	plitem_t   *plitem;
	memcpy (&plitem, &P_INT (pr, n), sizeof (plitem));
	return plitem;
}

static inline plitem_t *
record_plitem (progs_t *pr, plitem_t *plitem)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	if (plitem)
		Hash_AddElement (res->items, plitem);
	return plitem;
}

static inline plitem_t *
remove_plitem (progs_t *pr, plitem_t *plitem)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	
	Hash_DelElement (res->items, plitem);
	return plitem;
}

static void
bi_PL_GetPropertyList (progs_t *pr)
{
	plitem_t   *plitem = PL_GetPropertyList (P_GSTRING (pr, 0));	

	return_plitem (pr, record_plitem (pr, plitem));
}

static void
bi_PL_ObjectForKey (progs_t *pr)
{
	return_plitem (pr, PL_ObjectForKey (p_plitem (pr, 0), P_GSTRING (pr, 1)));
}

static void
bi_PL_ObjectAtIndex (progs_t *pr)
{
	return_plitem (pr, PL_ObjectAtIndex (p_plitem (pr, 0), P_INT (pr, 1)));
}

static void
bi_PL_D_AddObject (progs_t *pr)
{
	R_INT (pr) = PL_D_AddObject (p_plitem (pr, 0),
								 remove_plitem (pr, p_plitem (pr, 1)),
								 remove_plitem (pr, p_plitem (pr, 2)));
}

static void
bi_PL_A_AddObject (progs_t *pr)
{
	R_INT (pr) = PL_A_AddObject (p_plitem (pr, 0),
								 remove_plitem (pr, p_plitem (pr, 1)));
}

static void
bi_PL_A_InsertObjectAtIndex (progs_t *pr)
{
	R_INT (pr) = PL_A_InsertObjectAtIndex (p_plitem (pr, 0),
									   remove_plitem (pr, p_plitem (pr, 1)),
									   P_INT (pr, 2));
}

static void
bi_PL_NewDictionary (progs_t *pr)
{
	return_plitem (pr, record_plitem (pr, PL_NewDictionary ()));
}

static void
bi_PL_NewArray (progs_t *pr)
{
	return_plitem (pr, record_plitem (pr, PL_NewArray ()));
}
/*
static void
bi_PL_NewData (progs_t *pr)
{
}
*/
static void
bi_PL_NewString (progs_t *pr)
{
	return_plitem (pr, record_plitem (pr, PL_NewString (P_GSTRING (pr, 0))));
}

static void
bi_plist_clear (progs_t *pr, void *data)
{
	plist_resources_t *res = (plist_resources_t *) data;

	Hash_FlushTable (res->items);
}

static unsigned long
plist_get_hash (void *key, void *unused)
{
	return (unsigned long) key;
}

static int
plist_compare (void *k1, void *k2, void *unused)
{
	return k1 == k2;
}

static builtin_t builtins[] = {
	{"PL_GetPropertyList",			bi_PL_GetPropertyList,			-1},
	{"PL_ObjectForKey",				bi_PL_ObjectForKey,				-1},
	{"PL_ObjectAtIndex",			bi_PL_ObjectAtIndex,			-1},
	{"PL_D_AddObject",				bi_PL_D_AddObject,				-1},
	{"PL_A_AddObject",				bi_PL_A_AddObject,				-1},
	{"PL_A_InsertObjectAtIndex",	bi_PL_A_InsertObjectAtIndex,	-1},
	{"PL_NewDictionary",			bi_PL_NewDictionary,			-1},
	{"PL_NewArray",					bi_PL_NewArray,					-1},
//	{"PL_NewData",					bi_PL_NewData,					-1},
	{"PL_NewString",				bi_PL_NewString,				-1},
	{0}
};

void
RUA_Plist_Init (progs_t *pr, int secure)
{
	plist_resources_t *res = malloc (sizeof (plist_resources_t));
	res->items = Hash_NewTable (1021, 0, 0, 0);
	Hash_SetHashCompare (res->items, plist_get_hash, plist_compare);

	PR_Resources_Register (pr, "plist", res, bi_plist_clear);
	PR_RegisterBuiltins (pr, builtins);
}
