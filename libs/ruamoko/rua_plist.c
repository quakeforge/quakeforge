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
#include "QF/plist.h"
#include "QF/progs.h"

#include "rua_internal.h"

#define always_inline inline __attribute__((__always_inline__))

typedef struct bi_plist_s {
	struct bi_plist_s *next;
	struct bi_plist_s **prev;
	plitem_t   *plitem;
	int         users;
} bi_plist_t;

typedef struct {
	PR_RESMAP (bi_plist_t) plist_map;
	bi_plist_t *plists;
} plist_resources_t;

static bi_plist_t *
plist_new (plist_resources_t *res)
{
	return PR_RESNEW (res->plist_map);
}

static void
plist_free (plist_resources_t *res, bi_plist_t *plist)
{
	PR_RESFREE (res->plist_map, plist);
}

static void
plist_reset (plist_resources_t *res)
{
	PR_RESRESET (res->plist_map);
}

static inline bi_plist_t *
plist_get (plist_resources_t *res, unsigned index)
{
	return PR_RESGET(res->plist_map, index);
}

static inline int __attribute__((pure))
plist_index (plist_resources_t *res, bi_plist_t *plist)
{
	return PR_RESINDEX(res->plist_map, plist);
}

static void
bi_plist_clear (progs_t *pr, void *_res)
{
	plist_resources_t *res = (plist_resources_t *) _res;
	bi_plist_t *plist;

	for (plist = res->plists; plist; plist = plist->next) {
		PL_Release (plist->plitem);
	}
	res->plists = 0;

	plist_reset (res);
}

static void
bi_plist_destroy (progs_t *pr, void *_res)
{
	__auto_type res = (plist_resources_t *) _res;

	PR_RESDELMAP (res->plist_map);

	free (res);
}

static inline int
plist_handle (plist_resources_t *res, plitem_t *plitem)
{
	bi_plist_t *plist = PL_GetUserData (plitem);

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

	PL_Retain (plitem);
	plist->plitem = plitem;

	PL_SetUserData (plitem, plist);
	return plist_index (res, plist);
}

static inline void
plist_free_handle (plist_resources_t *res, bi_plist_t *plist)
{
	PL_SetUserData (plist->plitem, 0);
	PL_Release (plist->plitem);

	*plist->prev = plist->next;
	if (plist->next)
		plist->next->prev = plist->prev;
	plist_free (res, plist);
}

static always_inline bi_plist_t * __attribute__((pure))
get_plist (progs_t *pr, plist_resources_t *res, const char *name, int handle)
{
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

	if (!plitem)
		return 0;

	handle = plist_handle (res, plitem);
	if (!handle) {
		// we're taking ownership of the plitem, but we have nowhere to store
		// it, so we have to lose it. However, in this situation, we have worse
		// things to worry about.
		PL_Release (plitem);
		return 0;
	}

	return handle;
}

