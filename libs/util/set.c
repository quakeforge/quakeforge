/*
	set.c

	Set manipulation.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/8/4

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

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/set.h"

#define BITS (sizeof (((set_t *) 0)->map[0]) * 8)

set_t      *free_sets;
set_iter_t *free_set_iters;

static set_iter_t *
new_setiter (void)
{
	set_iter_t *set_iter;
	ALLOC (16, set_iter_t, set_iters, set_iter);
	return set_iter;
}

static void
delete_setiter (set_iter_t *set_iter)
{
	FREE (set_iters, set_iter);
}

void
set_del_iter (set_iter_t *set_iter)
{
	delete_setiter (set_iter);
}

set_t *
set_new (void)
{
	set_t      *set;

	ALLOC (16, set_t, sets, set);
	set->size = sizeof (set->defmap) * 8;
	set->map = set->defmap;
	return set;
}

void
set_delete (set_t *set)
{
	if (set->map != set->defmap)
		free (set->map);
	FREE (sets, set);
}

static void
set_expand (set_t *set, unsigned x)
{
	unsigned   *map = set->map;
	size_t      size;

	if (x <= set->size)
		return;

	size = (x + BITS) & ~(BITS - 1);
	set->map = malloc (size / 8);
	memcpy (set->map, map, set->size / 8);
	memset (set->map + set->size / BITS, 0, (size - set->size) / 8);
	set->size = size;
	if (map != set->defmap)
		free (map);
}

static inline void
_set_add (set_t *set, unsigned x)
{
	if (x >= set->size)
		set_expand (set, x + 1);
	set->map[x / BITS] |= 1 << (x % BITS);
}

static inline void
_set_remove (set_t *set, unsigned x)
{
	if (x >= set->size)
		return;
	set->map[x / BITS] &= ~(1 << (x % BITS));
}

set_t *
set_add (set_t *set, unsigned x)
{
	if (set->inverted)
		_set_remove (set, x);
	else
		_set_add (set, x);
	return set;
}

set_t *
set_remove (set_t *set, unsigned x)
{
	if (set->inverted)
		_set_add (set, x);
	else
		_set_remove (set, x);
	return set;
}

set_t *
set_invert (set_t *set)
{
	set->inverted = !set->inverted;
	return set;
}

static set_t *
_set_union (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] |= src->map[i];
	return dst;
}

static set_t *
_set_intersection (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] &= src->map[i];
	for ( ; i < dst->size / BITS; i++)
		dst->map[i] = 0;
	return dst;
}

static set_t *
_set_difference (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] &= ~src->map[i];
	return dst;
}

static set_t *
_set_reverse_difference (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] = ~dst->map[i] & src->map[i];
	return dst;
}

set_t *
set_union (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		return _set_intersection (dst, src);
	} else if (src->inverted) {
		dst->inverted = 1;
		return _set_difference (dst, src);
	} else if (dst->inverted) {
		return _set_reverse_difference (dst, src);
	} else {
		return _set_union (dst, src);
	}
}

set_t *
set_intersection (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		return _set_union (dst, src);
	} else if (src->inverted) {
		return _set_difference (dst, src);
	} else if (dst->inverted) {
		dst->inverted = 0;
		return _set_reverse_difference (dst, src);
	} else {
		return _set_intersection (dst, src);
	}
}

set_t *
set_difference (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		dst->inverted = 0;
		return _set_reverse_difference (dst, src);
	} else if (src->inverted) {
		return _set_intersection (dst, src);
	} else if (dst->inverted) {
		return _set_union (dst, src);
	} else {
		return _set_difference (dst, src);
	}
}

set_t *
set_reverse_difference (set_t *dst, const set_t *src)
{
	if (dst->inverted && src->inverted) {
		dst->inverted = 0;
		return _set_difference (dst, src);
	} else if (src->inverted) {
		dst->inverted = 1;
		return _set_union (dst, src);
	} else if (dst->inverted) {
		dst->inverted = 0;
		return _set_intersection (dst, src);
	} else {
		return _set_reverse_difference (dst, src);
	}
}

set_t *
set_assign (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size);
	set_expand (dst, size);
	dst->inverted = src->inverted;
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] = src->map[i];
	for ( ; i < dst->size / BITS; i++)
		dst->map[i] = 0;
	return dst;
}

set_t *
set_empty (set_t *set)
{
	unsigned    i;

	set->inverted = 0;
	for (i = 0; i < set->size / BITS; i++)
		set->map[i] = 0;
	return set;
}

set_t *
set_everything (set_t *set)
{
	unsigned    i;

	set->inverted = 1;
	for (i = 0; i < set->size / BITS; i++)
		set->map[i] = 0;
	return set;
}

static inline int
_set_is_empty (const set_t *set)
{
	unsigned    i;

	for (i = 0; i < set->size / BITS; i++)
		if (set->map[i])
			return 0;
	return 1;
}

int
set_is_empty (const set_t *set)
{
	if (set->inverted)
		return 0;
	return _set_is_empty (set);
}

int
set_is_everything (const set_t *set)
{
	if (!set->inverted)
		return 0;
	return _set_is_empty (set);
}

typedef enum {
	set_equiv,
	set_intersecting,
	set_disjoint,
} set_test_e;

static int
set_test_intersect_n_n (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	unsigned    intersection = 0;
	set_test_e  rval = set_equiv;

	end = min (s1->size, s2->size) / BITS;
	for (i = 0; i < end; i++) {
		unsigned    m1 = s1->map[i];
		unsigned    m2 = s2->map[i];

		intersection |= m1 & m2;
		if (m1 != m2)
			rval = set_intersecting;
	}
	if (rval == set_intersecting && !intersection)
		rval = set_disjoint;
	return rval;
}

static int
set_test_intersect_n_i (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	unsigned    intersection = 0;
	set_test_e  rval = set_equiv;

	end = min (s1->size, s2->size) / BITS;
	for (i = 0; i < end; i++) {
		unsigned    m1 = s1->map[i];
		unsigned    m2 = ~s2->map[i];

		intersection |= m1 & m2;
		if (m1 != m2)
			rval = set_intersecting;
	}
	if (rval == set_intersecting && !intersection)
		rval = set_disjoint;
	return rval;
}

static int
set_test_intersect_i_n (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	unsigned    intersection = 0;
	set_test_e  rval = set_equiv;

	end = min (s1->size, s2->size) / BITS;
	for (i = 0; i < end; i++) {
		unsigned    m1 = ~s1->map[i];
		unsigned    m2 = s2->map[i];

		intersection |= m1 & m2;
		if (m1 != m2)
			rval = set_intersecting;
	}
	if (rval == set_intersecting && !intersection)
		rval = set_disjoint;
	return rval;
}

static int
set_test_intersect_i_i (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	unsigned    intersection = 0;
	set_test_e  rval = set_equiv;

	end = min (s1->size, s2->size) / BITS;
	for (i = 0; i < end; i++) {
		unsigned    m1 = ~s1->map[i];
		unsigned    m2 = ~s2->map[i];

		intersection |= m1 & m2;
		if (m1 != m2)
			rval = set_intersecting;
	}
	return rval;
}

static int
set_test (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_test_e  rval = set_equiv;
	set_test_e  ext = set_disjoint;

	if (s1->inverted && s2->inverted) {
		rval = set_test_intersect_i_i (s1, s2);
		ext = set_intersecting;
	} else if (s2->inverted) {
		rval = set_test_intersect_n_i (s1, s2);
		if (rval == set_equiv)
			rval = set_disjoint;
	} else if (s1->inverted) {
		rval = set_test_intersect_i_n (s1, s2);
		if (rval == set_equiv)
			rval = set_disjoint;
	} else {
		rval = set_test_intersect_n_n (s1, s2);
	}
	if (rval == set_equiv) {
		end = min (s1->size, s2->size) / BITS;
		for (i = end; i < s1->size / BITS; i++)
			if (s1->map[i])
				return ext;
		for (i = end; i < s2->size / BITS; i++)
			if (s2->map[i])
				return ext;
	}
	return rval;
}

int
set_is_disjoint (const set_t *s1, const set_t *s2)
{
	return set_test (s1, s2) == set_disjoint;
}

int
set_is_intersecting (const set_t *s1, const set_t *s2)
{
	return set_test (s1, s2) == set_intersecting;
}

int
set_is_equivalent (const set_t *s1, const set_t *s2)
{
	return set_test (s1, s2) == set_equiv;
}

int
set_is_subset (const set_t *set, const set_t *sub)
{
	unsigned    i, end;

	end = min (set->size, sub->size) / BITS;
	if (set->inverted && sub->inverted) {
		for (i = 0; i < end; i++) {
			if (~sub->map[i] & set->map[i])
				return 0;
		}
		for ( ; i < set->size / BITS; i++)
			if (set->map[i])
				return 0;
	} else if (set->inverted) {
		for (i = 0; i < end; i++) {
			if (sub->map[i] & set->map[i])
				return 0;
		}
	} else if (sub->inverted) {
		// an inverted set cannot be a subset of a set that is not inverted
		return 0;
	} else {
		for (i = 0; i < end; i++) {
			if (sub->map[i] & ~set->map[i])
				return 0;
		}
		for ( ; i < sub->size / BITS; i++)
			if (sub->map[i])
				return 0;
	}
	return 1;
}

static inline int
_set_is_member (const set_t *set, unsigned x)
{
	if (x >= set->size)
		return 0;
	return (set->map[x / BITS] & (1 << (x % BITS))) != 0;
}

int
set_is_member (const set_t *set, unsigned x)
{
	if (set->inverted)
		return !_set_is_member (set, x);
	return _set_is_member (set, x);
}

unsigned
set_size (const set_t *set)
{
	unsigned    count = 0;
	unsigned    i;

	for (i = 0; i < set->size; i++)
		if (_set_is_member (set, i))
			count++;
	return count;
}

set_iter_t *
set_first (const set_t *set)
{
	unsigned    x;
	set_iter_t *set_iter;

	for (x = 0; x < set->size; x++) {
		if (_set_is_member (set, x)) {
			set_iter = new_setiter ();
			set_iter->set = set;
			set_iter->value = x;
			return set_iter;
		}
	}
	return 0;
}

set_iter_t *
set_next (set_iter_t *set_iter)
{
	unsigned    x;

	for (x = set_iter->value + 1; x < set_iter->set->size; x++) {
		if (_set_is_member (set_iter->set, x)) {
			set_iter->value = x;
			return set_iter;
		}
	}
	delete_setiter (set_iter);
	return 0;
}

const char *
set_as_string (const set_t *set)
{
	static dstring_t *str;
	unsigned    i;

	if (!str)
		str = dstring_new ();
	dstring_clearstr (str);
	if (set_is_empty (set)) {
		dstring_copystr (str, "[empty]");
		return str->str;
	}
	if (set_is_everything (set)) {
		dstring_copystr (str, "[everything]");
		return str->str;
	}
	for (i = 0; i < set->size; i++) {
		if (set_is_member (set, i)) {
			if (str->str[0])
				dasprintf (str, " %d", i);
			else
				dsprintf (str, "%d", i);
		}
	}
	if (set->inverted)
		dasprintf (str, "%s%d ...", str->str[0] ? " " : "", i);
	return str->str;
}
