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
	pr_func_t   gk;
	pr_func_t   gh;
	pr_func_t   cmp;
	pr_func_t   f;
	pr_ptr_t    ud;
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
	PR_PushFrame (ht->pr);
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->gk);
	pr_string_t string = R_STRING (ht->pr);
	PR_PopFrame (ht->pr);
	return PR_GetString (ht->pr, string);
}

static uintptr_t
bi_get_hash (const void *key, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_PushFrame (ht->pr);
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->gh);
	int         hash = R_INT (ht->pr);
	PR_PopFrame (ht->pr);
	return hash;
}

static int
bi_compare (const void *key1, const void *key2, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_PushFrame (ht->pr);
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key1);
	P_INT (ht->pr, 1) = (intptr_t) (key2);
	P_INT (ht->pr, 2) = ht->ud;
	ht->pr->pr_argc = 3;
	PR_ExecuteProgram (ht->pr, ht->cmp);
	int         cmp = R_INT (ht->pr);
	PR_PopFrame (ht->pr);
	return cmp;
}

static void
bi_free (void *key, void *_ht)
{
	bi_hashtab_t *ht = (bi_hashtab_t *)_ht;
	PR_PushFrame (ht->pr);
	PR_RESET_PARAMS (ht->pr);
	P_INT (ht->pr, 0) = (intptr_t) (key);
	P_INT (ht->pr, 1) = ht->ud;
	ht->pr->pr_argc = 2;
	PR_ExecuteProgram (ht->pr, ht->f);
	PR_PopFrame (ht->pr);
}

static void
bi_Hash_NewTable (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
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

static bi_hashtab_t * __attribute__((pure))
get_table (progs_t *pr, hash_resources_t *res, const char *name, int index)
{
	bi_hashtab_t *ht = table_get (res, index);

	if (!ht)
		PR_RunError (pr, "invalid hash table index passed to %s", name + 3);
	return ht;
}

static void
bi_Hash_SetHashCompare (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));
	uintptr_t   (*gh)(const void*,void*);
	int         (*cmp)(const void*,const void*,void*);

	ht->gh = P_FUNCTION (pr, 1);
	ht->cmp = P_FUNCTION (pr, 2);
	gh = ht->gh ? bi_get_hash : 0;
	cmp = ht->cmp ? bi_compare : 0;
	Hash_SetHashCompare (ht->tab, gh, cmp);
}

static void
bi_Hash_DelTable (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	Hash_DelTable (ht->tab);
	*ht->prev = ht->next;
	if (ht->next)
		ht->next->prev = ht->prev;
	table_free (res, ht);
}

static void
bi_Hash_FlushTable (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	Hash_FlushTable (ht->tab);
}

static void
bi_Hash_Add (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = Hash_Add (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_AddElement (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = Hash_AddElement (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_Find (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_Find (ht->tab, P_GSTRING (pr, 1));
}

static void
bi_Hash_FindElement (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_FindElement (ht->tab,
										  (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_FindList (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_FindList (ht->tab, P_GSTRING (pr, 1));
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		PR_PTR (ptr, &pr_list[count++]) = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_FindElementList (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_FindElementList (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		PR_PTR (ptr, &pr_list[count++]) = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_Del (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_Del (ht->tab, P_GSTRING (pr, 1));
}

static void
bi_Hash_DelElement (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = (intptr_t) Hash_DelElement (ht->tab,
										 (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_Free (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	Hash_Free (ht->tab, (void *) (intptr_t) P_INT (pr, 1));
}

static void
bi_Hash_String (progs_t *pr, void *_res)
{
	R_INT (pr) = Hash_String (P_GSTRING (pr, 0));
}

static void
bi_Hash_Buffer (progs_t *pr, void *_res)
{
	R_INT (pr) = Hash_Buffer (P_GPOINTER (pr, 0), P_INT (pr, 1));
}

static void
bi_Hash_GetList (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));
	void      **list, **l;
	pr_type_t  *pr_list;
	int         count;

	list = Hash_GetList (ht->tab);
	for (count = 1, l = list; *l; l++)
		count++;
	pr_list = PR_Zone_Malloc (pr, count * sizeof (pr_type_t));
	// the hash tables stores progs pointers...
	for (count = 0, l = list; *l; l++)
		PR_PTR(ptr, &pr_list[count++]) = (intptr_t) *l;
	free (list);
	RETURN_POINTER (pr, pr_list);
}

static void
bi_Hash_Stats (progs_t *pr, void *_res)
{
	__auto_type res = (hash_resources_t *) _res;
	bi_hashtab_t *ht = get_table (pr, res, __FUNCTION__, P_INT (pr, 0));

	Hash_Stats (ht->tab);
}

static void
bi_hash_clear (progs_t *pr, void *_res)
{
	hash_resources_t *res = (hash_resources_t *) _res;
	bi_hashtab_t *ht;

	for (ht = res->tabs; ht; ht = ht->next)
		Hash_DelTable (ht->tab);
	res->tabs = 0;
	table_reset (res);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(Hash_NewTable,        4, p(int), p(func), p(func), p(ptr)),
	bi(Hash_SetHashCompare,  3, p(ptr), p(func), p(func)),
	bi(Hash_DelTable,        1, p(ptr)),
	bi(Hash_FlushTable,      1, p(ptr)),
	bi(Hash_Add,             2, p(ptr), p(ptr)),
	bi(Hash_AddElement,      2, p(ptr), p(ptr)),
	bi(Hash_Find,            2, p(ptr), p(string)),
	bi(Hash_FindElement,     2, p(ptr), p(ptr)),
	bi(Hash_FindList,        2, p(ptr), p(string)),
	bi(Hash_FindElementList, 2, p(ptr), p(ptr)),
	bi(Hash_Del,             2, p(ptr), p(string)),
	bi(Hash_DelElement,      2, p(ptr), p(ptr)),
	bi(Hash_Free,            2, p(ptr), p(ptr)),
	bi(Hash_String,          1, p(string)),
	bi(Hash_Buffer,          2, p(ptr), p(int)),
	bi(Hash_GetList,         1, p(ptr)),
	bi(Hash_Stats,           1, p(ptr)),
	{0}
};

void
RUA_Hash_Init (progs_t *pr, int secure)
{
	hash_resources_t *res = calloc (1, sizeof (hash_resources_t));
	res->tabs = 0;

	PR_Resources_Register (pr, "Hash", res, bi_hash_clear);
	PR_RegisterBuiltins (pr, builtins, res);
}