static void
bi_pl_getfromfile (progs_t *pr, plist_resources_t *res,
				   plitem_t *get (const char *, struct hashctx_s **))
{
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

	plitem = get (buf, pr->hashctx);
	free (buf);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_pl_get (progs_t *pr, plist_resources_t *res,
		   plitem_t *get (const char *, struct hashctx_s **))
{
	plitem_t   *plitem = get (P_GSTRING (pr, 0), pr->hashctx);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_GetFromFile (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_getfromfile (pr, res, PL_GetPropertyList);
}

static void
bi_PL_GetPropertyList (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_get (pr, res, PL_GetPropertyList);
}

static void
bi_PL_GetDictionaryFromFile (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_getfromfile (pr, res, PL_GetDictionary);
}

static void
bi_PL_GetDictionary (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_get (pr, res, PL_GetDictionary);
}

static void
bi_PL_GetArrayFromFile (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_getfromfile (pr, res, PL_GetArray);
}

static void
bi_PL_GetArray (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	bi_pl_get (pr, res, PL_GetArray);
}

static void
bi_PL_WritePropertyList (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	char       *pl = PL_WritePropertyList (plist->plitem);

	R_STRING (pr) = PR_SetDynamicString (pr, pl);
	free (pl);
}

static void
bi_PL_Type (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	R_INT (pr) = PL_Type (plist->plitem);
}

static void
bi_PL_Line (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	R_INT (pr) = PL_Line (plist->plitem);
}

static void
bi_PL_String (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	const char *str = PL_String (plist->plitem);

	RETURN_STRING (pr, str);
}

static void
bi_PL_ObjectForKey (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	const char *key = P_GSTRING (pr, 1);
	plitem_t   *plitem = PL_ObjectForKey (plist->plitem, key);

	R_INT (pr) = 0;
	if (!plitem)
		return;
	R_INT (pr) = plist_handle (res, plitem);
}

static void
bi_PL_RemoveObjectForKey (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	const char *key = P_GSTRING (pr, 1);
	PL_RemoveObjectForKey (plist->plitem, key);
}

static void
bi_PL_ObjectAtIndex (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	int         ind = P_INT (pr, 1);
	plitem_t   *plitem = PL_ObjectAtIndex (plist->plitem, ind);

	R_INT (pr) = 0;
	if (!plitem)
		return;
	R_INT (pr) = plist_handle (res, plitem);
}

static void
bi_PL_D_AllKeys (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	plitem_t   *plitem = PL_D_AllKeys (plist->plitem);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_D_NumKeys (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	R_INT (pr) = PL_D_NumKeys (plist->plitem);
}

static void
bi_PL_KeyAtIndex (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	int         index = P_INT (pr, 1);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	const char *key = PL_KeyAtIndex (plist->plitem, index);

	RETURN_STRING (pr, key);
}

static void
bi_PL_D_AddObject (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         dict_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 2);
	bi_plist_t *dict = get_plist (pr, res, __FUNCTION__, dict_handle);
	const char *key = P_GSTRING (pr, 1);
	bi_plist_t *obj = get_plist (pr, res, __FUNCTION__, obj_handle);

	R_INT (pr) = PL_D_AddObject (dict->plitem, key, obj->plitem);
}

static void
bi_PL_A_AddObject (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         arr_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 1);
	bi_plist_t *arr = get_plist (pr, res, __FUNCTION__, arr_handle);
	bi_plist_t *obj = get_plist (pr, res, __FUNCTION__, obj_handle);

	R_INT (pr) = PL_A_AddObject (arr->plitem, obj->plitem);
}

static void
bi_PL_A_NumObjects (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	R_INT (pr) = PL_A_NumObjects (plist->plitem);
}

static void
bi_PL_A_InsertObjectAtIndex (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         dict_handle = P_INT (pr, 0);
	int         obj_handle = P_INT (pr, 1);
	bi_plist_t *arr = get_plist (pr, res, __FUNCTION__, dict_handle);
	bi_plist_t *obj = get_plist (pr, res, __FUNCTION__, obj_handle);
	int         ind = P_INT (pr, 2);

	R_INT (pr) = PL_A_InsertObjectAtIndex (arr->plitem, obj->plitem, ind);
}

static void
bi_PL_RemoveObjectAtIndex (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	int         ind = P_INT (pr, 1);
	PL_RemoveObjectAtIndex (plist->plitem, ind);
}

static void
bi_PL_NewDictionary (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	plitem_t   *plitem = PL_NewDictionary (pr->hashctx);

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewArray (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	plitem_t   *plitem = PL_NewArray ();

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewData (progs_t *pr, void *_res)
{
	//FIXME not safe
	plist_resources_t *res = _res;
	plitem_t   *plitem = PL_NewData (P_GPOINTER (pr, 0), P_INT (pr, 1));

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_NewString (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	plitem_t   *plitem = PL_NewString (P_GSTRING (pr, 0));

	R_INT (pr) = plist_retain (res, plitem);
}

static void
bi_PL_Release (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	if (!(plist->users && --plist->users > 0)) {
		plist_free_handle (res, plist);
		handle = 0;
	}
	R_INT (pr) = handle;
}

static void
bi_PL_Retain (progs_t *pr, void *_res)
{
	plist_resources_t *res = _res;
	int         handle = P_INT (pr, 0);
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);

	plist->users++;
	R_INT (pr) = handle;
}

plitem_t *
Plist_GetItem (progs_t *pr, int handle)
{
	plist_resources_t *res = PR_Resources_Find (pr, "plist");
	bi_plist_t *plist = get_plist (pr, res, __FUNCTION__, handle);
	return plist->plitem;
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(PL_GetFromFile,           1, p(ptr)),
	bi(PL_GetPropertyList,       1, p(string)),
	bi(PL_GetDictionary,         1, p(string)),
	bi(PL_GetDictionaryFromFile, 1, p(string)),
	bi(PL_GetArray,              1, p(string)),
	bi(PL_GetArrayFromFile,      1, p(string)),
	bi(PL_WritePropertyList,     1, p(ptr)),
	bi(PL_Type,                  1, p(ptr)),
	bi(PL_Line,                  1, p(ptr)),
	bi(PL_String,                1, p(ptr)),
	bi(PL_ObjectForKey,          2, p(ptr), p(string)),
	bi(PL_RemoveObjectForKey,    2, p(ptr), p(string)),
	bi(PL_ObjectAtIndex,         2, p(ptr), p(int)),
	bi(PL_D_AllKeys,             1, p(ptr)),
	bi(PL_D_NumKeys,             1, p(ptr)),
	bi(PL_KeyAtIndex,            2, p(ptr), p(int)),
	bi(PL_D_AddObject,           3, p(ptr), p(string), p(ptr)),
	bi(PL_A_AddObject,           2, p(ptr), p(ptr)),
	bi(PL_A_NumObjects,          1, p(ptr)),
	bi(PL_A_InsertObjectAtIndex, 3, p(ptr), p(ptr), p(int)),
	bi(PL_RemoveObjectAtIndex,   2, p(ptr), p(int)),
	bi(PL_NewDictionary,         0),
	bi(PL_NewArray,              0),
	bi(PL_NewData,               2, p(ptr), p(int)),
	bi(PL_NewString,             1, p(string)),
	bi(PL_Release,               1, p(ptr)),
	bi(PL_Retain,                1, p(ptr)),
	{0}
};

void
RUA_Plist_Init (progs_t *pr, int secure)
{
	plist_resources_t *res = calloc (1, sizeof (plist_resources_t));

	PR_Resources_Register (pr, "plist", res, bi_plist_clear, bi_plist_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
