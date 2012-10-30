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

#include "QF/dstring.h"
#include "QF/mathlib.h"

#include "set.h"

#define BITS (sizeof (unsigned) * 8)

set_t *
set_new (void)
{
	set_t      *set;

	set = calloc (1, sizeof (set_t));
	set->size = sizeof (set->defmap) * 8;
	set->map = set->defmap;
	return set;
}

void
set_delete (set_t *set)
{
	if (set->map != set->defmap)
		free (set->map);
	free (set);
}

static void
set_expand (set_t *set, unsigned x)
{
	unsigned   *map = set->map;
	size_t      size;

	if (x < set->size)
		return;

	size = (x + BITS - 1) & ~(BITS - 1);
	set->map = malloc (size / 8);
	memcpy (set->map, map, set->size / 8);
	if (map != set->defmap)
		free (map);
}

void
set_add (set_t *set, unsigned x)
{
	if (x >= set->size)
		set_expand (set, x);
	set->map[x / BITS] |= 1 << (x % BITS);
}

void
set_remove (set_t *set, unsigned x)
{
	if (x >= set->size)
		return;
	set->map[x / BITS] &= ~(1 << (x % BITS));
}

set_t *
set_union (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size) - 1;
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] |= src->map[i];
	return dst;
}

set_t *
set_intersection (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size) - 1;
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] &= src->map[i];
	return dst;
}

set_t *
set_difference (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size) - 1;
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] &= ~src->map[i];
	return dst;
}

set_t *
set_assign (set_t *dst, const set_t *src)
{
	unsigned    size;
	unsigned    i;

	size = max (dst->size, src->size) - 1;
	set_expand (dst, size);
	for (i = 0; i < src->size / BITS; i++)
		dst->map[i] = src->map[i];
	return dst;
}

set_t *
set_empty (set_t *set)
{
	unsigned    i;

	for (i = 0; i < set->size / BITS; i++)
		set->map[i] = 0;
	return set;
}

int
set_is_empty (const set_t *set)
{
	unsigned    i;

	for (i = 0; i < set->size / BITS; i++)
		if (set->map[i])
			return 0;
	return 1;
}

typedef enum {
	set_equiv,
	set_intersecting,
	set_disjoint,
} set_test_e;

static int
set_test (const set_t *s1, const set_t *s2)
{
	unsigned    i, end;
	set_test_e  rval = set_equiv;

	end = min (s1->size, s2->size) / BITS;
	for (i = 0; i < end; i++) {
		if (s1->map[i] != s2->map[i]) {
			if (s1->map[i] & s2->map[i])
				return set_intersecting;
			else
				rval = set_disjoint;
		}
	}
	if (rval == set_equiv) {
		for (; i < s1->size / BITS; i++)
			if (s1->map[i])
				return set_disjoint;
		for (; i < s2->size / BITS; i++)
			if (s2->map[i])
				return set_disjoint;
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
	for (i = 0; i < end; i++) {
		if (sub->map[i] & ~set->map[i])
			return 0;
	}
	for (i = 0; i < sub->size / BITS; i++)
		if (sub->map[i])
			return 0;
	return 1;
}

int
set_is_member (const set_t *set, unsigned x)
{
	if (x >= set->size)
		return 0;
	return (set->map[x / BITS] & (1 << (x % BITS))) != 0;
}

unsigned
set_first (const set_t *set)
{
	unsigned    x;

	for (x = 0; x < set->size; x++)
		if (set_is_member (set, x))
			return x;
	return -1;	// FIXME error?
}

const char *
set_as_string (const set_t *set)
{
	static dstring_t *str;
	unsigned    i;

	if (!str)
		str = dstring_new ();
	dstring_clearstr (str);
	for (i = 0; i < set->size; i++) {
		if (set_is_member (set, i)) {
			if (str->str[0])
				dasprintf (str, " %d", i);
			else
				dsprintf (str, "%d", i);
		}
	}
	return str->str;
}
