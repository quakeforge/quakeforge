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

#define always_inline inline __attribute__((__always_inline__))

typedef struct bi_plist_s {
	struct bi_plist_s *next;
	struct bi_plist_s **prev;
	plitem_t   *plitem;
	int         own;
} bi_plist_t;

typedef struct {
	PR_RESMAP (bi_plist_t) plist_map;
	bi_plist_t *plists;
	hashtab_t  *plist_tab;
} plist_resources_t;

static bi_plist_t *
plist_new (plist_resources_t *res)
{
	PR_RESNEW (bi_plist_t, res->plist_map);
}

static void
plist_free (plist_resources_t *res, bi_plist_t *plist)
{
	PR_RESFREE (bi_plist_t, res->plist_map, plist);
}

static void
plist_reset (plist_resources_t *res)
{
	PR_RESRESET (bi_plist_t, res->plist_map);
}

static inline bi_plist_t *
plist_get (plist_resources_t *res, unsigned index)
{
	PR_RESGET(res->plist_map, index);
}

static inline int __attribute__((pure))
plist_index (plist_resources_t *res, bi_plist_t *plist)
{
	PR_RESINDEX(res->plist_map, plist);
}

static void
bi_plist_clear (progs_t *pr, void *data)
{
	plist_resources_t *res = (plist_resources_t *) data;
	bi_plist_t *plist;

	for (plist = res->plists; plist; plist = plist->next) {
		if (plist->own)
			PL_Free (plist->plitem);
	}
	res->plists = 0;

	Hash_FlushTable (res->plist_tab);
	plist_reset (res);
}

static inline int
plist_handle (plist_resources_t *res, plitem_t *plitem)
{
	bi_plist_t  dummy = {0, 0, plitem, 0};
	bi_plist_t *plist = Hash_FindElement (res->plist_tab, &dummy);

	if (plist)
		return plist_index (res, plist);

	plist = plist_new (res);

	if (!plist)
		return 0;

	plist->next = res->plists;
	plist->prev = &res->plists;
	if (res->plists)
		res->plists->prev = &plist->next;
	res->plists = plist;

	plist->plitem = plitem;

	Hash_AddElement (res->plist_tab, plist);
	return plist_index (res, plist);
}

static inline void
plist_free_handle (plist_resources_t *res, bi_plist_t *plist)
{
	Hash_DelElement (res->plist_tab, plist);
	*plist->prev = plist->next;
	if (plist->next)
		plist->next->prev = plist->prev;
	plist_free (res, plist);
}

static always_inline bi_plist_t *
get_plist (progs_t *pr, const char *name, int handle)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	bi_plist_t *plist = plist_get (res, handle);

	// plist->prev will be null if the handle is unallocated
	if (!plist || !plist->prev)
		PR_RunError (pr, "invalid plist passed to %s", name + 3);
	return plist;
}

static inline int
plist_retain (plist_resources_t *res, plitem_t *plitem)
{
	int         handle;
	bi_plist_t *plist;

	if (!plitem)
		return 0;

	handle = plist_handle (res, plitem);
	if (!handle) {
		// we're taking ownership of the plitem, but we have nowhere to store
		// it, so we have to lose it. However, in this situation, we have worse
		// things to worry about.
		PL_Free (plitem);
		return 0;
	}

	plist = plist_get (res, handle);
	plist->own = 1;
	return handle;
}

static void
bi_PL_GetFromFile (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	QFile      *file = QFile_GetFile (pr, P_INT (pr, 0));
	plitem_t   *plitem;
	long        offset;
	long        size;
	long		len;
	char       *buf;

	offset = Qtell (file);
	size = Qfilesize (file);
	len = size - offset;
	buf = malloc (len + 1);
	Qread (file, buf, len);
	buf[len] = 0;

	plitem = PL_GetPropertyList (buf);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_GetPropertyList (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	plitem_t   *plitem = PL_GetPropertyList (P_GSTRING (pr, 0));

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_WritePropertyList (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	char       *pl = PL_WritePropertyList (plist->plitem);

	R_STRING (pr) = PR_SetDynamicString (pr, pl);
	free (pl);
}

static void
bi_PL_Type (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);

	R_INT (pr) = PL_Type (plist->plitem);
}

static void
bi_PL_Line (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);

	R_INT (pr) = PL_Line (plist->plitem);
}

static void
bi_PL_String (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	const char *str = PL_String (plist->plitem);

	RETURN_STRING (pr, str);
}

static void
bi_PL_ObjectForKey (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	const char *key = P_GSTRING (pr, 1);
	plitem_t   *plitem = PL_ObjectForKey (plist->plitem, key);

	R_INT (pr) = 0;
	if (!plitem)
		return;
	R_INT (pr) = plist_handle (res, plitem);
}

