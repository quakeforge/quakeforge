/*
	rua_set.c

	Ruamoko set api

	Copyright (C) 2012 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/12/15

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

#include "QF/progs.h"
#include "QF/pr_obj.h"
#include "QF/set.h"

#include "rua_internal.h"

typedef struct {
	pr_id_t     obj;
	pr_int_t    set;
} pr_set_t;

typedef struct {
	pr_id_t     obj;
	pr_int_t    iter;
} pr_set_iter_t;

typedef struct bi_set_s {
	struct bi_set_s *next;
	struct bi_set_s **prev;
	set_t      *set;
} bi_set_t;

typedef struct bi_set_iter_s {
	struct bi_set_iter_s *next;
	struct bi_set_iter_s **prev;
	set_iter_t *iter;
} bi_set_iter_t;

typedef struct {
	PR_RESMAP (bi_set_t) set_map;
	PR_RESMAP (bi_set_iter_t) set_iter_map;
	bi_set_t   *sets;
	bi_set_iter_t *set_iters;
} set_resources_t;

static bi_set_t *
res_set_new (set_resources_t *res)
{
	return PR_RESNEW (res->set_map);
}

static void
res_set_free (set_resources_t *res, bi_set_t *set)
{
	PR_RESFREE (res->set_map, set);
}

static void
res_set_reset (set_resources_t *res)
{
	PR_RESRESET (res->set_map);
}

static inline bi_set_t *
res_set_get (set_resources_t *res, int index)
{
	return PR_RESGET(res->set_map, index);
}

static inline int __attribute__((pure))
res_set_index (set_resources_t *res, bi_set_t *set)
{
	return PR_RESINDEX(res->set_map, set);
}

static bi_set_iter_t *
res_set_iter_new (set_resources_t *res)
{
	return PR_RESNEW (res->set_iter_map);
}

static void
res_set_iter_free (set_resources_t *res, bi_set_iter_t *set_iter)
{
	PR_RESFREE (res->set_iter_map, set_iter);
}

static void
res_set_iter_reset (set_resources_t *res)
{
	PR_RESRESET (res->set_iter_map);
}

static inline bi_set_iter_t *
res_set_iter_get (set_resources_t *res, int index)
{
	return PR_RESGET(res->set_iter_map, index);
}

static inline int __attribute__((pure))
res_set_iter_index (set_resources_t *res, bi_set_iter_t *set_iter)
{
	return PR_RESINDEX(res->set_iter_map, set_iter);
}

static bi_set_t *
get_set (progs_t *pr, const char *name, int index)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");
	bi_set_t   *set = res_set_get (res, index);

	if (!set)
		PR_RunError (pr, "invalid set index passed to %s", name + 3);
	return set;
}

static bi_set_iter_t *
get_set_iter (progs_t *pr, const char *name, int index)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");
	bi_set_iter_t   *set = res_set_iter_get (res, index);

	if (!set)
		PR_RunError (pr, "invalid set iterator index passed to %s", name + 3);
	return set;
}

static void
del_set_iter (progs_t *pr, bi_set_iter_t *set_iter)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");

	*set_iter->prev = set_iter->next;
	if (set_iter->next)
		set_iter->next->prev = set_iter->prev;
	res_set_iter_free (res, set_iter);
}

static void
bi_set_del_iter (progs_t *pr)
{
	bi_set_iter_t   *set_iter = get_set_iter (pr, __FUNCTION__, P_INT (pr, 0));

	set_del_iter (set_iter->iter);
	del_set_iter (pr, set_iter);
}

static void
bi_set_new (progs_t *pr)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");
	bi_set_t   *set;

	set = res_set_new (res);

	set->next = res->sets;
	set->prev = &res->sets;
	if (res->sets)
		res->sets->prev = &set->next;
	res->sets = set;

	set->set = set_new ();

	R_INT (pr) = res_set_index (res, set);
}

static void
bi_set_delete (progs_t *pr)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_delete (set->set);
	*set->prev = set->next;
	if (set->next)
		set->next->prev = set->prev;
	res_set_free (res, set);
}

static void
bi_set_add (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_add (set->set, P_UINT (pr, 1));
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_remove (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_remove (set->set, P_UINT (pr, 1));
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_invert (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_invert (set->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_union (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	set_union (set1->set, set2->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_intersection (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	set_intersection (set1->set, set2->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_difference (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	set_difference (set1->set, set2->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_reverse_difference (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	set_reverse_difference (set1->set, set2->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_assign (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	set_assign (set1->set, set2->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_empty (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_empty (set->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_everything (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	set_everything (set->set);
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_is_empty (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = set_is_empty (set->set);
}

static void
bi_set_is_everything (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = set_is_everything (set->set);
}

static void
bi_set_is_disjoint (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	R_INT (pr) = set_is_disjoint (set1->set, set2->set);
}

static void
bi_set_is_intersecting (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	R_INT (pr) = set_is_intersecting (set1->set, set2->set);
}

static void
bi_set_is_equivalent (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	R_INT (pr) = set_is_equivalent (set1->set, set2->set);
}

static void
bi_set_is_subset (progs_t *pr)
{
	bi_set_t   *set1 = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	bi_set_t   *set2 = get_set (pr, __FUNCTION__, P_INT (pr, 1));

	R_INT (pr) = set_is_subset (set1->set, set2->set);
}

static void
bi_set_is_member (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = set_is_member (set->set, P_UINT (pr, 1));
}

static void
bi_set_count (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	R_INT (pr) = set_count (set->set);
}

static void
bi_set_first (progs_t *pr)
{
	set_resources_t *res = PR_Resources_Find (pr, "Set");
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));
	set_iter_t *iter;
	bi_set_iter_t *set_iter;

	iter = set_first (set->set);
	if (!iter) {
		R_INT (pr) = 0;
		return;
	}
	set_iter = res_set_iter_new (res);

	set_iter->next = res->set_iters;
	set_iter->prev = &res->set_iters;
	if (res->set_iters)
		res->set_iters->prev = &set_iter->next;
	res->set_iters = set_iter;

	set_iter->iter = iter;

	R_INT (pr) = res_set_iter_index (res, set_iter);
}

static void
bi_set_next (progs_t *pr)
{
	bi_set_iter_t *set_iter = get_set_iter (pr, __FUNCTION__, P_INT (pr, 0));

	if (!set_next (set_iter->iter)) {
		del_set_iter (pr, set_iter);
		R_INT (pr) = 0;
		return;
	}
	R_INT (pr) = P_INT (pr, 0);
}

static void
bi_set_as_string (progs_t *pr)
{
	bi_set_t   *set = get_set (pr, __FUNCTION__, P_INT (pr, 0));

	RETURN_STRING (pr, set_as_string (set->set));
}

static void
bi_i_SetIterator__element (progs_t *pr)
{
	pr_set_iter_t *iter_obj = &P_STRUCT (pr, pr_set_iter_t, 0);
	bi_set_iter_t *set_iter = get_set_iter (pr, __FUNCTION__, iter_obj->iter);

	R_INT (pr) = set_iter->iter->element;
}

static void
bi_i_Set__add_ (progs_t *pr)
{
	pointer_t    set_ptr = P_POINTER (pr, 0);
	pr_set_t    *set_obj = &G_STRUCT (pr, pr_set_t, set_ptr);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	P_INT (pr, 1) = P_INT (pr, 2);
	bi_set_add (pr);
	R_INT (pr) = set_ptr;
}

static void
bi_i_Set__remove_ (progs_t *pr)
{
	pointer_t    set_ptr = P_POINTER (pr, 0);
	pr_set_t    *set_obj = &G_STRUCT (pr, pr_set_t, set_ptr);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	P_INT (pr, 1) = P_INT (pr, 2);
	bi_set_remove (pr);
	R_INT (pr) = set_ptr;
}

static void
bi_i_Set__invert (progs_t *pr)
{
	pointer_t    set_ptr = P_POINTER (pr, 0);
	pr_set_t    *set_obj = &G_STRUCT (pr, pr_set_t, set_ptr);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_invert (pr);
	R_INT (pr) = set_ptr;
}

static void
bi_i_Set__union_ (progs_t *pr)
{
	pointer_t    dst_ptr = P_POINTER (pr, 0);
	pr_set_t    *dst_obj = &G_STRUCT (pr, pr_set_t, dst_ptr);
	pr_set_t    *src_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = dst_obj->set;
	P_INT (pr, 1) = src_obj->set;
	bi_set_union (pr);
	R_INT (pr) = dst_ptr;
}

static void
bi_i_Set__intersection_ (progs_t *pr)
{
	pointer_t    dst_ptr = P_POINTER (pr, 0);
	pr_set_t    *dst_obj = &G_STRUCT (pr, pr_set_t, dst_ptr);
	pr_set_t    *src_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = dst_obj->set;
	P_INT (pr, 1) = src_obj->set;
	bi_set_intersection (pr);
	R_INT (pr) = dst_ptr;
}

static void
bi_i_Set__difference_ (progs_t *pr)
{
	pointer_t    dst_ptr = P_POINTER (pr, 0);
	pr_set_t    *dst_obj = &G_STRUCT (pr, pr_set_t, dst_ptr);
	pr_set_t    *src_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = dst_obj->set;
	P_INT (pr, 1) = src_obj->set;
	bi_set_difference (pr);
	R_INT (pr) = dst_ptr;
}

static void
bi_i_Set__reverse_difference_ (progs_t *pr)
{
	pointer_t    dst_ptr = P_POINTER (pr, 0);
	pr_set_t    *dst_obj = &G_STRUCT (pr, pr_set_t, dst_ptr);
	pr_set_t    *src_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = dst_obj->set;
	P_INT (pr, 1) = src_obj->set;
	bi_set_reverse_difference (pr);
	R_INT (pr) = dst_ptr;
}

static void
bi_i_Set__assign_ (progs_t *pr)
{
	pointer_t    dst_ptr = P_POINTER (pr, 0);
	pr_set_t    *dst_obj = &G_STRUCT (pr, pr_set_t, dst_ptr);
	pr_set_t    *src_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = dst_obj->set;
	P_INT (pr, 1) = src_obj->set;
	bi_set_assign (pr);
	R_INT (pr) = dst_ptr;
}

static void
bi_i_Set__empty (progs_t *pr)
{
	pointer_t    set_ptr = P_POINTER (pr, 0);
	pr_set_t    *set_obj = &G_STRUCT (pr, pr_set_t, set_ptr);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_empty (pr);
	R_INT (pr) = set_ptr;
}

static void
bi_i_Set__everything (progs_t *pr)
{
	pointer_t    set_ptr = P_POINTER (pr, 0);
	pr_set_t    *set_obj = &G_STRUCT (pr, pr_set_t, set_ptr);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_everything (pr);
	R_INT (pr) = set_ptr;
}

static void
bi_i_Set__is_empty (progs_t *pr)
{
	pr_set_t    *set_obj = &P_STRUCT (pr, pr_set_t, 0);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_is_empty (pr);
}

static void
bi_i_Set__is_everything (progs_t *pr)
{
	pr_set_t    *set_obj = &P_STRUCT (pr, pr_set_t, 0);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_is_everything (pr);
}

static void
bi_i_Set__is_disjoint_ (progs_t *pr)
{
	pr_set_t    *s1_obj = &P_STRUCT (pr, pr_set_t, 0);
	pr_set_t    *s2_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = s1_obj->set;
	P_INT (pr, 1) = s2_obj->set;
	bi_set_is_disjoint (pr);
}

static void
bi_i_Set__is_intersecting_ (progs_t *pr)
{
	pr_set_t    *s1_obj = &P_STRUCT (pr, pr_set_t, 0);
	pr_set_t    *s2_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = s1_obj->set;
	P_INT (pr, 1) = s2_obj->set;
	bi_set_is_intersecting (pr);
}

static void
bi_i_Set__is_equivalent_ (progs_t *pr)
{
	pr_set_t    *s1_obj = &P_STRUCT (pr, pr_set_t, 0);
	pr_set_t    *s2_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = s1_obj->set;
	P_INT (pr, 1) = s2_obj->set;
	bi_set_is_equivalent (pr);
}

static void
bi_i_Set__is_subset_ (progs_t *pr)
{
	pr_set_t    *s1_obj = &P_STRUCT (pr, pr_set_t, 0);
	pr_set_t    *s2_obj = &P_STRUCT (pr, pr_set_t, 2);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = s1_obj->set;
	P_INT (pr, 1) = s2_obj->set;
	bi_set_is_subset (pr);
}

static void
bi_i_Set__is_member_ (progs_t *pr)
{
	pr_set_t    *set_obj = &P_STRUCT (pr, pr_set_t, 0);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	P_UINT (pr, 1) = P_UINT (pr, 2);
	bi_set_is_member (pr);
}

static void
bi_i_Set__size (progs_t *pr)
{
	pr_set_t    *set_obj = &P_STRUCT (pr, pr_set_t, 0);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_count (pr);
}

static void
bi_i_Set__as_string (progs_t *pr)
{
	pr_set_t    *set_obj = &P_STRUCT (pr, pr_set_t, 0);

	PR_RESET_PARAMS (pr);
	P_INT (pr, 0) = set_obj->set;
	bi_set_as_string (pr);
}

static void
res_set_clear (progs_t *pr, void *data)
{
	set_resources_t *res = (set_resources_t *) data;
	bi_set_t *set;
	bi_set_iter_t *set_iter;

	for (set = res->sets; set; set = set->next)
		set_delete (set->set);
	for (set_iter = res->set_iters; set_iter; set_iter = set_iter->next)
		set_del_iter (set_iter->iter);
	res->sets = 0;
	res->set_iters = 0;
	res_set_reset (res);
	res_set_iter_reset (res);
}

static builtin_t builtins[] = {
	{"set_del_iter",			bi_set_del_iter,			-1},
	{"set_new",					bi_set_new,					-1},
	{"set_delete",				bi_set_delete,				-1},
	{"set_add",					bi_set_add,					-1},
	{"set_remove",				bi_set_remove,				-1},
	{"set_invert",				bi_set_invert,				-1},
	{"set_union",				bi_set_union,				-1},
	{"set_intersection",		bi_set_intersection,		-1},
	{"set_difference",			bi_set_difference,			-1},
	{"set_reverse_difference",	bi_set_reverse_difference,	-1},
	{"set_assign",				bi_set_assign,				-1},
	{"set_empty",				bi_set_empty,				-1},
	{"set_everything",			bi_set_everything,			-1},
	{"set_is_empty",			bi_set_is_empty,			-1},
	{"set_is_everything",		bi_set_is_everything,		-1},
	{"set_is_disjoint",			bi_set_is_disjoint,			-1},
	{"set_is_intersecting",		bi_set_is_intersecting,		-1},
	{"set_is_equivalent",		bi_set_is_equivalent,		-1},
	{"set_is_subset",			bi_set_is_subset,			-1},
	{"set_is_member",			bi_set_is_member,			-1},
	{"set_count",				bi_set_count,				-1},
	{"set_first",				bi_set_first,				-1},
	{"set_next",				bi_set_next,				-1},
	{"set_as_string",			bi_set_as_string,			-1},

	{"_i_SetIterator__element",	bi_i_SetIterator__element,	-1},

	{"_i_Set__add_",				bi_i_Set__add_,					-1},
	{"_i_Set__remove_",				bi_i_Set__remove_,				-1},
	{"_i_Set__invert",				bi_i_Set__invert,				-1},
	{"_i_Set__union_",				bi_i_Set__union_,				-1},
	{"_i_Set__intersection_",		bi_i_Set__intersection_,		-1},
	{"_i_Set__difference_",			bi_i_Set__difference_,			-1},
	{"_i_Set__reverse_difference_",	bi_i_Set__reverse_difference_,	-1},
	{"_i_Set__assign_",				bi_i_Set__assign_,				-1},
	{"_i_Set__empty",				bi_i_Set__empty,				-1},
	{"_i_Set__everything",			bi_i_Set__everything,			-1},
	{"_i_Set__is_empty",			bi_i_Set__is_empty,				-1},
	{"_i_Set__is_everything",		bi_i_Set__is_everything,		-1},
	{"_i_Set__is_disjoint_",		bi_i_Set__is_disjoint_,			-1},
	{"_i_Set__is_intersecting_",	bi_i_Set__is_intersecting_,		-1},
	{"_i_Set__is_equivalent_",		bi_i_Set__is_equivalent_,		-1},
	{"_i_Set__is_subset_",			bi_i_Set__is_subset_,			-1},
	{"_i_Set__is_member_",			bi_i_Set__is_member_,			-1},
	{"_i_Set__size",				bi_i_Set__size,					-1},
	{"_i_Set__as_string",			bi_i_Set__as_string,			-1},

	{0}
};

void
RUA_Set_Init (progs_t *pr, int secure)
{
	set_resources_t *res = calloc (1, sizeof (set_resources_t));
	res->sets = 0;

	PR_Resources_Register (pr, "Set", res, res_set_clear);
	PR_RegisterBuiltins (pr, builtins);
}
