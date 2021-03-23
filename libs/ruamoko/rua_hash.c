/*
	bi_hash.c

	QuakeC hash table api

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

#include "rua_internal.h"

typedef struct bi_hashtab_s {
	struct bi_hashtab_s *next;
	struct bi_hashtab_s **prev;
	progs_t    *pr;
	hashtab_t  *tab;
	func_t      gk;
	func_t      gh;
	func_t      cmp;
	func_t      f;
	pointer_t   ud;
} bi_hashtab_t;

typedef struct {
	PR_RESMAP (bi_hashtab_t) table_map;
	bi_hashtab_t *tabs;
} hash_resources_t;

static bi_hashtab_t *
table_new (hash_resources_t *res)
{
	return PR_RESNEW (res->table_map);
}

static void
table_free (hash_resources_t *res, bi_hashtab_t *table)
{
	PR_RESFREE (res->table_map, table);
}

static void
table_reset (hash_resources_t *res)
{
	PR_RESRESET (res->table_map);
}

static inline bi_hashtab_t *
table_get (hash_resources_t *res, int index)
{
	return PR_RESGET(res->table_map, index);
}

static inline int __attribute__((pure))
table_index (hash_resources_t *res, bi_hashtab_t *table)
{
	return PR_RESINDEX(res->table_map, table);
}

static const char *
bi_get_key (const void *key, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->gk);
	return PR_GetString (ht->pr, R_STRING (ht->pr));
}

static uintptr_t
bi_get_hash (const void *key, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->gh);
	return R_INT (ht->pr);
}

static int
bi_compare (const void *key1, const void *key2, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key1);
	P_INT (ht->pr, 1) = (intptr_t) (key2);
	P_INT (ht->pr, 2) = ht->ud;
	ht->pr->pr_argc = 3;
	PR_ExecuteProgram (ht->pr, ht->cmp);
	return R_INT (ht->pr);
}

static void
bi_free (void *key, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->f);
}

static void
bi_Hash_NewTable (progs_t *pr)
{
	hash_resources_t *res = PR_Resources_Find (pr, "Hash");
	int         tsize = P_INT (pr, 0);
	const char *(*gk)(const void*,void*);
	void        (*f)(void*,void*);
	bi_hashtab_t *ht;

	ht = table_new (res);
	ht->pr = pr;
	ht->gk = P_FUNCTION (pr, 1);
	ht->f = P_FUNCTION (pr, 2);
	ht->ud = P_INT (pr, 3);		// don't convert pointer for speed reasons

	ht->next = res->tabs;
	ht->prev = &res->tabs;
	if (res->tabs)
		res->tabs->prev = &ht->next;
	res->tabs = ht;

	gk = ht->gk ? bi_get_key : 0;
	f = ht->f ? bi_free : 0;
	ht->tab = Hash_NewTable (tsize, gk, f, ht, pr->hashlink_freelist);
	R_INT (pr) = table_index (res, ht);
}

static bi_hashtab_t *
get_table (progs_t *pr, const char *name, int index)
{
	hash_resources_t *res = PR_Resources_Find (pr, "Hash");
	bi_hashtab_t *ht = table_get (res, index);

	if (!ht)
		PR_RunError (pr, "invalid hash table index passed to %s", name + 3);
	return ht;
}

static void
bi_Hash_SetHashCompare (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));
	uintptr_t   (*gh)(const void*,void*);
	int         (*cmp)(const void*,const void*,void*);

	ht->gh = P_FUNCTION (pr, 1);
	ht->cmp = P_FUNCTION (pr, 2);
	gh = ht->gh ? bi_get_hash : 0;
	cmp = ht->cmp ? bi_compare : 0;
	Hash_SetHashCompare (ht->tab, gh, cmp);
}

static void
bi_Hash_DelTable (progs_t *pr)
{
	hash_resources_t *res = PR_Resources_Find (pr, "Hash");
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	Hash_DelTable (ht->tab);
	*ht->prev = ht->next;
	if (ht->next)
		ht->next->prev = ht->prev;
	table_free (res, ht);
}

static void
bi_Hash_FlushTable (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	Hash_FlushTable (ht->tab);
}

static void
bi_Hash_Add (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = Hash_Add (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_AddElement (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = Hash_AddElement (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_Find (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_Find (ht->tab, P_GSTRING (pr, 1));
}

static void
bi_Hash_FindElement (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_FindElement (ht->tab,
										  (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_FindList (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_FindList (ht->tab, P_GSTRING (pr, 1));
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		pr_list[count++].integer_var = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_FindElementList (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_FindElementList (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		pr_list[count++].integer_var = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_Del (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_Del (ht->tab, P_GSTRING (pr, 1));
}

static void
bi_Hash_DelElement (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_DelElement (ht->tab,
										 (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_Free (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	Hash_Free (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_String (progs_t *pr)
{
	R_INT (pr) = Hash_String (P_GSTRING (pr, 0));
}

static void
bi_Hash_Buffer (progs_t *pr)
{
	R_INT (pr) = Hash_Buffer (P_GPOINTER (pr, 0), P_INT (pr, 1));
}

static void
bi_Hash_GetList (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_GetList (ht->tab);
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		pr_list[count++].integer_var = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_Stats (progs_t *pr)
{
	bi_hashtab_t *ht = get_table (pr, __FUNCTION__, P_INT (pr, 0));

	Hash_Stats (ht->tab);
}

static void
bi_hash_clear (progs_t *pr, void *data)
{
	hash_resources_t *res = (hash_resources_t *) data;
	bi_hashtab_t *ht;

	for (ht = res->tabs; ht; ht = ht->next)
		Hash_DelTable (ht->tab);
	res->tabs = 0;
	table_reset (res);
}

static builtin_t builtins[] = {
	{"Hash_NewTable",			bi_Hash_NewTable,			-1},
	{"Hash_SetHashCompare",		bi_Hash_SetHashCompare,		-1},
	{"Hash_DelTable",			bi_Hash_DelTable,			-1},
	{"Hash_FlushTable",			bi_Hash_FlushTable,			-1},
	{"Hash_Add",				bi_Hash_Add,				-1},
	{"Hash_AddElement",			bi_Hash_AddElement,			-1},
	{"Hash_Find",				bi_Hash_Find,				-1},
	{"Hash_FindElement",		bi_Hash_FindElement,		-1},
	{"Hash_FindList",			bi_Hash_FindList,			-1},
	{"Hash_FindElementList",	bi_Hash_FindElementList,	-1},
	{"Hash_Del",				bi_Hash_Del,				-1},
	{"Hash_DelElement",			bi_Hash_DelElement,			-1},
	{"Hash_Free",				bi_Hash_Free,				-1},
	{"Hash_String",				bi_Hash_String,				-1},
	{"Hash_Buffer",				bi_Hash_Buffer,				-1},
	{"Hash_GetList",			bi_Hash_GetList,			-1},
	{"Hash_Stats",				bi_Hash_Stats,				-1},
	{0}
};

void
RUA_Hash_Init (progs_t *pr, int secure)
{
	hash_resources_t *res = calloc (1, sizeof (hash_resources_t));
	res->tabs = 0;

	PR_Resources_Register (pr, "Hash", res, bi_hash_clear);
	PR_RegisterBuiltins (pr, builtins);
}