static void
bi_PL_RemoveObjectForKey (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	const char *key = P_GSTRING (pr, 1);
	plitem_t   *plitem = PL_RemoveObjectForKey (plist->plitem, key);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_ObjectAtIndex (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	int         ind = P_INT (pr, 1);
	plitem_t   *plitem = PL_ObjectAtIndex (plist->plitem, ind);

	R_INT (pr) = 0;
	if (!plitem)
		return;
	R_INT (pr) = plist_handle (res, plitem);
}

static void
bi_PL_D_AllKeys (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	plitem_t   *plitem = PL_D_AllKeys (plist->plitem);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_D_NumKeys (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);

	R_INT (pr) = PL_D_NumKeys (plist->plitem);
}

static void
bi_PL_D_AddObject (progs_t *pr)
{
	int         dict_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 2);
	bi_plist_t *dict = get_plist (pr, __FUNCTION__, dict_handle);
	const char *key = P_GSTRING (pr, 1);
	bi_plist_t *obj = get_plist (pr, __FUNCTION__, obj_handle);

	obj->own = 0;
	R_INT (pr) = PL_D_AddObject (dict->plitem, key, obj->plitem);
}

static void
bi_PL_A_AddObject (progs_t *pr)
{
	int         arr_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 1);
	bi_plist_t *arr = get_plist (pr, __FUNCTION__, arr_handle);
	bi_plist_t *obj = get_plist (pr, __FUNCTION__, obj_handle);

	obj->own = 0;
	R_INT (pr) = PL_A_AddObject (arr->plitem, obj->plitem);
}

static void
bi_PL_A_NumObjects (progs_t *pr)
{
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);

	R_INT (pr) = PL_A_NumObjects (plist->plitem);
}

static void
bi_PL_A_InsertObjectAtIndex (progs_t *pr)
{
	int         dict_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 1);
	bi_plist_t *arr = get_plist (pr, __FUNCTION__, dict_handle);
	bi_plist_t *obj = get_plist (pr, __FUNCTION__, obj_handle);
	int         ind = P_INT (pr, 2);

	obj->own = 0;
	R_INT (pr) = PL_A_InsertObjectAtIndex (arr->plitem, obj->plitem, ind);
}

static void
bi_PL_RemoveObjectAtIndex (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);
	int         ind = P_INT (pr, 1);
	plitem_t   *plitem = PL_RemoveObjectAtIndex (plist->plitem, ind);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewDictionary (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	plitem_t   *plitem = PL_NewDictionary ();

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewArray (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	plitem_t   *plitem = PL_NewArray ();

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewData (progs_t *pr)
{
	//FIXME not safe
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	plitem_t   *plitem = PL_NewData (P_GPOINTER (pr, 0), P_INT (pr, 1));

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewString (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	plitem_t   *plitem = PL_NewString (P_GSTRING (pr, 0));

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_Free (progs_t *pr)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, __FUNCTION__, handle);

	if (!plist->own)
		PR_RunError (pr, "attempt to free unowned plist");

	PL_Free (plist->plitem);

	plist_free_handle (res, plist);
}

static uintptr_t
plist_get_hash (const void *key, void *unused)
{
	bi_plist_t *plist = (bi_plist_t *) key;
	return (uintptr_t) plist->plitem;
}

static int
plist_compare (const void *k1, const void *k2, void *unused)
{
	bi_plist_t *pl1 = (bi_plist_t *) k1;
	bi_plist_t *pl2 = (bi_plist_t *) k2;
	return pl1->plitem == pl2->plitem;
}

static builtin_t builtins[] = {
	{"PL_GetFromFile",				bi_PL_GetFromFile,				-1},
	{"PL_GetPropertyList",			bi_PL_GetPropertyList,			-1},
	{"PL_WritePropertyList",		bi_PL_WritePropertyList,		-1},
	{"PL_Type",						bi_PL_Type,						-1},
	{"PL_Line",						bi_PL_Line,						-1},
	{"PL_String",					bi_PL_String,					-1},
	{"PL_ObjectForKey",				bi_PL_ObjectForKey,				-1},
	{"PL_RemoveObjectForKey",		bi_PL_RemoveObjectForKey,		-1},
	{"PL_ObjectAtIndex",			bi_PL_ObjectAtIndex,			-1},
	{"PL_D_AllKeys",				bi_PL_D_AllKeys,				-1},
	{"PL_D_NumKeys",				bi_PL_D_NumKeys,				-1},
	{"PL_D_AddObject",				bi_PL_D_AddObject,				-1},
	{"PL_A_AddObject",				bi_PL_A_AddObject,				-1},
	{"PL_A_NumObjects",				bi_PL_A_NumObjects,				-1},
	{"PL_A_InsertObjectAtIndex",	bi_PL_A_InsertObjectAtIndex,	-1},
	{"PL_RemoveObjectAtIndex",		bi_PL_RemoveObjectAtIndex,		-1},
	{"PL_NewDictionary",			bi_PL_NewDictionary,			-1},
	{"PL_NewArray",					bi_PL_NewArray,					-1},
	{"PL_NewData",					bi_PL_NewData,					-1},
	{"PL_NewString",				bi_PL_NewString,				-1},
	{"PL_Free",						bi_PL_Free,						-1},
	{0}
};

void
RUA_Plist_Init (progs_t *pr, int secure)
{
	plist_resources_t *res = calloc (1, sizeof (plist_resources_t));
	res->plist_tab = Hash_NewTable (1021, 0, 0, 0, pr->hashlink_freelist);
	Hash_SetHashCompare (res->plist_tab, plist_get_hash, plist_compare);

	PR_Resources_Register (pr, "plist", res, bi_plist_clear);
	PR_RegisterBuiltins (pr, builtins);
}
